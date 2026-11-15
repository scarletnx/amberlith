// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Writes each of the demos' tracks (source/music.h) as a WAV to listen to, one pass of its form,
// and one more of a switch between two of them across the silence barrier, the samples the
// music family holds a machine to: `make -f host.mk music-wav MUSIC_WAV=folder`.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "music.h"

static void Put32(unsigned char *at, uint32_t v) {
    for (unsigned i = 0; i < 4u; ++i) {
        at[i] = (unsigned char)(v >> (8u * i));
    }
}

static int Write(const char *folder, const char *name, const int16_t *samples, uint64_t frames) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s.wav", folder, name);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "music-wav: %s could not be opened: %s\n", path, strerror(errno));
        return 1;
    }
    const uint32_t bytes = (uint32_t)(frames * 4u);
    unsigned char header[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E',
                                 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0 };
    Put32(header + 4, 36u + bytes);
    Put32(header + 24, MUSIC_RATE);
    Put32(header + 28, MUSIC_RATE * 4u);
    header[32] = 4;
    header[34] = 16;
    memcpy(header + 36, "data", 4);
    Put32(header + 40, bytes);
    unsigned char *data = malloc(bytes);
    if (data == NULL) {
        fclose(f);
        fprintf(stderr, "music-wav: no memory for %u bytes\n", (unsigned)bytes);
        return 1;
    }
    for (uint64_t i = 0; i < frames * 2u; ++i) {
        const uint16_t v = (uint16_t)samples[i];
        data[2u * i] = (unsigned char)(v & 0xFFu);
        data[2u * i + 1u] = (unsigned char)(v >> 8);
    }
    const int bad = fwrite(header, 1, sizeof(header), f) != sizeof(header) ||
                    fwrite(data, 1, bytes, f) != bytes;
    free(data);
    if (fclose(f) != 0 || bad) {
        fprintf(stderr, "music-wav: %s was not written whole\n", path);
        return 1;
    }
    printf("%s: %llu frames, %.1f s, hash %016llx\n", path, (unsigned long long)frames,
           (double)frames / MUSIC_RATE, (unsigned long long)MusicHash(MUSIC_HASH_START, samples,
                                                                       (size_t)(frames * 2u)));
    return 0;
}

static MusicPlayer player;

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: music-wav FOLDER\n");
        return 2;
    }
    int failed = 0;
    for (unsigned t = 0; t < MUSIC_TRACKS; ++t) {
        MusicStart(&player, (MusicTrack)t);
        const uint64_t frames = MusicFormFrames(&player.song);
        int16_t *samples = malloc((size_t)frames * 4u);
        if (samples == NULL) {
            fprintf(stderr, "music-wav: no memory for %llu frames\n", (unsigned long long)frames);
            return 1;
        }
        MusicRender(&player, samples, (size_t)frames);
        if (player.clipped != 0u) {
            fprintf(stderr, "music-wav: %s clipped %llu samples\n", MusicColourOf(t)->name,
                    (unsigned long long)player.clipped);
        }
        failed |= Write(argv[1], MusicColourOf(t)->name, samples, frames);
        free(samples);
    }
    // Eight seconds of the browser's track, a switch, and Wimsy's first eight seconds after the
    // fade and the barrier.
    const uint64_t before = 8u * MUSIC_RATE;
    const uint64_t after = MUSIC_FADE_FRAMES + MUSIC_BARRIER_FRAMES + 8u * MUSIC_RATE;
    int16_t *samples = malloc((size_t)(before + after) * 4u);
    if (samples == NULL) {
        fprintf(stderr, "music-wav: no memory for the switch\n");
        return 1;
    }
    MusicStart(&player, MUSIC_TRACK_MENU);
    MusicRender(&player, samples, (size_t)before);
    MusicSwitch(&player, MUSIC_TRACK_WIMSY);
    MusicRender(&player, samples + 2u * before, (size_t)after);
    failed |= Write(argv[1], "switch-menu-wimsy", samples, before + after);
    free(samples);
    return failed;
}
