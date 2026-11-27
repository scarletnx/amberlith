// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// VitaSDK's newlib formats no C99 size modifiers: its newlib.h has _WANT_IO_C99_FORMATS undefined,
// so %zu, %jd and %td print the letters and leave the arguments out of step, which the next %s
// then reads as a pointer. The engine prints sizes
// with %zu everywhere, so the Vita's build wraps its printf family (vita.mk, --wrap): each format
// has z made l (size_t is an unsigned long's width there), j made ll (intmax_t a long long's) and
// t taken out (ptrdiff_t an int's), and the library prints the rest as it does.

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int __real_vsnprintf(char *out, size_t size, const char *format, va_list args);
int __real_vfprintf(FILE *stream, const char *format, va_list args);
int __wrap_vsnprintf(char *out, size_t size, const char *format, va_list args);
int __wrap_vfprintf(FILE *stream, const char *format, va_list args);
int __wrap_snprintf(char *out, size_t size, const char *format, ...);
int __wrap_fprintf(FILE *stream, const char *format, ...);
int __wrap_printf(const char *format, ...);

static_assert(sizeof(size_t) == sizeof(unsigned long), "z is printed as l");
static_assert(sizeof(long long) == 8 && sizeof(ptrdiff_t) == sizeof(int), "j as ll, t as nothing");

/// @p format with each conversion's z, j and t modifiers as the library takes them, into @p out
/// (@p size bytes): nullptr when it does not fit, and the caller makes room.
static const char *Translate(const char *format, char *out, size_t size) {
    size_t used = 0;
    for (const char *p = format; *p != '\0'; ++p) {
        if (used + 3u >= size) {
            return nullptr;
        }
        out[used++] = *p;
        if (*p != '%') {
            continue;
        }
        // flags, width and precision, then the length modifier
        for (++p; *p != '\0' && strchr("-+ #0123456789.*'", *p) != nullptr; ++p) {
            if (used + 3u >= size) {
                return nullptr;
            }
            out[used++] = *p;
        }
        if (*p == '\0') {
            break;
        }
        if (*p == 'z') {
            out[used++] = 'l';
        } else if (*p == 'j') {
            out[used++] = 'l';
            out[used++] = 'l';
        } else if (*p != 't') {
            out[used++] = *p;
        }
    }
    out[used] = '\0';
    return out;
}

/// The library's call with @p format translated, on the stack when it fits and else on the heap:
/// -1, as a failed printf, when the heap has no room for it.
#define TRANSLATED(call)                                                                            \
    char small[512];                                                                                \
    const char *said = Translate(format, small, sizeof(small));                                    \
    char *big = nullptr;                                                                               \
    if (said == nullptr) {                                                                             \
        const size_t need = strlen(format) * 2u + 4u;                                               \
        big = malloc(need);                                                                         \
        if (big == nullptr) {                                                                          \
            return -1;                                                                              \
        }                                                                                           \
        said = Translate(format, big, need);                                                        \
    }                                                                                               \
    const int r = (call);                                                                           \
    free(big);                                                                                      \
    return r

int __wrap_vsnprintf(char *out, size_t size, const char *format, va_list args) {
    TRANSLATED(__real_vsnprintf(out, size, said, args));
}
int __wrap_vfprintf(FILE *stream, const char *format, va_list args) {
    TRANSLATED(__real_vfprintf(stream, said, args));
}
int __wrap_snprintf(char *out, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int r = __wrap_vsnprintf(out, size, format, args);
    va_end(args);
    return r;
}
int __wrap_fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int r = __wrap_vfprintf(stream, format, args);
    va_end(args);
    return r;
}
int __wrap_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int r = __wrap_vfprintf(stdout, format, args);
    va_end(args);
    return r;
}
