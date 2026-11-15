// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "demo.h"
#include "report.h"
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The browser's part that is no screen's (docs/UI.md): the catalogue of cases it lists, read
/// from the reports a card holds, and which steps a pick runs. Nothing here runs in a run from the
/// mode file.

/// A demo the browser lists, one line of the frontend's table as a step is one of its STEPS: its
/// row's name, up to 24 characters; the suite its lines report under; a sentence or two for the
/// box beside the list; the family of its step (MODE_FAMILY_*); and its run (demo.h). Demos of one
/// suite in consecutive lines are the levels its pause menu walks. Its music is BrowseMusicOf's.
typedef struct {
    const char *name;
    const char *suite;
    const char *description;
    uint32_t family;
    void (*run)(const DemoRun *run);
} BrowseDemo;

/// The music a demo plays in the browser: its track's name (music.h's MusicTrackNamed), NULL for
/// none, and whether the demo streams it on its own audio renderer (MusicStreamOpen and
/// MusicStreamFeed), as the game scene's audio thread steps one, rather than through the
/// browser's player, which would open a second.
typedef struct {
    const char *track;
    bool streams;
} BrowseMusic;

/// The music of the demo whose lines report under @p suite.
BrowseMusic BrowseMusicOf(const char *suite);

/// The track the browser plays between runs.
#define BROWSE_MENU_TRACK "menu"

/// The cases the browser lists: the last report's of this build on the card, then each of this
/// session's runs over them. One allocation, grown as cases arrive.
typedef struct {
    ReportRecord *records;
    uint32_t count;
    uint32_t capacity;
} BrowseCatalog;

/// Reads one line of a report into @p out when it is a case's, unescaped as a reader of the line
/// gets it back: its suite, name, tier, result (a skip whose reason is crashed as crashed) and
/// detail. False for any other line, and for one that does not parse.
bool BrowseParseCaseLine(const char *line, ReportRecord *out);

/// Adds every case of the report at @p path, a case the catalogue holds already in place of it.
/// Hands back how many lines were cases; nought when there is no such file.
uint32_t BrowseCatalogRead(BrowseCatalog *catalog, const char *path);

/// Adds @p count kept cases, a case the catalogue holds already (its suite and name) in place.
void BrowseCatalogMerge(BrowseCatalog *catalog, const ReportRecord *records, uint32_t count);

void BrowseCatalogFree(BrowseCatalog *catalog);

/// Whether the step named @p step runs in a run of @p pick: the judge's always, a step whose
/// suite is picked, whole or a case of it, and the net and service steps when a suite of their
/// family is.
bool BrowseStepRuns(const char *step, const UiPick *pick, const UiBrowseInfo *info);

/// The levels of suite @p suite a pick asks for, bit n for level n: all @p levels of them for the
/// suite whole, else those a picked case names as "level <n>, ..." or "level <n>: ...".
uint32_t BrowseLevels(const UiPick *pick, const UiBrowseInfo *info, const char *suite,
                      uint32_t levels);

/// One sentence of what @p pick holds for the run's mode line: "clear, depth whole; coverage: 2
/// cases; demos: Wimsy level 3". The length written.
size_t BrowseDescribePick(char *out, size_t out_size, const UiPick *pick,
                          const UiBrowseInfo *info);

#ifdef __cplusplus
}
#endif
