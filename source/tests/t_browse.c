// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../browse.h"
#include "../check.h"
#include "../mode.h"
#include "../music.h"
#include "../report.h"
#include "../ui.h"
#include "tests.h"

// The browser (docs/UI.md): the catalogue it reads from a report, what a pick runs, the walk of
// its tabs, the HUD and the pause menu over a demo, and that a run from the mode file never opens
// it. Part of the support suite, since nothing here asks the hardware for anything.

static const char *const SUITE = "support";

#define FRAME_WIDTH 1280u
#define FRAME_HEIGHT 720u

static const uint8_t PAGE[3] = { 0xDA, 0xD6, 0xBE };
static const uint8_t RUN_PAGE[3] = { 0x14, 0x14, 0x16 };

/// A small catalogue, as a report's case lines give one: the coverage suite's bring-up and three
/// judged cases, one of clear, two of the net family's dns, and a level of the Wimsy suite.
static void Catalogue(BrowseCatalog *catalog) {
    static const struct {
        const char *suite;
        const char *name;
        const char *tier;
        ReportResult result;
    } CASES[] = {
        { "coverage", "graphics device opens with its targets", "none", REPORT_PASS },
        { "coverage", "the shaders load", "none", REPORT_PASS },
        { "coverage", "four raster samples write 0xf where a pixel is covered", "exact",
          REPORT_FAIL },
        { "coverage", "the sample mask is in the mask written", "exact", REPORT_FAIL },
        { "coverage", "alpha to coverage at nought covers no sample", "exact", REPORT_PASS },
        { "clear", "clear to opaque red", "exact", REPORT_PASS },
        { "dns", "a name resolves", "exact", REPORT_PASS },
        { "dns", "a missing name says so", "exact", REPORT_SKIP },
        { "wimsy", "level 3, frame 149 of 600 is what the processor works out", "position",
          REPORT_PASS },
    };
    static ReportRecord records[sizeof(CASES) / sizeof(CASES[0])];
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); ++i) {
        ReportRecordFill(&records[i], CASES[i].suite, CASES[i].name, CASES[i].tier,
                         CASES[i].result, "", -1);
    }
    BrowseCatalogMerge(catalog, records, (uint32_t)(sizeof(CASES) / sizeof(CASES[0])));
}

static uint32_t CatalogueIndex(const BrowseCatalog *catalog, const char *suite,
                               const char *name) {
    for (uint32_t i = 0; i < catalog->count; ++i) {
        if (strcmp(catalog->records[i].suite, suite) == 0 &&
            strcmp(catalog->records[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

static void Mark(uint8_t *bits, uint32_t index) {
    bits[index / 8u] = (uint8_t)(bits[index / 8u] | (1u << (index % 8u)));
}

static void CheckSuites(void) {
    // Every suite once, families in their order, the judge's first.
    uint32_t backwards = 0;
    uint32_t twice = 0;
    const uint32_t count = UiBrowseSuiteCount();
    for (uint32_t i = 0; i < count; ++i) {
        if (i > 0u && UiBrowseSuiteFamily(i) < UiBrowseSuiteFamily(i - 1u)) {
            ++backwards;
        }
        twice += UiBrowseSuiteIndex(UiBrowseSuite(i)) != i ? 1u : 0u;
        twice += UiFamilyOf(UiBrowseSuite(i)) != UiBrowseSuiteFamily(i) ? 1u : 0u;
    }
    CHECK_TRUE(SUITE, "the browser lists every suite the screens know once, under its family",
               CHECK_NONE,
               count > 100u && backwards == 0u && twice == 0u &&
                   UiBrowseSuiteFamily(0) == UI_FAMILY_JUDGE &&
                   UiBrowseSuiteIndex("coverage") != UINT32_MAX &&
                   UiBrowseSuiteIndex("no such suite") == UINT32_MAX,
               "%u suites, %u out of their families' order, %u listed wrong; the first in %s",
               (unsigned)count, (unsigned)backwards, (unsigned)twice,
               UiFamilyName(UiBrowseSuiteFamily(0)));
}

static void CheckParse(void) {
    // Lines as the report writes them, names and details with the characters an escape carries.
    static const char *const NAMES[] = {
        "a plain name",
        "a \"quoted\" name with a back\\slash",
        "tab\tand newline\nin a name",
    };
    uint32_t wrong = 0;
    char line[REPORT_LINE_MAX];
    ReportRecord record;
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); ++i) {
        ReportBuildCaseLine(line, sizeof(line), "clear", NAMES[i], "exact", i != 1u,
                            "a \"detail\"", NULL);
        // As the screens keep it from the same words: what a reader of the line gets back.
        ReportRecord want;
        ReportRecordFill(&want, "clear", NAMES[i], "exact", REPORT_PASS, "a \"detail\"", -1);
        if (!BrowseParseCaseLine(line, &record) || strcmp(record.suite, "clear") != 0 ||
            strcmp(record.name, want.name) != 0 || strcmp(record.tier, "exact") != 0 ||
            record.result != (i != 1u ? REPORT_PASS : REPORT_FAIL) ||
            (i == 1u && strcmp(record.detail, want.detail) != 0) || record.picture != -1) {
            ++wrong;
        }
    }
    ReportBuildResultLine(line, sizeof(line), "net", "a skipped case", "none", "skip", "crashed",
                          "it ended the program", NULL);
    const bool crashed = BrowseParseCaseLine(line, &record) && record.result == REPORT_CRASHED;
    ReportBuildResultLine(line, sizeof(line), "net", "another", "none", "skip", "depends", "x",
                          NULL);
    const bool skipped = BrowseParseCaseLine(line, &record) && record.result == REPORT_SKIP;
    const bool note = BrowseParseCaseLine("{\"type\":\"note\",\"text\":\"mode: quick\"}", &record);
    const bool torn = BrowseParseCaseLine("{\"type\":\"case\",\"suite\":\"clear\",\"name\":\"cut",
                                          &record);
    CHECK_TRUE(SUITE, "a report's case line reads back into the browser as it was written",
               CHECK_NONE, wrong == 0u && crashed && skipped && !note && !torn,
               "%u of 3 lines read back wrong; crashed %d, skipped %d, a note read %d, a torn "
               "line read %d",
               (unsigned)wrong, (int)crashed, (int)skipped, (int)note, (int)torn);
}

static void CheckCatalogue(void) {
    BrowseCatalog catalog;
    memset(&catalog, 0, sizeof(catalog));
    Catalogue(&catalog);
    const uint32_t before = catalog.count;
    ReportRecord again;
    ReportRecordFill(&again, "clear", "clear to opaque red", "exact", REPORT_FAIL, "now red", 3);
    BrowseCatalogMerge(&catalog, &again, 1u);
    const uint32_t at = CatalogueIndex(&catalog, "clear", "clear to opaque red");
    const bool in_place = catalog.count == before && at != UINT32_MAX &&
                          catalog.records[at].result == REPORT_FAIL &&
                          catalog.records[at].picture == -1;
    ReportRecordFill(&again, "clear", "a new case", "exact", REPORT_PASS, "", -1);
    BrowseCatalogMerge(&catalog, &again, 1u);
    const bool added = catalog.count == before + 1u;
    const uint32_t read = BrowseCatalogRead(&catalog, "no/such/report.jsonl");
    CHECK_TRUE(SUITE, "the catalogue keeps one case for a suite and a name, the newest",
               CHECK_NONE, in_place && added && read == 0u,
               "%u cases before, %u after; in place %d, added %d, a missing report gave %u",
               (unsigned)before, (unsigned)catalog.count, (int)in_place, (int)added,
               (unsigned)read);
    BrowseCatalogFree(&catalog);
}

static void CheckPick(void) {
    BrowseCatalog catalog;
    memset(&catalog, 0, sizeof(catalog));
    Catalogue(&catalog);
    UiBrowseInfo info;
    memset(&info, 0, sizeof(info));
    info.catalogue = catalog.records;
    info.catalogue_count = catalog.count;

    UiPick pick;
    memset(&pick, 0, sizeof(pick));
    Mark(pick.cases, CatalogueIndex(&catalog, "coverage", "the sample mask is in the mask written"));
    Mark(pick.suites, UiBrowseSuiteIndex("clear"));
    Mark(pick.cases, CatalogueIndex(&catalog, "dns", "a name resolves"));
    struct {
        const char *suite;
        const char *name;
        bool runs;
    } ASKED[] = {
        { "selfcheck", "anything the judge asks", true },
        { "coverage", "the sample mask is in the mask written", true },
        { "coverage", "graphics device opens with its targets", true },
        { "coverage", "a case the last run did not have", true },
        { "coverage", "four raster samples write 0xf where a pixel is covered", false },
        { "clear", "any case of a suite picked whole", true },
        { "fill", "a suite not picked", false },
        { "dns", "a missing name says so", false },
        { "nifm", "a suite of the net step not picked", false },
    };
    uint32_t wrong = 0;
    const char *first = "";
    for (size_t i = 0; i < sizeof(ASKED) / sizeof(ASKED[0]); ++i) {
        if (UiPickHas(&pick, &info, ASKED[i].suite, ASKED[i].name) != ASKED[i].runs &&
            wrong++ == 0u) {
            first = ASKED[i].name;
        }
    }
    struct {
        const char *step;
        bool runs;
    } STEPS[] = {
        { "selfcheck", true }, { "support", true }, { "reference", true }, { "cpu", false },
        { "coverage", true },  { "clear", true },   { "fill", false },     { "net", true },
        { "service", false },  { "game", false },
    };
    uint32_t steps_wrong = 0;
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); ++i) {
        steps_wrong += BrowseStepRuns(STEPS[i].step, &pick, &info) != STEPS[i].runs ? 1u : 0u;
    }
    // No pick at all runs everything: the report's filter is off.
    const bool everything = UiPickHas(NULL, &info, "fill", "anything");
    CHECK_TRUE(SUITE,
               "a pick runs the judge, its suites whole, its cases with their suite's bring-up, "
               "and the steps that hold them",
               CHECK_NONE, wrong == 0u && steps_wrong == 0u && everything,
               "%u cases decided wrong, the first \"%s\"; %u steps wrong; no pick runs all %d",
               (unsigned)wrong, first, (unsigned)steps_wrong, (int)everything);

    // The levels of a suite a pick asks for.
    UiPick levels;
    memset(&levels, 0, sizeof(levels));
    Mark(levels.cases,
         CatalogueIndex(&catalog, "wimsy", "level 3, frame 149 of 600 is what the processor works "
                                           "out"));
    const uint32_t from_case = BrowseLevels(&levels, &info, "wimsy", 5u);
    Mark(levels.suites, UiBrowseSuiteIndex("wimsy"));
    const uint32_t whole = BrowseLevels(&levels, &info, "wimsy", 5u);
    const uint32_t none = BrowseLevels(&pick, &info, "wimsy", 5u);
    CHECK_TRUE(SUITE, "a pick asks a suite for the levels its cases name, or all of them whole",
               CHECK_NONE, from_case == 1u << 3 && whole == 0x1Fu && none == 0u,
               "from a case 0x%x, whole 0x%x, unpicked 0x%x", (unsigned)from_case,
               (unsigned)whole, (unsigned)none);

    char said[256];
    BrowseDescribePick(said, sizeof(said), &pick, &info);
    CHECK_TRUE(SUITE, "the run's mode line says what the browser picked", CHECK_NONE,
               strcmp(said, "clear whole; coverage: 1 case; dns: 1 case") == 0, "\"%s\"", said);
    BrowseCatalogFree(&catalog);
}

static UiBrowseState Press(UiBrowseState state, uint32_t buttons, const UiBrowseInfo *info) {
    return UiBrowseStep(state, buttons, UI_SETTLING_FRAMES, info);
}

static void CheckWalk(void) {
    BrowseCatalog catalog;
    memset(&catalog, 0, sizeof(catalog));
    Catalogue(&catalog);
    static const UiDemoRow DEMOS[2] = {
        { "Game scene", "game", "the scene", NULL },
        { "Wimsy level 3", "wimsy", "a heavy frame", "wimsy" },
    };
    UiStartInfo start;
    memset(&start, 0, sizeof(start));
    start.heading = "edenprobe";
    start.families_present = MODE_FAMILIES_PRESENT;
    UiBrowseInfo info;
    memset(&info, 0, sizeof(info));
    info.heading = "edenprobe";
    info.catalogue = catalog.records;
    info.catalogue_count = catalog.count;
    info.demos = DEMOS;
    info.demo_count = 2u;
    info.start = &start;
    const RunMode mode = ModeFromText("");
    const UiStartState opened = UiStartOpen(&mode, MODE_FAMILIES_PRESENT);
    UiBrowseState state = UiBrowseOpen(UI_TAB_SUITES, &opened, UiOptionsFrom(&mode), &info);

    // Nothing while the pad settles, then R and L walk the tabs round.
    const UiBrowseState settling = UiBrowseStep(state, UI_BUTTON_TAB_NEXT, 0u, &info);
    UiBrowseState walked = Press(state, UI_BUTTON_TAB_NEXT, &info);
    const bool to_demos = walked.tab == UI_TAB_DEMOS;
    walked = Press(Press(walked, UI_BUTTON_TAB_PREVIOUS, &info), UI_BUTTON_TAB_PREVIOUS, &info);
    const bool round = settling.tab == UI_TAB_SUITES && to_demos && walked.tab == UI_TAB_START;

    // The suites: graphics open, down to coverage, its cases, a case marked, back up.
    state.family = (uint32_t)UI_FAMILY_GRAPHICS;
    state = Press(state, UI_BUTTON_OPEN, &info);
    const bool opened_family = state.level == 1u &&
                               UiBrowseSuiteFamily(state.suite) == UI_FAMILY_GRAPHICS;
    while (strcmp(UiBrowseSuite(state.suite), "coverage") != 0 && state.level == 1u) {
        const uint32_t before = state.suite;
        state = Press(state, UI_BUTTON_DOWN, &info);
        if (state.suite == before) {
            break;
        }
    }
    state = Press(state, UI_BUTTON_OPEN, &info);
    const bool in_cases = state.level == 2u && UiBrowseCases(&info, state.suite) == 5u;
    state = Press(Press(state, UI_BUTTON_DOWN, &info), UI_BUTTON_DOWN, &info);
    state = Press(state, UI_BUTTON_CYCLE_VIEW, &info);
    const uint32_t marked_case = UiBrowseCaseAt(&info, state.suite, 2u);
    const bool marked = (state.case_marks[marked_case / 8u] >> (marked_case % 8u) & 1u) != 0u;
    UiPick pick;
    // Plus asks whether the judge runs first, and only A or B after it asks for the run.
    state = Press(state, UI_BUTTON_RUN, &info);
    const bool question = state.waiting == UI_ASK_RUN && state.ask == UI_ASK_NONE;
    state = Press(state, UI_BUTTON_OPEN, &info);
    const bool run_asked = question && state.judge && state.ask == UI_ASK_RUN &&
                           UiBrowsePick(&state, &info, &pick) &&
                           (pick.cases[marked_case / 8u] >> (marked_case % 8u) & 1u) != 0u;
    state = Press(Press(state, UI_BUTTON_OPEN, &info), UI_BUTTON_BACK, &info);
    const bool alone = !state.judge && state.ask == UI_ASK_RUN_ONE &&
                       UiBrowsePick(&state, &info, &pick);
    state = Press(Press(state, UI_BUTTON_BACK, &info), UI_BUTTON_BACK, &info);
    const bool back_up = state.level == 0u && state.tab == UI_TAB_SUITES;
    state = Press(state, UI_BUTTON_BACK, &info);
    const bool to_start = state.tab == UI_TAB_START;

    // The start tab begins its families; the demos tab runs the one under the cursor; minus
    // quits from any tab.
    state = Press(state, UI_BUTTON_RUN, &info);
    const bool begin = state.ask == UI_ASK_BEGIN && UiBrowsePick(&state, &info, &pick) &&
                       UiPickHasFamily(&pick, UI_FAMILY_GRAPHICS) &&
                       !UiPickHasFamily(&pick, UI_FAMILY_AUDIO);
    state.tab = UI_TAB_DEMOS;
    state = Press(Press(state, UI_BUTTON_DOWN, &info), UI_BUTTON_OPEN, &info);
    state = Press(state, UI_BUTTON_QUIT, &info);
    const bool backed = state.waiting == UI_ASK_NONE && state.ask == UI_ASK_NONE;
    state = Press(Press(state, UI_BUTTON_OPEN, &info), UI_BUTTON_OPEN, &info);
    const bool demo = backed && state.judge && state.ask == UI_ASK_RUN_ONE &&
                      UiBrowsePick(&state, &info, &pick) && pick.demos == 2u;
    state = Press(state, UI_BUTTON_QUIT, &info);
    const bool quit = state.ask == UI_ASK_QUIT;
    CHECK_TRUE(SUITE, "the browser walks its tabs, its levels and its marks as its keys say",
               CHECK_NONE,
               round && opened_family && in_cases && marked && run_asked && alone && back_up &&
                   to_start && begin && demo && quit,
               "tabs %d, family %d, cases %d, mark %d, run %d, alone %d, back %d, start %d, "
               "begin %d, demo %d, quit %d",
               (int)round, (int)opened_family, (int)in_cases, (int)marked, (int)run_asked,
               (int)alone, (int)back_up, (int)to_start, (int)begin, (int)demo, (int)quit);

    // Options start as the mode file leaves them and change on their row.
    const RunMode file = ModeFromText("notime images retry frames=200");
    UiBrowseState options = UiBrowseOpen(UI_TAB_OPTIONS, &opened, UiOptionsFrom(&file), &info);
    const bool from_file = !options.options.timing && options.options.every_picture &&
                           options.options.retry && options.options.frames == 600u &&
                           options.options.music && options.options.motion;
    options = Press(Press(Press(options, UI_BUTTON_DOWN, &info), UI_BUTTON_DOWN, &info),
                    UI_BUTTON_DOWN, &info);
    options = Press(options, UI_BUTTON_RIGHT, &info);
    const uint32_t after_right = options.options.frames;
    options = Press(Press(options, UI_BUTTON_LEFT, &info), UI_BUTTON_LEFT, &info);
    CHECK_TRUE(SUITE, "the options start as the mode file leaves them and change on their row",
               CHECK_NONE, from_file && after_right == 3600u && options.options.frames == 120u,
               "as the file %d; frames %u after right, %u after two lefts", (int)from_file,
               (unsigned)after_right, (unsigned)options.options.frames);

    // Taps: a tab, then a key of the foot.
    UiBrowseState tapped = UiBrowseOpen(UI_TAB_SUITES, &opened, UiOptionsFrom(&mode), &info);
    UiKey keys[UI_FOOT_KEYS];
    const uint32_t count = UiBrowseKeys(&tapped, &info, keys);
    uint32_t quit_key = UINT32_MAX;
    for (uint32_t i = 0; i < count; ++i) {
        if (strcmp(keys[i].key, "Minus") == 0) {
            quit_key = i;
        }
    }
    const UiRect box = UiFootKey(FRAME_WIDTH, FRAME_HEIGHT, keys, count, quit_key);
    const UiTap on_key = { box.x + 4u, box.y + 4u, box.x + 6u, box.y + 6u };
    const UiTap on_tab = { 24u, UI_TABS_TOP + 4u, 26u, UI_TABS_TOP + 6u };
    const UiBrowseState tab_tap = UiBrowseTap(tapped, &on_tab, FRAME_WIDTH, FRAME_HEIGHT, &info);
    const UiBrowseState key_tap = UiBrowseTap(tapped, &on_key, FRAME_WIDTH, FRAME_HEIGHT, &info);
    CHECK_TRUE(SUITE, "a tap on a tab and on a key of the foot do what the buttons do",
               CHECK_NONE,
               box.width > 0u && tab_tap.tab == UI_TAB_START && key_tap.ask == UI_ASK_QUIT,
               "the quit key's box %ux%u at %u,%u; the tab tap went to tab %u, the key tap "
               "asked %u",
               (unsigned)box.width, (unsigned)box.height, (unsigned)box.x, (unsigned)box.y,
               (unsigned)tab_tap.tab, (unsigned)key_tap.ask);
    BrowseCatalogFree(&catalog);
}

/// Whether every key of @p keys has its box on the foot, left to right, none over the next: the
/// first that has not is said in @p said.
static bool KeysFit(const UiKey *keys, uint32_t count, const char *where, char *said,
                    size_t said_size) {
    uint32_t right = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const UiRect box = UiFootKey(FRAME_WIDTH, FRAME_HEIGHT, keys, count, i);
        if (box.width == 0u || box.x < right || box.x + box.width > FRAME_WIDTH) {
            if (said[0] == '\0') {
                snprintf(said, said_size, "%s: key %u of %u, %s %s, has the box %ux%u at %u",
                         where, (unsigned)(i + 1u), (unsigned)count, keys[i].key, keys[i].what,
                         (unsigned)box.width, (unsigned)box.height, (unsigned)box.x);
            }
            return false;
        }
        right = box.x + box.width;
    }
    return true;
}

/// Every foot the browser draws holds all its keys, the results' of a run it can run again too,
/// and on the results tab a tap on a key does what its button does.
static void CheckFeet(void) {
    BrowseCatalog catalog;
    memset(&catalog, 0, sizeof(catalog));
    Catalogue(&catalog);
    // A picture on the first failure, so its page names the X key as well.
    catalog.records[CatalogueIndex(&catalog, "coverage",
                                   "four raster samples write 0xf where a pixel is covered")]
        .picture = 0;
    static UiRun run;
    UiRunIndex(&run, catalog.records, catalog.count, catalog.count);
    static const UiDemoRow DEMOS[1] = { { "Game scene", "game", "the scene", NULL } };
    UiStartInfo start;
    memset(&start, 0, sizeof(start));
    start.heading = "edenprobe";
    start.families_present = MODE_FAMILIES_PRESENT;
    UiBrowseInfo info;
    memset(&info, 0, sizeof(info));
    info.heading = "edenprobe";
    info.catalogue = catalog.records;
    info.catalogue_count = catalog.count;
    info.demos = DEMOS;
    info.demo_count = 1u;
    info.start = &start;
    info.results = &run;
    const RunMode mode = ModeFromText("");
    const UiStartState opened = UiStartOpen(&mode, MODE_FAMILIES_PRESENT);

    char said[200] = "";
    uint32_t sets = 0;
    bool fit = true;
    UiKey keys[UI_FOOT_KEYS];
    static const char *const TABS[UI_TABS] = { "start", "suites", "demos", "results",
                                               "options" };
    for (uint32_t tab = 0; tab < UI_TABS; ++tab) {
        UiBrowseState state = UiBrowseOpen((UiTab)tab, &opened, UiOptionsFrom(&mode), &info);
        state.suite = UiBrowseSuiteIndex("coverage");
        state.family = (uint32_t)UI_FAMILY_GRAPHICS;
        for (uint32_t level = 0; level < (tab == UI_TAB_SUITES ? 3u : 1u); ++level) {
            state.level = level;
            char where[48];
            snprintf(where, sizeof(where), "the %s tab, level %u", TABS[tab], (unsigned)level);
            fit = KeysFit(keys, UiBrowseKeys(&state, &info, keys), where, said, sizeof(said)) &&
                  fit;
            ++sets;
        }
    }
    // The results, in the browser and out of it, at each level, a run it can run again or not.
    static const char *const LEVELS[3] = { "overview", "suite", "case" };
    for (uint32_t browser = 0; browser < 2u; ++browser) {
        for (uint32_t repeatable = 0; repeatable < 2u; ++repeatable) {
            for (uint32_t level = 0; level < 3u; ++level) {
                UiState state = UiStart(&run);
                state.browser = browser != 0u;
                state.repeatable = repeatable != 0u;
                state.level = (UiLevel)level;
                char where[96];
                snprintf(where, sizeof(where), "the results%s, %s%s",
                         browser != 0u ? " in the browser" : "", LEVELS[level],
                         repeatable != 0u ? ", the run repeatable" : "");
                fit = KeysFit(keys, UiResultsKeys(&run, state, keys), where, said,
                              sizeof(said)) &&
                      fit;
                ++sets;
            }
        }
    }
    CHECK_TRUE(SUITE, "every foot the browser draws holds all its keys, one box after another",
               CHECK_NONE, fit, "%u key sets laid out; %s", (unsigned)sets,
               said[0] != '\0' ? said : "every key has its box");

    // On the results tab of a run it can run again: a tap on Y, on minus and on B.
    UiBrowseState results = UiBrowseOpen(UI_TAB_RESULTS, &opened, UiOptionsFrom(&mode), &info);
    results.results = UiStart(&run);
    results.results.browser = true;
    results.results.repeatable = true;
    results.results.level = UI_LEVEL_OVERVIEW;
    const uint32_t count = UiResultsKeys(&run, results.results, keys);
    UiAsk asked[3] = { UI_ASK_NONE, UI_ASK_NONE, UI_ASK_NONE };
    UiTab tab_after_b = UI_TABS;
    static const char *const TAPPED[3] = { "Y", "Minus", "B" };
    for (uint32_t t = 0; t < 3u; ++t) {
        for (uint32_t i = 0; i < count; ++i) {
            if (strcmp(keys[i].key, TAPPED[t]) != 0) {
                continue;
            }
            const UiRect box = UiFootKey(FRAME_WIDTH, FRAME_HEIGHT, keys, count, i);
            const UiTap tap = { box.x + 4u, box.y + 4u, box.x + 6u, box.y + 6u };
            const UiBrowseState after =
                UiBrowseTap(results, &tap, FRAME_WIDTH, FRAME_HEIGHT, &info);
            // Y asks to run again, which waits on the judge question as every run picked does.
            asked[t] = after.waiting != UI_ASK_NONE ? after.waiting : after.ask;
            if (t == 2u) {
                tab_after_b = after.tab;
            }
        }
    }
    CHECK_TRUE(SUITE, "on the browser's results a tap on Y, minus and B does what the button does",
               CHECK_NONE,
               asked[0] == UI_ASK_AGAIN && asked[1] == UI_ASK_QUIT && asked[2] == UI_ASK_NONE &&
                   tab_after_b == UI_TAB_SUITES,
               "Y asked %u (again is %u), minus asked %u (quit is %u), B asked %u and went to "
               "tab %u (the suites are %u)",
               (unsigned)asked[0], (unsigned)UI_ASK_AGAIN, (unsigned)asked[1],
               (unsigned)UI_ASK_QUIT, (unsigned)asked[2], (unsigned)tab_after_b,
               (unsigned)UI_TAB_SUITES);
    BrowseCatalogFree(&catalog);
}

/// Every demo's music is one of the music's tracks, and the game scene's goes through its own
/// renderer, since its audio thread steps one; the others through the browser's player.
static void CheckMusic(void) {
    static const struct {
        const char *suite;
        const char *track;
        bool streams;
    } WANT[] = {
        { "game", "wimsy", true },          { "wimsy", "wimsy", false },
        { "mandelbrot", "fractal", false }, { "julia", "fractal", false },
        { "fluid", "fluid", false },        { "fluid-alias", "fluid", false },
    };
    char said[200] = "";
    for (size_t i = 0; i < sizeof(WANT) / sizeof(WANT[0]) && said[0] == '\0'; ++i) {
        const BrowseMusic music = BrowseMusicOf(WANT[i].suite);
        if (music.track == NULL || strcmp(music.track, WANT[i].track) != 0 ||
            music.streams != WANT[i].streams || MusicTrackNamed(music.track) == MUSIC_TRACKS) {
            snprintf(said, sizeof(said), "%s plays %s%s, where %s%s", WANT[i].suite,
                     music.track != NULL ? music.track : "none",
                     music.streams ? " on its own renderer" : "", WANT[i].track,
                     WANT[i].streams ? " on its own renderer" : "");
        }
    }
    const BrowseMusic none = BrowseMusicOf("clear");
    if (said[0] == '\0' && none.track != NULL) {
        snprintf(said, sizeof(said), "clear, no demo, plays %s", none.track);
    }
    CHECK_TRUE(SUITE, "every demo plays one of the music's tracks, the game scene's on its own "
               "renderer",
               CHECK_NONE, said[0] == '\0', "%s", said);
}

static void CheckScriptRuns(void) {
    // Every mode file a script or a runner writes keeps the browser closed, and with it the
    // music and everything the browser would read; the word opens it in a detailed run only.
    static const char *const SCRIPTS[] = {
        "quick",
        "quick all",
        "quick notime machine=scarletnx hooks",
        "quick game level=3 frames=120",
        "quick browse",
        "quick listen audio",
    };
    uint32_t opened = 0;
    for (size_t i = 0; i < sizeof(SCRIPTS) / sizeof(SCRIPTS[0]); ++i) {
        const RunMode mode = ModeFromText(SCRIPTS[i]);
        opened += ModeOpensBrowser(&mode) ? 1u : 0u;
    }
    const RunMode word = ModeFromText("browse");
    const RunMode both = ModeFromText("quick detailed browse");
    const RunMode refused = ModeFromText("quick browse");
    char said[REPORT_NOTE_MAX + 1];
    ModeDescribe(said, sizeof(said), &refused, MODE_FAMILIES_PRESENT);
    char opens[REPORT_NOTE_MAX + 1];
    ModeDescribe(opens, sizeof(opens), &word, MODE_FAMILIES_PRESENT);
    CHECK_TRUE(SUITE,
               "no mode file a script writes opens the browser, and the word browse opens it in "
               "a detailed run",
               CHECK_NONE,
               opened == 0u && ModeOpensBrowser(&word) && ModeOpensBrowser(&both) &&
                   word.unknown_count == 0u &&
                   strstr(said, "browse refused: the run is quick") != NULL &&
                   strstr(opens, "the browser, not the start screen") != NULL,
               "%u script files opened it; browse %d, quick detailed browse %d; the lines "
               "\"%.120s\" and \"%.120s\"",
               (unsigned)opened, (int)ModeOpensBrowser(&word), (int)ModeOpensBrowser(&both),
               said, opens);
}

static uint32_t CountColour(const UiFrame *frame, uint32_t x0, uint32_t y0, uint32_t x1,
                            uint32_t y1, const uint8_t colour[3]) {
    uint32_t count = 0;
    for (uint32_t y = y0; y < y1; ++y) {
        for (uint32_t x = x0; x < x1; ++x) {
            count += memcmp(UiPixel(frame, x, y), colour, 3u) == 0 ? 1u : 0u;
        }
    }
    return count;
}

static void CheckHud(uint8_t *pixels) {
    // A frame the demo drew: one colour no screen uses.
    static const uint8_t DEMO[4] = { 0x10, 0x80, 0x30, 0xFF };
    const UiFrame frame = { pixels, FRAME_WIDTH, FRAME_HEIGHT, FRAME_WIDTH * 4u };
    for (size_t i = 0; i < (size_t)FRAME_WIDTH * FRAME_HEIGHT; ++i) {
        memcpy(pixels + i * 4u, DEMO, 4u);
    }
    UiHud hud;
    memset(&hud, 0, sizeof(hud));
    hud.name = "Wimsy level 3";
    hud.track = "wimsy";
    hud.frame = 312u;
    hud.frames = 600u;
    hud.frame_us = 16667u;
    hud.fps_tenths = 600u;
    for (uint32_t i = 0; i < UI_HUD_TIMES; ++i) {
        hud.times[i] = i == 97u ? 40000u : 16667u;
    }
    hud.time_count = UI_HUD_TIMES;
    UiComposeHud(&frame, &hud);
    // Its boxes in the corners and the keys at the bottom, the middle of the frame left alone,
    // and nothing in between blended: every pixel is the demo's or one of the menu's colours.
    const uint32_t middle = CountColour(&frame, 400u, 220u, 880u, 600u, DEMO);
    const uint32_t box = CountColour(&frame, 16u, 16u, 384u, 200u, PAGE);
    const bool hitch = CountColour(&frame, 912u, 60u, 1264u, 200u,
                                   (const uint8_t[3]){ 0xA5, 0x47, 0x3C }) > 0u;
    UiPauseInfo info;
    memset(&info, 0, sizeof(info));
    static const UiDemoRow LEVELS[5] = {
        { "Wimsy level 0", "wimsy", "", "wimsy" }, { "Wimsy level 1", "wimsy", "", "wimsy" },
        { "Wimsy level 2", "wimsy", "", "wimsy" }, { "Wimsy level 3", "wimsy", "", "wimsy" },
        { "Wimsy level 4", "wimsy", "", "wimsy" },
    };
    info.name = "Wimsy level 3";
    info.levels = LEVELS;
    info.level_count = 5u;
    info.running = 3u;
    info.frame = 312u;
    info.frames = 600u;
    UiPauseState pause = UiPauseOpen(&info, 600u, true, true);
    for (size_t i = 0; i < (size_t)FRAME_WIDTH * FRAME_HEIGHT; ++i) {
        memcpy(pixels + i * 4u, DEMO, 4u);
    }
    UiComposePause(&frame, &info, &pause);
    // Every other row outside the menu's box the run's page; the rows between them the demo's.
    const uint32_t dimmed = CountColour(&frame, 0u, 0u, 100u, 90u, RUN_PAGE);
    const uint32_t kept = CountColour(&frame, 0u, 0u, 100u, 90u, DEMO);
    CHECK_TRUE(SUITE, "the HUD and the pause menu draw over a demo's frame with nothing blended",
               CHECK_NONE,
               middle == 480u * 380u && box > 20000u && hitch && dimmed == 100u * 45u &&
                   kept == 100u * 45u,
               "%u of the middle's %u pixels the demo's; %u of the HUD's box the page; a hitch "
               "drawn %d; %u rows' pixels dimmed and %u kept",
               (unsigned)middle, 480u * 380u, (unsigned)box, (int)hitch, (unsigned)dimmed,
               (unsigned)kept);

    // The pause menu: down to the level, right to the next, A starts it; Plus resumes; B back.
    pause = UiPauseStep(pause, UI_BUTTON_DOWN, &info);
    pause = UiPauseStep(pause, UI_BUTTON_RIGHT, &info);
    const bool level = pause.level == 4u && !pause.start_level;
    pause = UiPauseStep(pause, UI_BUTTON_OPEN, &info);
    const bool started = pause.start_level && pause.level == 4u;
    UiPauseState other = UiPauseOpen(&info, 600u, true, true);
    other = UiPauseStep(UiPauseStep(other, UI_BUTTON_DOWN, &info), UI_BUTTON_DOWN, &info);
    other = UiPauseStep(other, UI_BUTTON_RIGHT, &info);
    const bool frames = other.frames == 3600u;
    const bool resumed = UiPauseStep(other, UI_BUTTON_RUN, &info).resume;
    const bool back = UiPauseStep(other, UI_BUTTON_BACK, &info).back;
    // A demo's own rows sit between the HUD's and Restart: Left and Right walk a row's values
    // round, A starts the demo with them, and Restart and Back are the rows after them.
    static const char *const THICK[3] = { "Thin", "Thick", "Thicker" };
    static const char *const SWITCH[2] = { "On", "Off" };
    static const UiPauseOption OWN[2] = { { "Grass", THICK, 3u }, { "Legend", SWITCH, 2u } };
    UiPauseInfo with = info;
    with.options = OWN;
    with.option_count = 2u;
    with.option_now[0] = 2u;
    UiPauseState own = UiPauseOpen(&with, 600u, true, true);
    for (uint32_t i = 0; i < 5u; ++i) {
        own = UiPauseStep(own, UI_BUTTON_DOWN, &with);
    }
    own = UiPauseStep(own, UI_BUTTON_RIGHT, &with);
    const bool walked = own.cursor == 5u && own.option[0] == 0u && own.option[1] == 0u && !own.start_level;
    own = UiPauseStep(UiPauseStep(own, UI_BUTTON_DOWN, &with), UI_BUTTON_LEFT, &with);
    const bool second = own.cursor == 6u && own.option[1] == 1u;
    const bool chosen = UiPauseStep(own, UI_BUTTON_OPEN, &with).start_level;
    own = UiPauseStep(own, UI_BUTTON_DOWN, &with);
    const UiPauseState again = UiPauseStep(own, UI_BUTTON_OPEN, &with);
    const bool restart = own.cursor == 7u && again.start_level && again.level == 3u;
    own = UiPauseStep(UiPauseStep(own, UI_BUTTON_DOWN, &with), UI_BUTTON_DOWN, &with);
    const bool round_again = own.cursor == 0u;
    UiComposePause(&frame, &with, &own);
    const bool rows_own = walked && second && chosen && restart && round_again;
    CHECK_TRUE(SUITE, "the pause menu changes the level and the frames, resumes and goes back",
               CHECK_NONE, level && started && frames && resumed && back && rows_own,
               "level %d, started %d, frames %d (%u), resumed %d, back %d; a demo's own rows: "
               "walked %d, the second %d, chosen %d, restart after them %d, round %d",
               (int)level, (int)started, (int)frames, (unsigned)other.frames, (int)resumed,
               (int)back, (int)walked, (int)second, (int)chosen, (int)restart, (int)round_again);

    // Motion: the new screen from the top, all of it at the last step.
    CHECK_TRUE(SUITE, "motion brings a new screen in from the top, whole at its last step",
               CHECK_NONE,
               UiMotionRows(0u, 720u) == 120u && UiMotionRows(2u, 720u) == 360u &&
                   UiMotionRows(UI_MOTION_STEPS, 720u) == 720u &&
                   UiMotionRows(UI_MOTION_STEPS + 3u, 720u) == 720u,
               "steps 0, 2 and the last show %u, %u and %u rows", (unsigned)UiMotionRows(0u, 720u),
               (unsigned)UiMotionRows(2u, 720u), (unsigned)UiMotionRows(UI_MOTION_STEPS, 720u));
}

/// The browser the mode file's word opens waits a minute for a person, counting it down on the
/// foot, and the start screen's waits for ever; the countdown is drawn on the foot and nowhere
/// else.
static void CheckWait(uint8_t *pixels) {
    static const char NAME[] = "the browser the word opens counts a minute down on its foot, and "
                               "the start screen's never leaves";
    const uint64_t second = 1000000000ull;
    const uint64_t by_word = UiBrowseUnattendedNanoseconds(true);
    const uint64_t by_screen = UiBrowseUnattendedNanoseconds(false);
    const uint32_t left[5] = {
        UiBrowseSecondsLeft(by_word, 0u),
        UiBrowseSecondsLeft(by_word, 59u * second + 1u),
        UiBrowseSecondsLeft(by_word, 60u * second - 1u),
        UiBrowseSecondsLeft(by_word, 60u * second),
        UiBrowseSecondsLeft(by_screen, 1000u * second),
    };
    const bool counts = UI_BROWSE_NANOSECONDS == 60u * second && by_word == 60u * second &&
                        by_screen == 0u && left[0] == 60u && left[1] == 1u && left[2] == 1u &&
                        left[3] == 0u && left[4] == 0u;
    UiStartInfo start;
    memset(&start, 0, sizeof(start));
    start.heading = "edenprobe 0.17.0 (test)";
    start.families_present = MODE_FAMILIES_PRESENT;
    UiBrowseInfo info;
    memset(&info, 0, sizeof(info));
    info.heading = "0.17.0 on scarletnx";
    info.machine = info.heading;
    info.start = &start;
    const RunMode mode = ModeFromText("");
    const UiStartState opened = UiStartOpen(&mode, MODE_FAMILIES_PRESENT);
    const UiBrowseState state =
        UiBrowseOpen(UI_TAB_SUITES, &opened, UiOptionsFrom(&mode), &info);
    const size_t bytes = (size_t)FRAME_WIDTH * FRAME_HEIGHT * 4u;
    uint8_t *const without = (uint8_t *)malloc(bytes);
    if (pixels == NULL || without == NULL) {
        free(without);
        ReportCase(SUITE, NAME, CHECK_NONE, false, "no memory for two frames");
        return;
    }
    const UiFrame frame = { pixels, FRAME_WIDTH, FRAME_HEIGHT, FRAME_WIDTH * 4u };
    UiComposeBrowse(&frame, &info, &state);
    memcpy(without, pixels, bytes);
    info.seconds_left = left[0];
    UiComposeBrowse(&frame, &info, &state);
    uint32_t on_foot = 0;
    uint32_t above = 0;
    for (uint32_t y = 0; y < FRAME_HEIGHT; ++y) {
        for (uint32_t x = 0; x < FRAME_WIDTH; ++x) {
            const size_t at = ((size_t)y * FRAME_WIDTH + x) * 4u;
            if (memcmp(pixels + at, without + at, 4u) != 0) {
                on_foot += y >= UI_FOOT_TOP(FRAME_HEIGHT) ? 1u : 0u;
                above += y < UI_FOOT_TOP(FRAME_HEIGHT) ? 1u : 0u;
            }
        }
    }
    free(without);
    CHECK_TRUE(SUITE, NAME, CHECK_NONE, counts && on_foot > 0u && above == 0u,
               "waits %llu s by the word and %llu s from the start screen; seconds left at "
               "nought %u, a breath past 59 s %u, a breath before 60 s %u, at 60 s %u, with no "
               "limit %u; the countdown changed %u pixels on the foot and %u above it",
               (unsigned long long)(by_word / second), (unsigned long long)(by_screen / second),
               (unsigned)left[0], (unsigned)left[1], (unsigned)left[2], (unsigned)left[3],
               (unsigned)left[4], (unsigned)on_foot, (unsigned)above);
}

/// The browser's screens written where a person can look at them, when EDENPROBE_FRAMES names a
/// directory: host only, and never a case, as the screens' own frames are.
static void DumpBrowser(uint8_t *pixels) {
#ifndef __SWITCH__
    const char *const directory = getenv("EDENPROBE_FRAMES");
    if (directory == NULL || directory[0] == '\0' || pixels == NULL) {
        return;
    }
    BrowseCatalog catalog;
    memset(&catalog, 0, sizeof(catalog));
    Catalogue(&catalog);
    static const UiDemoRow DEMOS[6] = {
        { "Game scene", "game", "Everything a game asks at once: four threads, a silent tone, the "
                                "card and the services.",
          NULL },
        { "Wimsy level 0", "wimsy", "Sprites: Wimsy flat, a sheet of her frames.", "wimsy" },
        { "Wimsy level 1", "wimsy", "Her lit model, turning.", "wimsy" },
        { "Wimsy level 2", "wimsy", "A scene near and far.", "wimsy" },
        { "Wimsy level 3", "wimsy", "A game's heavy frame: Wimsy among some fifteen hundred "
                                    "draws over a 4K ASTC ground.",
          "wimsy" },
        { "Wimsy level 4", "wimsy", "Level 3 over an 8K ground.", "wimsy" },
    };
    UiStartInfo start;
    memset(&start, 0, sizeof(start));
    start.heading = "edenprobe 0.17.0 (test)";
    start.families_present = MODE_FAMILIES_PRESENT;
    UiBrowseInfo info;
    memset(&info, 0, sizeof(info));
    info.heading = "0.17.0 on scarletnx";
    info.machine = info.heading;
    info.catalogue = catalog.records;
    info.catalogue_count = catalog.count;
    info.catalogue_from = "sdmc:/edenprobe/runs/test";
    info.demos = DEMOS;
    info.demo_count = 6u;
    info.start = &start;
    const RunMode mode = ModeFromText("");
    const UiStartState opened = UiStartOpen(&mode, MODE_FAMILIES_PRESENT);
    const UiFrame frame = { pixels, FRAME_WIDTH, FRAME_HEIGHT, FRAME_WIDTH * 4u };
    UiBrowseState state = UiBrowseOpen(UI_TAB_SUITES, &opened, UiOptionsFrom(&mode), &info);
    static const struct {
        const char *name;
        UiTab tab;
        uint32_t level;
    } SHOTS[] = {
        { "30-browse-start", UI_TAB_START, 0u },   { "31-browse-families", UI_TAB_SUITES, 0u },
        { "32-browse-suites", UI_TAB_SUITES, 1u }, { "33-browse-cases", UI_TAB_SUITES, 2u },
        { "34-browse-demos", UI_TAB_DEMOS, 0u },   { "35-browse-options", UI_TAB_OPTIONS, 0u },
        { "36-browse-countdown", UI_TAB_SUITES, 0u },
    };
    for (size_t i = 0; i < sizeof(SHOTS) / sizeof(SHOTS[0]); ++i) {
        // The last as the word opens it, nothing pressed yet.
        info.seconds_left = i + 1u == sizeof(SHOTS) / sizeof(SHOTS[0])
                                ? (uint32_t)(UI_BROWSE_NANOSECONDS / 1000000000ull)
                                : 0u;
        state.tab = SHOTS[i].tab;
        state.level = SHOTS[i].level;
        state.suite = UiBrowseSuiteIndex("coverage");
        state.item = 2u;
        state.demo = 4u;
        state.option = UI_OPTION_FRAMES;
        if (SHOTS[i].level == 2u) {
            Mark(state.case_marks, CatalogueIndex(&catalog, "coverage", "the sample mask is in "
                                                                       "the mask written"));
            Mark(state.suite_marks, UiBrowseSuiteIndex("clear"));
        }
        UiComposeBrowse(&frame, &info, &state);
        char path[512];
        snprintf(path, sizeof(path), "%.400s/%s.pam", directory, SHOTS[i].name);
        FILE *const file = fopen(path, "wb");
        if (file != NULL) {
            fprintf(file,
                    "P7\nWIDTH %u\nHEIGHT %u\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n",
                    (unsigned)FRAME_WIDTH, (unsigned)FRAME_HEIGHT);
            fwrite(pixels, 4u, (size_t)FRAME_WIDTH * FRAME_HEIGHT, file);
            fclose(file);
        }
    }
    BrowseCatalogFree(&catalog);
#else
    (void)pixels;
#endif
}

void RunSupportBrowse(void) {
    uint8_t *const pixels = (uint8_t *)malloc((size_t)FRAME_WIDTH * FRAME_HEIGHT * 4u);
    CheckSuites();
    CheckParse();
    CheckCatalogue();
    CheckPick();
    CheckWalk();
    CheckFeet();
    CheckMusic();
    CheckScriptRuns();
    CheckWait(pixels);
    if (pixels == NULL) {
        static const char *const NAMES[3] = {
            "the HUD and the pause menu draw over a demo's frame with nothing blended",
            "the pause menu changes the level and the frames, resumes and goes back",
            "motion brings a new screen in from the top, whole at its last step",
        };
        for (size_t i = 0; i < 3u; ++i) {
            ReportCase(SUITE, NAMES[i], CHECK_NONE, false, "no memory for a frame");
        }
        return;
    }
    CheckHud(pixels);
    DumpBrowser(pixels);
    free(pixels);
}
