// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>
#include <sys/syslimits.h>
#include <switch.h>

#include "sdk.h"
#include "sdk_card.h"

// The C library's sdmc: device over nn::fs (sdk_card.h): each call of the device is the SDK's
// function for it, on the card mounted as SDK_CARD. It asks nothing of libnx's own start, so a
// start that is not libnx's adds it the same way.

#define CARD SDK_CARD

/// The longest path nn::fs takes, with the mount name before it.
#define CARD_PATH_MAX (sizeof(CARD) + 0x301u)

typedef struct {
    NnFsFileHandle handle;
    int64_t position;
    bool append;
    bool writes;
} CardFile;

typedef struct {
    NnFsDirectoryHandle handle;
    char path[CARD_PATH_MAX];
} CardDirectory;

/// @p path as nn::fs takes it: CARD's own paths as they are, one with no device under CARD.
static bool FullPath(char out[CARD_PATH_MAX], const char *path) {
    const char *const colon = strchr(path, ':');
    const int wrote = colon != NULL ? snprintf(out, CARD_PATH_MAX, "%s", path)
                                    : snprintf(out, CARD_PATH_MAX, CARD ":%s%s",
                                               path[0] == '/' ? "" : "/", path);
    return wrote > 0 && (size_t)wrote < CARD_PATH_MAX;
}

/// The errno an fs result stands for.
static int ErrnoOf(uint32_t rc) {
    if (R_MODULE(rc) != NN_FS_MODULE) {
        return EIO;
    }
    const uint32_t description = R_DESCRIPTION(rc);
    return description == 1u                         ? ENOENT
           : description == 2u                       ? EEXIST
           : description == 7u                       ? EBUSY
           : description == 8u                       ? ENOTEMPTY
           : description >= 30u && description <= 45u ? ENOSPC
                                                      : EIO;
}

/// Sets errno from @p rc and gives what the device call returns on a failure.
static int Fail(struct _reent *r, uint32_t rc) {
    r->_errno = ErrnoOf(rc);
    return -1;
}

static int CardOpen(struct _reent *r, void *file_struct, const char *path, int flags, int mode) {
    (void)mode;
    CardFile *const f = (CardFile *)file_struct;
    memset(f, 0, sizeof(*f));
    char full[CARD_PATH_MAX];
    if (!FullPath(full, path)) {
        r->_errno = ENAMETOOLONG;
        return -1;
    }
    const int access = flags & O_ACCMODE;
    f->writes = access != O_RDONLY;
    f->append = (flags & O_APPEND) != 0;
    // A write past the end needs a file opened to append, so every writer is.
    const int open_mode = access == O_RDONLY   ? NN_FS_OPEN_READ
                          : access == O_WRONLY ? NN_FS_OPEN_WRITE | NN_FS_OPEN_APPEND
                                               : NN_FS_OPEN_READ | NN_FS_OPEN_WRITE |
                                                     NN_FS_OPEN_APPEND;
    if ((flags & O_CREAT) != 0) {
        int32_t type = -1;
        const uint32_t found = NnFsGetEntryType(&type, full);
        if (R_SUCCEEDED(found) && (flags & O_EXCL) != 0) {
            r->_errno = EEXIST;
            return -1;
        }
        if (R_FAILED(found)) {
            const uint32_t made = NnFsCreateFile(full, 0);
            if (R_FAILED(made)) {
                return Fail(r, made);
            }
        }
    }
    uint32_t rc = NnFsOpenFile(&f->handle, full, open_mode);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    if ((flags & O_TRUNC) != 0 && f->writes) {
        rc = NnFsSetFileSize(f->handle, 0);
        if (R_FAILED(rc)) {
            NnFsCloseFile(f->handle);
            return Fail(r, rc);
        }
    }
    return 0;
}

static int CardClose(struct _reent *r, void *fd) {
    CardFile *const f = (CardFile *)fd;
    // A file written to is flushed before it is closed, which the SDK requires of a writer.
    const uint32_t rc = f->writes ? NnFsFlushFile(f->handle) : 0u;
    NnFsCloseFile(f->handle);
    return R_FAILED(rc) ? Fail(r, rc) : 0;
}

static ssize_t CardRead(struct _reent *r, void *fd, char *ptr, size_t len) {
    CardFile *const f = (CardFile *)fd;
    int64_t size = 0;
    uint32_t rc = NnFsGetFileSize(&size, f->handle);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    // A read that starts past the end is refused by the SDK; one at the end reads nothing.
    if (f->position >= size) {
        return 0;
    }
    size_t read = 0;
    rc = NnFsReadFile(&read, f->handle, f->position, ptr, len);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    f->position += (int64_t)read;
    return (ssize_t)read;
}

static ssize_t CardWrite(struct _reent *r, void *fd, const char *ptr, size_t len) {
    CardFile *const f = (CardFile *)fd;
    int64_t size = 0;
    uint32_t rc = NnFsGetFileSize(&size, f->handle);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    if (f->append) {
        f->position = size;
    } else if (f->position > size) {
        // The gap a seek past the end leaves reads as noughts, as POSIX has it.
        rc = NnFsSetFileSize(f->handle, f->position);
        if (R_FAILED(rc)) {
            return Fail(r, rc);
        }
    }
    // Each write flushed: a journal line has to be on the card before the next case can end the
    // program.
    const NnFsWriteOption flush = { NN_FS_WRITE_FLUSH };
    rc = NnFsWriteFile(f->handle, f->position, ptr, len, &flush);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    f->position += (int64_t)len;
    return (ssize_t)len;
}

static off_t CardSeek(struct _reent *r, void *fd, off_t pos, int dir) {
    CardFile *const f = (CardFile *)fd;
    int64_t base = 0;
    if (dir == SEEK_CUR) {
        base = f->position;
    } else if (dir == SEEK_END) {
        const uint32_t rc = NnFsGetFileSize(&base, f->handle);
        if (R_FAILED(rc)) {
            return Fail(r, rc);
        }
    } else if (dir != SEEK_SET) {
        r->_errno = EINVAL;
        return -1;
    }
    if (base + (int64_t)pos < 0) {
        r->_errno = EINVAL;
        return -1;
    }
    f->position = base + (int64_t)pos;
    return (off_t)f->position;
}

static int CardFstat(struct _reent *r, void *fd, struct stat *st) {
    CardFile *const f = (CardFile *)fd;
    int64_t size = 0;
    const uint32_t rc = NnFsGetFileSize(&size, f->handle);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFREG | 0666;
    st->st_size = (off_t)size;
    st->st_nlink = 1;
    return 0;
}

static int CardStat(struct _reent *r, const char *path, struct stat *st) {
    char full[CARD_PATH_MAX];
    if (!FullPath(full, path)) {
        r->_errno = ENAMETOOLONG;
        return -1;
    }
    int32_t type = -1;
    uint32_t rc = NnFsGetEntryType(&type, full);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    memset(st, 0, sizeof(*st));
    st->st_nlink = 1;
    if (type == NN_FS_ENTRY_DIRECTORY) {
        st->st_mode = S_IFDIR | 0777;
        return 0;
    }
    NnFsFileHandle file = { 0 };
    rc = NnFsOpenFile(&file, full, NN_FS_OPEN_READ);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    int64_t size = 0;
    rc = NnFsGetFileSize(&size, file);
    NnFsCloseFile(file);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    st->st_mode = S_IFREG | 0666;
    st->st_size = (off_t)size;
    return 0;
}

/// A call of @p call on @p path made full: 0, or -1 with errno.
static int OnPath(struct _reent *r, const char *path, uint32_t (*call)(const char *)) {
    char full[CARD_PATH_MAX];
    if (!FullPath(full, path)) {
        r->_errno = ENAMETOOLONG;
        return -1;
    }
    const uint32_t rc = call(full);
    return R_FAILED(rc) ? Fail(r, rc) : 0;
}

static int CardUnlink(struct _reent *r, const char *name) {
    return OnPath(r, name, NnFsDeleteFile);
}

static int CardRmdir(struct _reent *r, const char *name) {
    return OnPath(r, name, NnFsDeleteDirectory);
}

static int CardMkdir(struct _reent *r, const char *path, int mode) {
    (void)mode;
    return OnPath(r, path, NnFsCreateDirectory);
}

static int CardRename(struct _reent *r, const char *old_name, const char *new_name) {
    char from[CARD_PATH_MAX];
    char to[CARD_PATH_MAX];
    if (!FullPath(from, old_name) || !FullPath(to, new_name)) {
        r->_errno = ENAMETOOLONG;
        return -1;
    }
    int32_t type = -1;
    uint32_t rc = NnFsGetEntryType(&type, from);
    if (R_SUCCEEDED(rc)) {
        rc = type == NN_FS_ENTRY_DIRECTORY ? NnFsRenameDirectory(from, to)
                                           : NnFsRenameFile(from, to);
    }
    return R_FAILED(rc) ? Fail(r, rc) : 0;
}

static int CardFtruncate(struct _reent *r, void *fd, off_t len) {
    CardFile *const f = (CardFile *)fd;
    const uint32_t rc = NnFsSetFileSize(f->handle, (int64_t)len);
    return R_FAILED(rc) ? Fail(r, rc) : 0;
}

static int CardFsync(struct _reent *r, void *fd) {
    CardFile *const f = (CardFile *)fd;
    const uint32_t rc = NnFsFlushFile(f->handle);
    return R_FAILED(rc) ? Fail(r, rc) : 0;
}

static DIR_ITER *CardDiropen(struct _reent *r, DIR_ITER *dir_state, const char *path) {
    CardDirectory *const d = (CardDirectory *)dir_state->dirStruct;
    memset(d, 0, sizeof(*d));
    if (!FullPath(d->path, path)) {
        r->_errno = ENAMETOOLONG;
        return NULL;
    }
    const uint32_t rc = NnFsOpenDirectory(&d->handle, d->path,
                                          NN_FS_DIRECTORY_DIRECTORIES | NN_FS_DIRECTORY_FILES);
    if (R_FAILED(rc)) {
        Fail(r, rc);
        return NULL;
    }
    return dir_state;
}

static int CardDirreset(struct _reent *r, DIR_ITER *dir_state) {
    CardDirectory *const d = (CardDirectory *)dir_state->dirStruct;
    NnFsCloseDirectory(d->handle);
    const uint32_t rc = NnFsOpenDirectory(&d->handle, d->path,
                                          NN_FS_DIRECTORY_DIRECTORIES | NN_FS_DIRECTORY_FILES);
    return R_FAILED(rc) ? Fail(r, rc) : 0;
}

static int CardDirnext(struct _reent *r, DIR_ITER *dir_state, char *filename,
                       struct stat *filestat) {
    CardDirectory *const d = (CardDirectory *)dir_state->dirStruct;
    static NnFsDirectoryEntry entry;
    int64_t count = 0;
    const uint32_t rc = NnFsReadDirectory(&count, &entry, d->handle, 1);
    if (R_FAILED(rc)) {
        return Fail(r, rc);
    }
    if (count == 0) {
        r->_errno = ENOENT;
        return -1;
    }
    const size_t length = strnlen(entry.name, sizeof(entry.name));
    if (length > NAME_MAX) {
        r->_errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(filename, entry.name, length);
    filename[length] = '\0';
    if (filestat != NULL) {
        memset(filestat, 0, sizeof(*filestat));
        filestat->st_mode =
            entry.type == FsDirEntryType_Dir ? (S_IFDIR | 0777) : (S_IFREG | 0666);
        filestat->st_size = (off_t)entry.file_size;
        filestat->st_nlink = 1;
    }
    return 0;
}

static int CardDirclose(struct _reent *r, DIR_ITER *dir_state) {
    (void)r;
    CardDirectory *const d = (CardDirectory *)dir_state->dirStruct;
    NnFsCloseDirectory(d->handle);
    return 0;
}

static const devoptab_t CARD_DEVICE = {
    .name = CARD,
    .structSize = sizeof(CardFile),
    .open_r = CardOpen,
    .close_r = CardClose,
    .write_r = CardWrite,
    .read_r = CardRead,
    .seek_r = CardSeek,
    .fstat_r = CardFstat,
    .stat_r = CardStat,
    .unlink_r = CardUnlink,
    .rename_r = CardRename,
    .mkdir_r = CardMkdir,
    .dirStateSize = sizeof(CardDirectory),
    .diropen_r = CardDiropen,
    .dirreset_r = CardDirreset,
    .dirnext_r = CardDirnext,
    .dirclose_r = CardDirclose,
    .ftruncate_r = CardFtruncate,
    .fsync_r = CardFsync,
    .rmdir_r = CardRmdir,
    .lstat_r = CardStat,
};

const devoptab_t *SdkCardDevice(void) {
    return &CARD_DEVICE;
}
