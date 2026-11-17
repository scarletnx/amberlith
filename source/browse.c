// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browse.h"

/// The JSON string after `"key":` in @p line, unescaped: a character past ASCII, which no line
/// here writes, reads as a question mark. False when the key is not there or its string does
/// not end.
static bool StringField(const char *line, const char *key, char *out, size_t out_size) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *at = strstr(line, pattern);
    if (out_size > 0u) {
        out[0] = '\0';
    }
    if (at == NULL || out_size == 0u) {
        return false;
    }
    at += strlen(pattern);
    size_t used = 0;
    for (;;) {
        char c = *at++;
        if (c == '\0') {
            return false;
        }
        if (c == '"') {
            break;
        }
        if (c == '\\') {
            const char escaped = *at++;
            switch (escaped) {
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
            case 'r':
                c = '\r';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'u': {
                unsigned value = 0;
                for (int i = 0; i < 4; ++i) {
                    const char h = *at++;
                    const unsigned digit = h >= '0' && h <= '9'   ? (unsigned)(h - '0')
                                           : h >= 'a' && h <= 'f' ? (unsigned)(h - 'a' + 10)
                                           : h >= 'A' && h <= 'F' ? (unsigned)(h - 'A' + 10)
                                                                  : 16u;
                    if (digit == 16u) {
                        return false;
                    }
                    value = value * 16u + digit;
                }
                c = value < 0x80u ? (char)value : '?';
                break;
            }
            case '\0':
                return false;
            default:
                c = escaped;
                break;
            }
        }
        if (used + 1u < out_size) {
            out[used++] = c;
        }
    }
    out[used] = '\0';
    return true;
}

bool BrowseParseCaseLine(const char *line, ReportRecord *out) {
    if (line == NULL || out == NULL || strstr(line, "\"type\":\"case\"") == NULL) {
        return false;
    }
    char suite[REPORT_KEPT_SUITE];
    char name[REPORT_KEPT_NAME];
    char tier[12];
    char result[16];
    char reason[24];
    static char detail[REPORT_KEPT_DETAIL];
    if (!StringField(line, "suite", suite, sizeof(suite)) ||
        !StringField(line, "name", name, sizeof(name)) ||
        !StringField(line, "tier", tier, sizeof(tier)) ||
        !StringField(line, "result", result, sizeof(result))) {
        return false;
    }
    (void)StringField(line, "reason", reason, sizeof(reason));
    (void)StringField(line, "detail", detail, sizeof(detail));
    ReportResult kind;
    if (strcmp(result, "pass") == 0) {
        kind = REPORT_PASS;
    } else if (strcmp(result, "fail") == 0) {
        kind = REPORT_FAIL;
    } else if (strcmp(result, "skip") == 0) {
        kind = strcmp(reason, "crashed") == 0 ? REPORT_CRASHED : REPORT_SKIP;
    } else {
        return false;
    }
    ReportRecordFill(out, suite, name, tier, kind, detail, -1);
    return true;
}

/// The catalogue's case of @p suite named @p name, or UINT32_MAX.
static uint32_t Find(const BrowseCatalog *catalog, const char *suite, const char *name) {
    for (uint32_t i = 0; i < catalog->count; ++i) {
        if (strcmp(catalog->records[i].suite, suite) == 0 &&
            strcmp(catalog->records[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

/// Puts @p record in: over the case of its suite and name, or after the last. Past
/// REPORT_MAX_RECORDS, or with no memory to grow, the case is not kept, as the report's own store
/// keeps none past its limit.
static void Put(BrowseCatalog *catalog, const ReportRecord *record) {
    const uint32_t at = Find(catalog, record->suite, record->name);
    if (at != UINT32_MAX) {
        catalog->records[at] = *record;
        catalog->records[at].picture = -1;
        return;
    }
    if (catalog->count >= REPORT_MAX_RECORDS) {
        return;
    }
    if (catalog->count == catalog->capacity) {
        const uint32_t grown = catalog->capacity == 0u ? 256u : catalog->capacity * 2u;
        ReportRecord *const records =
            (ReportRecord *)realloc(catalog->records, (size_t)grown * sizeof(ReportRecord));
        if (records == NULL) {
            return;
        }
        catalog->records = records;
        catalog->capacity = grown;
    }
    catalog->records[catalog->count] = *record;
    catalog->records[catalog->count].picture = -1;
    ++catalog->count;
}

uint32_t BrowseCatalogRead(BrowseCatalog *catalog, const char *path) {
    if (catalog == NULL || path == NULL) {
        return 0;
    }
    FILE *const file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }
    // A line of the report is REPORT_LINE_MAX at most; a longer one, which only a note can be,
    // is read in pieces, none of them a case.
    static char line[REPORT_LINE_MAX + 2u];
    uint32_t cases = 0;
    ReportRecord record;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (BrowseParseCaseLine(line, &record)) {
            Put(catalog, &record);
            ++cases;
        }
    }
    fclose(file);
    return cases;
}

void BrowseCatalogMerge(BrowseCatalog *catalog, const ReportRecord *records, uint32_t count) {
    for (uint32_t i = 0; catalog != NULL && records != NULL && i < count; ++i) {
        Put(catalog, &records[i]);
    }
}

void BrowseCatalogFree(BrowseCatalog *catalog) {
    if (catalog != NULL) {
        free(catalog->records);
        memset(catalog, 0, sizeof(*catalog));
    }
}

static bool Bit(const uint8_t *bits, uint32_t index) {
    return (bits[index / 8u] >> (index % 8u) & 1u) != 0u;
}

/// Whether suite @p suite (UiBrowseSuite) is in @p pick: whole, or a case of it.
static bool SuitePicked(const UiPick *pick, const UiBrowseInfo *info, uint32_t suite) {
    if (Bit(pick->suites, suite)) {
        return true;
    }
    const char *const name = UiBrowseSuite(suite);
    for (uint32_t i = 0; info != NULL && i < info->catalogue_count && i < UI_MAX_CASES; ++i) {
        if (Bit(pick->cases, i) && strcmp(info->catalogue[i].suite, name) == 0) {
            return true;
        }
    }
    return false;
}

bool BrowseStepRuns(const char *step, const UiPick *pick, const UiBrowseInfo *info) {
    if (step == NULL || pick == NULL) {
        return false;
    }
    if (UiFamilyOf(step) == UI_FAMILY_JUDGE) {
        return true;
    }
    for (uint32_t i = 0; i < UiBrowseSuiteCount(); ++i) {
        if (!SuitePicked(pick, info, i)) {
            continue;
        }
        const UiFamily family = UiBrowseSuiteFamily(i);
        if (strcmp(UiBrowseSuite(i), step) == 0 ||
            (strcmp(step, "net") == 0 && family == UI_FAMILY_NET) ||
            (strcmp(step, "service") == 0 && family == UI_FAMILY_SERVICE)) {
            return true;
        }
    }
    return false;
}

uint32_t BrowseLevels(const UiPick *pick, const UiBrowseInfo *info, const char *suite,
                      uint32_t levels) {
    const uint32_t index = UiBrowseSuiteIndex(suite);
    const uint32_t all = levels >= 32u ? UINT32_MAX : (1u << levels) - 1u;
    if (pick == NULL || index == UINT32_MAX) {
        return 0u;
    }
    if (Bit(pick->suites, index)) {
        return all;
    }
    uint32_t asked = 0;
    for (uint32_t i = 0; info != NULL && i < info->catalogue_count && i < UI_MAX_CASES; ++i) {
        const ReportRecord *const record = &info->catalogue[i];
        unsigned level = 0;
        char after = '\0';
        if (Bit(pick->cases, i) && strcmp(record->suite, suite) == 0 &&
            sscanf(record->name, "level %u%c", &level, &after) == 2 &&
            (after == ',' || after == ':') && level < levels) {
            asked |= 1u << level;
        }
    }
    return asked;
}

size_t BrowseDescribePick(char *out, size_t out_size, const UiPick *pick,
                          const UiBrowseInfo *info) {
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (pick == NULL) {
        return 0;
    }
    size_t used = 0;
    const char *separator = "";
    for (uint32_t i = 0; i < UiBrowseSuiteCount() && used + 1u < out_size; ++i) {
        if (Bit(pick->suites, i)) {
            snprintf(out + used, out_size - used, "%s%s", separator, UiBrowseSuite(i));
            used = strlen(out);
            separator = ", ";
        }
    }
    if (used > 0u && used + 1u < out_size) {
        snprintf(out + used, out_size - used, " whole");
        used = strlen(out);
    }
    for (uint32_t i = 0; i < UiBrowseSuiteCount() && used + 1u < out_size; ++i) {
        if (Bit(pick->suites, i)) {
            continue;
        }
        uint32_t cases = 0;
        const char *const name = UiBrowseSuite(i);
        for (uint32_t c = 0; info != NULL && c < info->catalogue_count && c < UI_MAX_CASES; ++c) {
            cases += Bit(pick->cases, c) && strcmp(info->catalogue[c].suite, name) == 0 ? 1u : 0u;
        }
        if (cases > 0u) {
            snprintf(out + used, out_size - used, "%s%s: %u case%s", used > 0u ? "; " : "", name,
                     (unsigned)cases, cases == 1u ? "" : "s");
            used = strlen(out);
        }
    }
    separator = used > 0u ? "; demos: " : "demos: ";
    for (uint32_t i = 0; info != NULL && i < info->demo_count && i < 32u && used + 1u < out_size;
         ++i) {
        if ((pick->demos >> i & 1u) != 0u) {
            snprintf(out + used, out_size - used, "%s%s", separator, info->demos[i].name);
            used = strlen(out);
            separator = ", ";
        }
    }
    return used;
}

BrowseMusic BrowseMusicOf(const char *suite) {
    static const struct {
        const char *suite;
        BrowseMusic music;
    } MUSIC[] = {
        { "game", { "wimsy", true } },        { "wimsy", { "wimsy", false } },
        { "mandelbrot", { "fractal", false } }, { "julia", { "fractal", false } },
        { "fluid", { "fluid", false } },      { "fluid-alias", { "fluid", false } },
    };
    for (size_t i = 0; suite != NULL && i < sizeof(MUSIC) / sizeof(MUSIC[0]); ++i) {
        if (strcmp(MUSIC[i].suite, suite) == 0) {
            return MUSIC[i].music;
        }
    }
    const BrowseMusic none = { NULL, false };
    return none;
}
