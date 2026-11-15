// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

#include "fontvga.h"
#include "icons.h"
#include "padref.h"
#include "ui.h"

// The screens wear ScarletNX's look (its launcher, its game menu): the VGA font, its YoRHa
// theme's colours, rows as boxes with the focused one inverted, and the keys on a band along the
// bottom. The screens a person chooses on (the start screen, the resume screen, the results)
// take the theme's menu colours; the ones drawn while the run is under way (the running screen,
// listening, the pad and the touch screen) take the colours of its run screen.

// The margin all four sides keep, and the gap between two panels. One number, because the
// layout below divides what is left by three and a gap that differed from the margin would put
// the middle panel off centre.
#define MARGIN 32u

// The cell a glyph advances by: the VGA font's, eight across and sixteen down. Text is drawn at
// that size, a list row of it, with more room on a page of detail, which is read rather than
// scanned; only a prompt meant to be read from a sofa is drawn larger.
#define CELL FONTVGA_WIDTH
#define GLYPH CELL
#define LINE UI_ROW_HEIGHT
#define PAGE_LINE 24u

// The bottom of the frame belongs to the countdown's row and, under it, the band of the keys;
// nothing else is drawn below this far from the edge.
#define FOOT (UI_COUNTDOWN_ROW + UI_FOOT_BAND)

// Lines under the overview's list for the selected group, and under a group's for its case.
#define PREVIEW_LINES 4u
#define DETAIL_LINES 5u

// One set of colours: the theme's, by its names, and the four a result is said in.
typedef struct {
    uint8_t page[4];
    // The page's faint lines: a grid on a menu, scan lines on a run.
    uint8_t ground[4];
    uint8_t ink[4];
    // Secondary words.
    uint8_t soft[4];
    // Rules, and the boxes of rows.
    uint8_t dim[4];
    uint8_t passed[4];
    uint8_t failed[4];
    uint8_t skipped[4];
    uint8_t crashed[4];
    bool grid;
} Look;

// YoRHa's [colors], the launcher's own until another theme is picked (ScarletNX's
// src/gui/themes/yorha/theme.ini): page, the grid its backdrop draws in the dim colour at 72 of
// 255 over the page, ink, soft and dim; a pass in its added colour, a failure in its failure
// colour, a skip in its warning colour, and a crash in a plum of the same weight, which the
// theme has no colour for.
static const Look MENU = {
    { 0xDA, 0xD6, 0xBE, 0xFF }, { 0xCF, 0xCB, 0xB4, 0xFF }, { 0x45, 0x41, 0x3A, 0xFF },
    { 0x8A, 0x86, 0x76, 0xFF }, { 0xB4, 0xB0, 0x9C, 0xFF }, { 0x4A, 0x70, 0x40, 0xFF },
    { 0xA5, 0x47, 0x3C, 0xFF }, { 0x8A, 0x6A, 0x2A, 0xFF }, { 0x6E, 0x55, 0x85, 0xFF },
    true,
};

// YoRHa's [run]: its page with the texture's line of white at 14 of 255 every fourth row, ink,
// soft and dim; a failure and a skip in its failure and warning colours, and a pass and a crash
// in a green and a plum that read on it as the menu's do on theirs.
static const Look RUN = {
    { 0x14, 0x14, 0x16, 0xFF }, { 0x21, 0x21, 0x23, 0xFF }, { 0xE6, 0xE4, 0xDC, 0xFF },
    { 0x8E, 0x8C, 0x86, 0xFF }, { 0x3E, 0x3E, 0x40, 0xFF }, { 0x7D, 0xB0, 0x6A, 0xFF },
    { 0xD2, 0x46, 0x3A, 0xFF }, { 0xD9, 0xB2, 0x4A, 0xFF }, { 0xA9, 0x8B, 0xCB, 0xFF },
    false,
};

// The set the screen being composed is in; each UiCompose* names its own before drawing.
static const Look *g_look = &MENU;

#define BACKGROUND (g_look->page)
#define TEXT (g_look->ink)
#define DIM (g_look->soft)
#define BORDER (g_look->dim)
#define PASSED (g_look->passed)
#define FAILED (g_look->failed)
#define SKIPPED (g_look->skipped)
#define CRASHED (g_look->crashed)

// The grid's squares, as the launcher draws them at the two times scale it opens in on a frame
// of this height.
#define GRID 12u

const uint8_t *UiPixel(const UiFrame *frame, uint32_t x, uint32_t y) {
    if (frame == NULL || frame->pixels == NULL || x >= frame->width || y >= frame->height) {
        return NULL;
    }
    return frame->pixels + (size_t)y * frame->pitch + (size_t)x * 4u;
}

static void PutPixel(const UiFrame *frame, uint32_t x, uint32_t y, const uint8_t colour[4]) {
    if (x >= frame->width || y >= frame->height) {
        return;
    }
    memcpy(frame->pixels + (size_t)y * frame->pitch + (size_t)x * 4u, colour, 4u);
}

static void FillRect(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t width,
                     uint32_t height, const uint8_t colour[4]) {
    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t column = 0; column < width; ++column) {
            PutPixel(frame, x + column, y + row, colour);
        }
    }
}

/// A frame outlined one pixel wide.
static void Outline(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                    const uint8_t colour[4]) {
    if (width == 0u || height == 0u) {
        return;
    }
    FillRect(frame, x, y, width, 1u, colour);
    FillRect(frame, x, y + height - 1u, width, 1u, colour);
    FillRect(frame, x, y, 1u, height, colour);
    FillRect(frame, x + width - 1u, y, 1u, height, colour);
}

/// The page under a rectangle: its colour, and its faint lines, which follow the frame rather
/// than the rectangle so a part of the page drawn again meets the rest.
static void Page(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    for (uint32_t row = y; row < y + height && row < frame->height; ++row) {
        for (uint32_t column = x; column < x + width && column < frame->width; ++column) {
            const bool line =
                g_look->grid ? row % GRID == 0u || column % GRID == 0u : row % 4u == 3u;
            PutPixel(frame, column, row, line ? g_look->ground : g_look->page);
        }
    }
}

/// One code page 437 character at @p scale times the font's cell.
static void DrawGlyph(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t scale,
                      const uint8_t colour[4], unsigned char character) {
    const uint8_t *const rows = FONTVGA[character];
    for (uint32_t row = 0; row < FONTVGA_HEIGHT; ++row) {
        for (uint32_t column = 0; column < FONTVGA_WIDTH; ++column) {
            if ((rows[row] & (0x80u >> column)) == 0u) {
                continue;
            }
            FillRect(frame, x + column * scale, y + row * scale, scale, scale, colour);
        }
    }
}

/// Draws @p text at @p scale times the font's cell. Only printable ASCII: the lines a report
/// carries are ASCII, and a byte past it would draw whatever code page 437 has there.
static void UiText(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t scale,
                   const uint8_t colour[4], const char *text) {
    if (frame == NULL || frame->pixels == NULL || text == NULL || scale == 0u) {
        return;
    }
    for (size_t i = 0; text[i] != '\0'; ++i) {
        const unsigned char character = (unsigned char)text[i];
        if (character > ' ' && character < 0x7Fu) {
            DrawGlyph(frame, x + (uint32_t)i * CELL * scale, y, scale, colour, character);
        }
    }
}

/// Text, handing back where the next word along the line would start.
static uint32_t Say(const UiFrame *frame, uint32_t x, uint32_t y, const uint8_t colour[4],
                    const char *text) {
    UiText(frame, x, y, 1u, colour, text);
    return x + (uint32_t)strlen(text) * GLYPH;
}

static void DrawIcon(const UiFrame *frame, uint32_t x, uint32_t y, Icon icon,
                     const uint8_t colour[4]) {
    for (uint32_t row = 0; row < 16u; ++row) {
        for (uint32_t column = 0; column < 16u; ++column) {
            if ((ICONS[icon][row] & (0x8000u >> column)) != 0u) {
                PutPixel(frame, x + column, y + row, colour);
            }
        }
    }
}

/// Draws @p text broken into lines of @p columns, @p step rows apart, at most @p max_lines of
/// them, and hands back how many it used. Breaks on a space where there is one, because a detail
/// is a sentence. With no frame it draws nothing and only counts.
static uint32_t TextWrapped(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t scale,
                            uint32_t step, const uint8_t colour[4], const char *text,
                            uint32_t columns, uint32_t max_lines) {
    if (text == NULL) {
        return 0;
    }
    // A line of no columns would take nothing and loop for ever.
    if (columns == 0u) {
        columns = 1u;
    }
    const size_t length = strlen(text);
    size_t start = 0;
    uint32_t line = 0;
    while (start < length && line < max_lines) {
        size_t take = length - start < columns ? length - start : columns;
        if (start + take < length) {
            size_t space = take;
            while (space > 0 && text[start + space] != ' ') {
                --space;
            }
            if (space > 0) {
                take = space;
            }
        }
        char buffer[200];
        const size_t room = take + 1u < sizeof(buffer) ? take : sizeof(buffer) - 1u;
        memcpy(buffer, text + start, room);
        buffer[room] = '\0';
        UiText(frame, x, y + line * step, scale, colour, buffer);
        start += take;
        while (start < length && text[start] == ' ') {
            ++start;
        }
        ++line;
    }
    return line;
}

uint32_t UiWrappedLines(const char *text, uint32_t columns) {
    return TextWrapped(NULL, 0, 0, 1, 0, TEXT, text, columns, UINT32_MAX);
}

/// One panel: the picture at @p scale times its own size, nearest neighbour, in a frame.
///
/// A colour picture is drawn over a checkerboard through its alpha, which is the only way a
/// transparent clear tells itself apart from an opaque one on a screen. A picture of bit
/// patterns is drawn opaque from the three bytes below the top one, because those carry the
/// variation in a float and in a depth; the byte left off is still in the files and still in
/// the verdict, so nothing is judged by what the screen leaves out.
static void DrawPanel(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t scale,
                      const PictureRecord *record, UiView which) {
    const uint32_t width = record->width * scale;
    const uint32_t height = record->height * scale;
    // A panel at the very edge of the frame has no room for a border on that side, and a
    // subtraction there would wrap round and draw it on the opposite one.
    if (x > 0u) {
        FillRect(frame, x - 1u, y, 1u, height, BORDER);
    }
    if (y > 0u) {
        FillRect(frame, x, y - 1u, width, 1u, BORDER);
    }
    FillRect(frame, x, y + height, width, 1u, BORDER);
    FillRect(frame, x + width, y, 1u, height, BORDER);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t column = 0; column < width; ++column) {
            const size_t index =
                (size_t)(row / scale) * record->width + (column / scale);
            uint8_t colour[4] = { 0, 0, 0, 0xFF };
            if (which == UI_VIEW_DIFF) {
                PictureDiffColour(record->verdict[index], colour);
            } else {
                const uint8_t *const source =
                    (which == UI_VIEW_EXPECTED ? record->expected : record->got) + index * 4u;
                if (record->format == PICTURE_COLOUR) {
                    const uint8_t check =
                        ((((x + column) >> 3) ^ ((y + row) >> 3)) & 1u) != 0u ? 0x60 : 0x90;
                    const uint32_t alpha = source[3];
                    for (uint32_t channel = 0; channel < 3u; ++channel) {
                        colour[channel] = (uint8_t)((source[channel] * alpha +
                                                     check * (255u - alpha) + 127u) /
                                                    255u);
                    }
                } else {
                    colour[0] = source[2];
                    colour[1] = source[1];
                    colour[2] = source[0];
                }
            }
            PutPixel(frame, x + column, y + row, colour);
        }
    }
}

/// Copies at most @p columns characters of @p text, marking a cut with a tilde so a name that
/// did not fit says so instead of running off the edge.
static void Fit(char *out, size_t out_size, const char *text, uint32_t columns) {
    const size_t room = columns + 1u < out_size ? columns : out_size - 1u;
    const size_t length = strlen(text);
    const size_t take = length < room ? length : room;
    memcpy(out, text, take);
    out[take] = '\0';
    if (length > take && take > 0u) {
        out[take - 1u] = '~';
    }
}

/// How many big characters fit across the frame inside the margins.
static uint32_t Columns(const UiFrame *frame) {
    return frame->width > 2u * MARGIN + GLYPH ? (frame->width - 2u * MARGIN) / GLYPH : 1u;
}

/// Big text cut to what is left of the line from @p x, so nothing runs off the frame's edge.
static uint32_t SayFitted(const UiFrame *frame, uint32_t x, uint32_t y, const uint8_t colour[4],
                          const char *text) {
    const uint32_t right = frame->width > MARGIN ? frame->width - MARGIN : 0u;
    char fitted[200];
    Fit(fitted, sizeof(fitted), text, x < right ? (right - x) / GLYPH : 0u);
    return Say(frame, x, y, colour, fitted);
}

/// The three colours the diff is made of, named on the screen. The person holding the pad is
/// not holding the docs, and red is the most important thing on this display.
static void DrawKey(const UiFrame *frame, uint32_t x, uint32_t y) {
    static const struct {
        PictureVerdict verdict;
        const char *word;
    } KEY[4] = {
        { PICTURE_MATCHED, "accepted" },
        { PICTURE_WRONG, "refused" },
        { PICTURE_UNJUDGED, "not judged" },
        { PICTURE_UNSET, "nobody looked" },
    };
    uint32_t at = x;
    for (uint32_t i = 0; i < 4u; ++i) {
        uint8_t colour[4] = { 0, 0, 0, 0xFF };
        PictureDiffColour((uint8_t)KEY[i].verdict, colour);
        FillRect(frame, at, y, 16u, 16u, colour);
        Outline(frame, at, y, 16u, 16u, TEXT);
        UiText(frame, at + 22u, y, 1, DIM, KEY[i].word);
        at += 22u + (uint32_t)strlen(KEY[i].word) * CELL + 3u * CELL;
    }
}

/// The head's first line, at the top margin's row: a verdict, then what it is about.
#define HEAD_Y 8u

/// The head's second line, which on a menu sits in a box with a bar at its left, as the
/// launcher's tips do under its head.
#define SECOND_Y 30u

static void DrawSecondBox(const UiFrame *frame) {
    if (!g_look->grid) {
        return;
    }
    const uint32_t top = SECOND_Y - 3u;
    const uint32_t height = UI_ROW_HEIGHT + 2u;
    FillRect(frame, MARGIN / 2u, top, frame->width - MARGIN, height, BACKGROUND);
    Outline(frame, MARGIN / 2u, top, frame->width - MARGIN, height, BORDER);
    FillRect(frame, MARGIN / 2u + 4u, top + 3u, 3u, height - 6u, TEXT);
}

uint32_t UiFootRight(uint32_t frame_width) {
    return frame_width > MARGIN + UI_TOUCH_BAR_WIDTH + 2u * CELL + MARGIN
               ? frame_width - MARGIN - UI_TOUCH_BAR_WIDTH - 2u * CELL
               : MARGIN;
}

/// The band along the bottom that the keys sit on: the ink colour across the whole frame, with
/// the launcher's bar at its left, in the page's colour as everything on it is.
static uint32_t FootBand(const UiFrame *frame) {
    const uint32_t top = frame->height - UI_FOOT_BAND;
    FillRect(frame, 0, top, frame->width, UI_FOOT_BAND, TEXT);
    FillRect(frame, MARGIN - 12u, top + 6u, 3u, UI_FOOT_BAND - 12u, BACKGROUND);
    return top;
}

/// A key of the foot: what to press, and what it does on this screen.
typedef struct {
    const char *key;
    const char *what;
} Key;

// A key's button: the text a few pixels inside its frame, and a gap to the next.
#define KEY_PAD 4u
#define KEY_GAP 6u
#define KEY_HEIGHT (FONTVGA_HEIGHT + 4u)

/// The keys on the band, left to right from the margin, each a button as the launcher's foot
/// draws one: framed in the soft colour, the key on it and what it does after the key. Stops
/// short of @p right, where the touch bar starts; a key that does not fit whole is cut and says
/// so with the tilde.
static void DrawKeys(const UiFrame *frame, const Key *keys, uint32_t count, uint32_t right) {
    const uint32_t top = FootBand(frame);
    const uint32_t y = top + (UI_FOOT_BAND - KEY_HEIGHT) / 2u;
    uint32_t x = MARGIN;
    for (uint32_t i = 0; i < count && x + 2u * KEY_PAD + 2u * CELL < right; ++i) {
        const uint32_t room = (right - x - 2u * KEY_PAD) / CELL;
        char key[64];
        char what[160];
        Fit(key, sizeof(key), keys[i].key, room);
        const uint32_t key_columns = (uint32_t)strlen(key);
        Fit(what, sizeof(what), keys[i].what,
            room > key_columns + 1u ? room - key_columns - 1u : 0u);
        const uint32_t what_columns = (uint32_t)strlen(what);
        const uint32_t columns = key_columns + (what_columns > 0u ? 1u + what_columns : 0u);
        const uint32_t width = columns * CELL + 2u * KEY_PAD;
        FillRect(frame, x + KEY_PAD - 1u, y + 2u, key_columns * CELL + 2u, FONTVGA_HEIGHT, DIM);
        Outline(frame, x, y, width, KEY_HEIGHT, DIM);
        const uint32_t after = Say(frame, x + KEY_PAD, y + 2u, BACKGROUND, key);
        Say(frame, after + CELL, y + 2u, BACKGROUND, what);
        x += width + KEY_GAP;
    }
}

/// A screen's sentence on the band where it has no keys to name.
static void DrawFootNote(const UiFrame *frame, const char *text, uint32_t right) {
    const uint32_t top = FootBand(frame);
    char fitted[200];
    Fit(fitted, sizeof(fitted), text, right > MARGIN ? (right - MARGIN) / CELL : 0u);
    Say(frame, MARGIN, top + (UI_FOOT_BAND - FONTVGA_HEIGHT) / 2u, BACKGROUND, fitted);
}

/// Where the keys of a screen whose touch bar has @p boxes boxes must stop.
static uint32_t KeysRight(const UiFrame *frame, uint32_t boxes) {
    const UiRect box = UiTouchBox(frame->width, frame->height, boxes, 0u);
    return box.width > 0u ? box.x - 2u * CELL : frame->width - MARGIN;
}

UiRect UiTouchBox(uint32_t frame_width, uint32_t frame_height, uint32_t count, uint32_t index) {
    const UiRect none = { 0, 0, 0, 0 };
    if (frame_width < UI_MIN_WIDTH || frame_height < UI_MIN_HEIGHT || count == 0u ||
        count > UI_TOUCH_BOXES || index >= count) {
        return none;
    }
    const uint32_t span = count * UI_TOUCH_BOX_WIDTH + (count - 1u) * UI_TOUCH_BOX_GAP;
    // On the foot's band, at its right, where the keys stop short of it.
    const uint32_t x =
        frame_width - MARGIN - span + index * (UI_TOUCH_BOX_WIDTH + UI_TOUCH_BOX_GAP);
    const UiRect box = { x, frame_height - UI_TOUCH_BOX_HEIGHT - 2u, UI_TOUCH_BOX_WIDTH,
                         UI_TOUCH_BOX_HEIGHT };
    return box;
}

/// The bar's boxes with their words, which say what a tap on each does: buttons of the band,
/// filled as the key of a button is.
static void DrawTouchBar(const UiFrame *frame, const char *const *labels, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        const UiRect box = UiTouchBox(frame->width, frame->height, count, i);
        if (box.width == 0u) {
            return;
        }
        FillRect(frame, box.x, box.y, box.width, box.height, DIM);
        const uint32_t text = (uint32_t)strlen(labels[i]) * GLYPH;
        Say(frame, box.x + (box.width > text ? (box.width - text) / 2u : 0u),
            box.y + (box.height - FONTVGA_HEIGHT) / 2u, BACKGROUND, labels[i]);
    }
}

static bool Inside(UiRect rect, uint32_t x, uint32_t y) {
    return rect.width > 0u && x >= rect.x && x - rect.x < rect.width && y >= rect.y &&
           y - rect.y < rect.height;
}

/// The box of a bar of @p count both ends of @p tap are in, or UINT32_MAX.
static uint32_t TapBox(const UiTap *tap, uint32_t width, uint32_t height, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        const UiRect box = UiTouchBox(width, height, count, i);
        if (Inside(box, tap->x0, tap->y0) && Inside(box, tap->x1, tap->y1)) {
            return i;
        }
    }
    return UINT32_MAX;
}

/// The row of a list of @p rows, @p step apart from @p top, under @p x, @p y, or UINT32_MAX. Its
/// width is the band the cursor's row is drawn in.
static uint32_t RowAt(uint32_t width, uint32_t top, uint32_t step, uint32_t rows, uint32_t x,
                      uint32_t y) {
    if (x < MARGIN / 2u || x >= width - MARGIN / 2u || y < top) {
        return UINT32_MAX;
    }
    const uint32_t row = (y - top) / step;
    return row < rows ? row : UINT32_MAX;
}

/// The one row both ends of @p tap are on, or UINT32_MAX.
static uint32_t TapRow(const UiTap *tap, uint32_t width, uint32_t top, uint32_t step,
                       uint32_t rows) {
    const uint32_t row = RowAt(width, top, step, rows, tap->x0, tap->y0);
    return row == RowAt(width, top, step, rows, tap->x1, tap->y1) ? row : UINT32_MAX;
}

void UiTouchReset(UiTouch *touch) {
    if (touch != NULL) {
        memset(touch, 0, sizeof(*touch));
        touch->down = true;
    }
}

UiTouchEvent UiTouchStep(UiTouch *touch, uint32_t count, uint32_t x, uint32_t y,
                         uint32_t frame_index, UiTap *tap) {
    if (touch == NULL) {
        return UI_TOUCH_NONE;
    }
    if (count == 0u) {
        // Lifted: the release frame carries no position, so the tap ends where it was last seen.
        const bool tapped = touch->down && touch->armed;
        if (tapped && tap != NULL) {
            *tap = touch->tap;
        }
        touch->down = false;
        touch->armed = false;
        return tapped ? UI_TOUCH_TAP : UI_TOUCH_NONE;
    }
    if (!touch->down) {
        touch->down = true;
        touch->armed = count == 1u && frame_index >= UI_SETTLING_FRAMES;
        const UiTap at = { x, y, x, y };
        touch->tap = at;
        return touch->armed ? UI_TOUCH_BEGAN : UI_TOUCH_NONE;
    }
    if (count > 1u) {
        touch->armed = false;
    } else {
        touch->tap.x1 = x;
        touch->tap.y1 = y;
    }
    return UI_TOUCH_NONE;
}

static const char *ViewLabel(UiView view) {
    switch (view) {
    case UI_VIEW_GOT:
        return "drawn";
    case UI_VIEW_EXPECTED:
        return "expected";
    case UI_VIEW_DIFF:
        return "judged";
    case UI_VIEW_ALL:
        break;
    }
    return "all three";
}

// Which family each suite belongs to, by the name its lines carry. The judge first, because
// nothing after it can be believed without it.
static const struct {
    const char *suite;
    UiFamily family;
} FAMILIES[] = {
    { "selfcheck", UI_FAMILY_JUDGE }, { "support", UI_FAMILY_JUDGE },
    { "reference", UI_FAMILY_JUDGE }, { "cpu", UI_FAMILY_CPU },
    { "pad", UI_FAMILY_PAD },         { "touch", UI_FAMILY_PAD },
    { "clear", UI_FAMILY_GRAPHICS },  { "format", UI_FAMILY_GRAPHICS },
    { "raster", UI_FAMILY_GRAPHICS }, { "intmath", UI_FAMILY_GRAPHICS },
    { "blend", UI_FAMILY_GRAPHICS },  { "depth", UI_FAMILY_GRAPHICS },
    { "fixed", UI_FAMILY_GRAPHICS },
    { "stencil", UI_FAMILY_GRAPHICS }, { "lod", UI_FAMILY_GRAPHICS },
    { "sampling", UI_FAMILY_GRAPHICS },
    { "coverage", UI_FAMILY_GRAPHICS }, { "msaa", UI_FAMILY_GRAPHICS },
    { "texture", UI_FAMILY_GRAPHICS }, { "coherence", UI_FAMILY_GRAPHICS },
    { "fmath", UI_FAMILY_GRAPHICS },  { "order", UI_FAMILY_GRAPHICS },
    { "queue", UI_FAMILY_GRAPHICS },  { "storage", UI_FAMILY_GRAPHICS },
    { "index", UI_FAMILY_GRAPHICS },  { "topology", UI_FAMILY_GRAPHICS },
    { "vertex", UI_FAMILY_GRAPHICS },
    { "instance", UI_FAMILY_GRAPHICS },
    { "bindless", UI_FAMILY_GRAPHICS }, { "astc", UI_FAMILY_GRAPHICS },
    { "astcimage", UI_FAMILY_GRAPHICS },
    { "eac", UI_FAMILY_GRAPHICS },    { "etc2", UI_FAMILY_GRAPHICS },
    { "bcn", UI_FAMILY_GRAPHICS },    { "bcnimage", UI_FAMILY_GRAPHICS },
    { "zeta", UI_FAMILY_GRAPHICS },
    { "swizzle", UI_FAMILY_GRAPHICS },
    { "texel", UI_FAMILY_GRAPHICS },  { "stream", UI_FAMILY_GRAPHICS },
    { "fill", UI_FAMILY_GRAPHICS },   { "ring", UI_FAMILY_GRAPHICS },
    { "occlusion", UI_FAMILY_GRAPHICS }, { "drawtex", UI_FAMILY_GRAPHICS },
    { "geometry", UI_FAMILY_GRAPHICS }, { "xfb", UI_FAMILY_GRAPHICS },
    { "query", UI_FAMILY_GRAPHICS },  { "dma", UI_FAMILY_GRAPHICS },
    { "blit2d", UI_FAMILY_GRAPHICS }, { "copy", UI_FAMILY_GRAPHICS },
    { "tex8k", UI_FAMILY_GRAPHICS },
    { "fs", UI_FAMILY_FS },           { "sched", UI_FAMILY_KERNEL },
    { "svc", UI_FAMILY_KERNEL },
    { "binder", UI_FAMILY_DISPLAY },
    { "vic", UI_FAMILY_VIDEO },       { "nvdec", UI_FAMILY_VIDEO },
    { "audio", UI_FAMILY_AUDIO },
    { "hwopus", UI_FAMILY_SERVICE },  { "am", UI_FAMILY_SERVICE },
    { "fsp", UI_FAMILY_SERVICE },     { "account", UI_FAMILY_SERVICE },
    { "ldn", UI_FAMILY_SERVICE },     { "set", UI_FAMILY_SERVICE },
    { "csrng", UI_FAMILY_SERVICE },   { "apm", UI_FAMILY_SERVICE },
    { "nfp", UI_FAMILY_SERVICE },     { "aoc", UI_FAMILY_SERVICE },
    { "time", UI_FAMILY_SERVICE },
    { "pl", UI_FAMILY_SERVICE },
    { "profile", UI_FAMILY_SERVICE },
    { "am-state", UI_FAMILY_SERVICE },
    { "am-self", UI_FAMILY_SERVICE },
    { "am-functions", UI_FAMILY_SERVICE },
    { "auddev", UI_FAMILY_SERVICE },
    { "fsdir", UI_FAMILY_SERVICE },
    { "am-audio", UI_FAMILY_SERVICE },
    { "am-channel", UI_FAMILY_SERVICE },
    { "am-storage", UI_FAMILY_SERVICE },
    { "mii", UI_FAMILY_SERVICE },
    { "am-mode", UI_FAMILY_SERVICE },
    { "bsd-local", UI_FAMILY_SERVICE },
    { "ipc", UI_FAMILY_SERVICE },
    { "hid", UI_FAMILY_SERVICE },
    { "time-snap", UI_FAMILY_SERVICE },
    { "vi", UI_FAMILY_SERVICE },
    { "renderer", UI_FAMILY_SERVICE },
    { "ngc", UI_FAMILY_SERVICE },
    { "am-lock", UI_FAMILY_SERVICE },
    { "set-sys", UI_FAMILY_SERVICE },
    { "time-zone", UI_FAMILY_SERVICE },
    { "time-perm", UI_FAMILY_SERVICE },
    { "audout", UI_FAMILY_SERVICE },
    { "audin", UI_FAMILY_SERVICE },
    { "pctl", UI_FAMILY_SERVICE },
    { "fs-file", UI_FAMILY_SERVICE },
    { "acc-app", UI_FAMILY_SERVICE },
    { "sdk", UI_FAMILY_SERVICE },
    { "nifm", UI_FAMILY_NET },
    { "dns", UI_FAMILY_NET },         { "bsd", UI_FAMILY_NET },
    { "ssl", UI_FAMILY_NET },         { "acc", UI_FAMILY_NET },
    { "friend", UI_FAMILY_NET },
    { "libc", UI_FAMILY_LIBC },       { "system", UI_FAMILY_LIBC },
    { "socket", UI_FAMILY_LIBC },
    { "game", UI_FAMILY_GAME },       { "wimsy", UI_FAMILY_GAME },
    { "mandelbrot", UI_FAMILY_DEMO }, { "julia", UI_FAMILY_DEMO },
    { "fluid", UI_FAMILY_DEMO },       { "fluid-alias", UI_FAMILY_DEMO },
    { "music", UI_FAMILY_MUSIC },
    { "nvn", UI_FAMILY_NVN },
};

UiFamily UiFamilyOf(const char *suite) {
    for (size_t i = 0; suite != NULL && i < sizeof(FAMILIES) / sizeof(FAMILIES[0]); ++i) {
        if (strcmp(FAMILIES[i].suite, suite) == 0) {
            return FAMILIES[i].family;
        }
    }
    return UI_FAMILY_OTHER;
}

const char *UiFamilyName(UiFamily family) {
    switch (family) {
    case UI_FAMILY_JUDGE:
        return "judge";
    case UI_FAMILY_CPU:
        return "cpu";
    case UI_FAMILY_PAD:
        return "pad";
    case UI_FAMILY_GRAPHICS:
        return "graphics";
    case UI_FAMILY_NVN:
        return "nvn";
    case UI_FAMILY_FS:
        return "fs";
    case UI_FAMILY_KERNEL:
        return "kernel";
    case UI_FAMILY_DISPLAY:
        return "display";
    case UI_FAMILY_VIDEO:
        return "video";
    case UI_FAMILY_AUDIO:
        return "audio";
    case UI_FAMILY_SERVICE:
        return "service";
    case UI_FAMILY_NET:
        return "net";
    case UI_FAMILY_GAME:
        return "game";
    case UI_FAMILY_LIBC:
        return "libc";
    case UI_FAMILY_DEMO:
        return "demo";
    case UI_FAMILY_MUSIC:
        return "music";
    case UI_FAMILY_OTHER:
    case UI_FAMILY_COUNT:
        break;
    }
    return "other";
}

/// A case that asked nothing: skipped for any reason, crashed included.
static bool Skipped(ReportResult result) {
    return result == REPORT_SKIP || result == REPORT_CRASHED;
}

_Static_assert(UI_MAX_GROUPS < 0xFFu, "0xFF marks a case with no group");

void UiRunIndex(UiRun *run, const ReportRecord *records, uint32_t count, uint32_t reported) {
    if (run == NULL) {
        return;
    }
    memset(run, 0, sizeof(*run));
    if (records == NULL) {
        count = 0u;
    }
    run->records = records;
    run->count = count;
    run->reported = reported > count ? reported : count;

    // Groups are made in the order the run first reached them; 0xFF marks a case that found no
    // room, which is counted rather than shown under somebody else's suite.
    uint8_t group_of[UI_MAX_CASES];
    const uint32_t indexed = count < UI_MAX_CASES ? count : UI_MAX_CASES;
    run->unplaced = count - indexed;
    for (uint32_t i = 0; i < indexed; ++i) {
        const ReportRecord *const record = &records[i];
        uint32_t g = 0;
        while (g < run->group_count && strcmp(run->groups[g].suite, record->suite) != 0) {
            ++g;
        }
        if (g == run->group_count) {
            if (run->group_count == UI_MAX_GROUPS) {
                group_of[i] = 0xFF;
                ++run->unplaced;
                continue;
            }
            run->groups[g].suite = record->suite;
            run->groups[g].family = UiFamilyOf(record->suite);
            ++run->group_count;
        }
        group_of[i] = (uint8_t)g;
        UiGroup *const group = &run->groups[g];
        ++group->count;
        if (record->result == REPORT_FAIL) {
            ++group->failed;
            ++run->failed;
        } else if (record->result == REPORT_CRASHED) {
            ++group->crashed;
            ++run->crashed;
        } else if (record->result == REPORT_SKIP) {
            ++group->skipped;
            ++run->skipped;
        } else {
            ++group->passed;
            ++run->passed;
        }
    }

    // Stable, so a family's groups keep the order the run reached them in.
    UiGroup sorted[UI_MAX_GROUPS];
    uint8_t position[UI_MAX_GROUPS];
    uint32_t placed = 0;
    for (uint32_t family = 0; family < UI_FAMILY_COUNT; ++family) {
        for (uint32_t g = 0; g < run->group_count; ++g) {
            if (run->groups[g].family == (UiFamily)family) {
                position[g] = (uint8_t)placed;
                sorted[placed++] = run->groups[g];
            }
        }
    }
    uint32_t cursor[UI_MAX_GROUPS];
    uint32_t first = 0;
    for (uint32_t g = 0; g < run->group_count; ++g) {
        run->groups[g] = sorted[g];
        run->groups[g].first = first;
        cursor[g] = first;
        first += sorted[g].count;
    }
    for (uint32_t i = 0; i < indexed; ++i) {
        if (group_of[i] != 0xFF) {
            run->order[cursor[position[group_of[i]]]++] = (uint16_t)i;
        }
    }
}

const ReportRecord *UiCaseAt(const UiRun *run, uint32_t group, uint32_t index) {
    if (run == NULL || group >= run->group_count || index >= run->groups[group].count) {
        return NULL;
    }
    return &run->records[run->order[run->groups[group].first + index]];
}

const ReportRecord *UiSelected(const UiRun *run, UiState state) {
    return state.level == UI_LEVEL_OVERVIEW ? NULL : UiCaseAt(run, state.group, state.index);
}

int32_t UiOpenPicture(const UiRun *run, UiState state) {
    const ReportRecord *const selected = UiSelected(run, state);
    return state.level == UI_LEVEL_CASE && selected != NULL ? selected->picture : -1;
}

/// One step along a list of @p count, coming round at either end.
static uint32_t Around(uint32_t at, int step, uint32_t count) {
    return step > 0 ? (at + 1u) % count : (at + count - 1u) % count;
}

static bool CaseFailed(const UiRun *run, uint32_t group, uint32_t index) {
    const ReportRecord *const record = UiCaseAt(run, group, index);
    return record != NULL && record->result == REPORT_FAIL;
}

/// Where a group is entered: its first failure, or its first case.
static uint32_t FirstFailureIn(const UiRun *run, uint32_t group) {
    for (uint32_t i = 0; i < run->groups[group].count; ++i) {
        if (CaseFailed(run, group, i)) {
            return i;
        }
    }
    return 0u;
}

/// The next failing case of @p group in @p step's direction, coming round once, or @p from when
/// there is none. Coming round rather than stopping, because a group with two failures out of a
/// hundred is exactly the group this is for.
static uint32_t SeekFailingCase(const UiRun *run, uint32_t group, uint32_t from, int step) {
    const uint32_t count = run->groups[group].count;
    uint32_t at = from;
    for (uint32_t tried = 0; tried < count; ++tried) {
        at = Around(at, step, count);
        if (CaseFailed(run, group, at)) {
            return at;
        }
    }
    return from;
}

static uint32_t SeekFailingGroup(const UiRun *run, uint32_t from, int step) {
    uint32_t at = from;
    for (uint32_t tried = 0; tried < run->group_count; ++tried) {
        at = Around(at, step, run->group_count);
        if (run->groups[at].failed > 0u) {
            return at;
        }
    }
    return from;
}

/// The first group of the family before or after the one @p from is in, coming round.
static uint32_t SeekFamily(const UiRun *run, uint32_t from, int step) {
    const UiGroup *const groups = run->groups;
    uint32_t start = from;
    while (start > 0u && groups[start - 1u].family == groups[from].family) {
        --start;
    }
    uint32_t at = start;
    if (step > 0) {
        do {
            at = Around(at, 1, run->group_count);
        } while (at != start && groups[at].family == groups[from].family);
    } else {
        at = Around(start, -1, run->group_count);
        while (at > 0u && groups[at - 1u].family == groups[at].family) {
            --at;
        }
    }
    return at;
}

UiState UiStart(const UiRun *run) {
    UiState state;
    memset(&state, 0, sizeof(state));
    state.level = UI_LEVEL_OVERVIEW;
    state.view = UI_VIEW_ALL;
    state.changed = true;
    for (uint32_t g = 0; run != NULL && g < run->group_count; ++g) {
        if (run->groups[g].failed > 0u) {
            state.group = g;
            break;
        }
    }
    return state;
}

void UiFrameDrawn(UiState *state) {
    if (state != NULL) {
        state->changed = false;
    }
}

static void StepOverview(UiState *next, UiInput input, const UiRun *run) {
    switch (input) {
    case UI_INPUT_UP:
        next->group = Around(next->group, -1, run->group_count);
        break;
    case UI_INPUT_DOWN:
        next->group = Around(next->group, 1, run->group_count);
        break;
    case UI_INPUT_LEFT:
        next->group = SeekFamily(run, next->group, -1);
        break;
    case UI_INPUT_RIGHT:
        next->group = SeekFamily(run, next->group, 1);
        break;
    case UI_INPUT_NEXT_FAILURE:
        next->group = SeekFailingGroup(run, next->group, 1);
        break;
    case UI_INPUT_PREVIOUS_FAILURE:
        next->group = SeekFailingGroup(run, next->group, -1);
        break;
    case UI_INPUT_OPEN:
        next->level = UI_LEVEL_GROUP;
        next->index = FirstFailureIn(run, next->group);
        break;
    default:
        break;
    }
}

static void StepGroup(UiState *next, UiInput input, const UiRun *run) {
    const uint32_t count = run->groups[next->group].count;
    switch (input) {
    case UI_INPUT_UP:
        next->index = Around(next->index, -1, count);
        break;
    case UI_INPUT_DOWN:
        next->index = Around(next->index, 1, count);
        break;
    case UI_INPUT_LEFT:
    case UI_INPUT_RIGHT:
        next->group = Around(next->group, input == UI_INPUT_RIGHT ? 1 : -1, run->group_count);
        next->index = FirstFailureIn(run, next->group);
        break;
    case UI_INPUT_NEXT_FAILURE:
        next->index = SeekFailingCase(run, next->group, next->index, 1);
        break;
    case UI_INPUT_PREVIOUS_FAILURE:
        next->index = SeekFailingCase(run, next->group, next->index, -1);
        break;
    case UI_INPUT_OPEN:
        next->level = UI_LEVEL_CASE;
        break;
    case UI_INPUT_BACK:
        next->level = UI_LEVEL_OVERVIEW;
        break;
    default:
        break;
    }
}

static void StepCase(UiState *next, UiInput input, const UiRun *run) {
    const uint32_t count = run->groups[next->group].count;
    switch (input) {
    case UI_INPUT_UP:
    case UI_INPUT_LEFT:
        next->index = Around(next->index, -1, count);
        break;
    case UI_INPUT_DOWN:
    case UI_INPUT_RIGHT:
        next->index = Around(next->index, 1, count);
        break;
    case UI_INPUT_NEXT_FAILURE:
        next->index = SeekFailingCase(run, next->group, next->index, 1);
        break;
    case UI_INPUT_PREVIOUS_FAILURE:
        next->index = SeekFailingCase(run, next->group, next->index, -1);
        break;
    case UI_INPUT_CYCLE_VIEW: {
        // Only where there is something to cycle: on a case with no pictures the press would
        // change a view nobody can see, and the one it opened next would not be the one chosen.
        const ReportRecord *const record = UiCaseAt(run, next->group, next->index);
        if (record != NULL && record->picture >= 0) {
            next->view = (UiView)((next->view + 1) % 4);
        }
        break;
    }
    case UI_INPUT_BACK:
        next->level = UI_LEVEL_GROUP;
        break;
    default:
        break;
    }
}

UiState UiStep(UiState state, UiInput input, const UiRun *run) {
    UiState next = state;
    if (input == UI_INPUT_QUIT) {
        next.quit = true;
    } else if (input == UI_INPUT_AGAIN) {
        // Only where the browser began the run: anywhere else there is nothing to run again.
        next.again = next.browser && next.repeatable;
    } else if (input == UI_INPUT_BACK && next.browser &&
               (next.level == UI_LEVEL_OVERVIEW || run == NULL || run->group_count == 0u)) {
        next.back = true;
    } else if (run != NULL && run->group_count > 0u) {
        if (next.group >= run->group_count) {
            next.group = run->group_count - 1u;
        }
        if (next.index >= run->groups[next.group].count) {
            next.index = run->groups[next.group].count - 1u;
        }
        switch (next.level) {
        case UI_LEVEL_OVERVIEW:
            StepOverview(&next, input, run);
            break;
        case UI_LEVEL_GROUP:
            StepGroup(&next, input, run);
            break;
        case UI_LEVEL_CASE:
            StepCase(&next, input, run);
            break;
        }
    }
    next.changed = next.changed || next.level != state.level || next.group != state.group ||
                   next.index != state.index || next.view != state.view ||
                   next.quit != state.quit || next.back != state.back ||
                   next.again != state.again;
    return next;
}

uint32_t UiResultsKeys(const UiRun *run, UiState state, UiKey *keys) {
    uint32_t count = 0;
    const UiKey quit = { state.browser ? "Minus" : "Plus", "quit", UI_INPUT_QUIT };
    const UiKey again = { "Y", "again", UI_INPUT_AGAIN };
    if (run == NULL || run->group_count == 0u) {
        if (state.browser) {
            const UiKey tabs = { "L/R", "tabs", UI_INPUT_NONE };
            keys[count++] = tabs;
        }
        keys[count++] = quit;
        return count;
    }
    const UiKey failures = { "ZL/ZR", "failures", UI_INPUT_NONE };
    const UiKey back = { "B", state.level == UI_LEVEL_OVERVIEW ? "suites" : "back",
                         UI_INPUT_BACK };
    if (state.level == UI_LEVEL_OVERVIEW) {
        const UiKey move = { "Up/Down", "group", UI_INPUT_NONE };
        const UiKey open = { "A", "open", UI_INPUT_OPEN };
        keys[count++] = move;
        keys[count++] = open;
        keys[count++] = failures;
        if (state.browser) {
            const UiKey tabs = { "L/R", "tabs", UI_INPUT_NONE };
            keys[count++] = tabs;
            keys[count++] = back;
        }
    } else if (state.level == UI_LEVEL_GROUP) {
        const UiKey move = { "Up/Down", "case", UI_INPUT_NONE };
        const UiKey open = { "A", "open", UI_INPUT_OPEN };
        keys[count++] = move;
        keys[count++] = open;
        keys[count++] = failures;
        keys[count++] = back;
    } else {
        const UiKey previous = { "Left", "case", UI_INPUT_LEFT };
        const UiKey next = { "Right", "case", UI_INPUT_RIGHT };
        keys[count++] = previous;
        keys[count++] = next;
        const ReportRecord *const record = UiCaseAt(run, state.group, state.index);
        if (record != NULL && record->picture >= 0) {
            const UiKey view = { "X", "view", UI_INPUT_CYCLE_VIEW };
            keys[count++] = view;
        }
        keys[count++] = back;
    }
    if (state.browser && state.repeatable) {
        keys[count++] = again;
    }
    keys[count++] = quit;
    return count;
}

/// The group on overview row @p line, or UINT32_MAX for a family's heading.
static uint32_t GroupOnRow(const UiRun *run, uint32_t line) {
    for (uint32_t g = 0; g < run->group_count; ++g) {
        if (UiOverviewRow(run, g) == line) {
            return g;
        }
    }
    return UINT32_MAX;
}

UiState UiTapStep(UiState state, const UiTap *tap, const UiRun *run, uint32_t frame_width,
                  uint32_t frame_height) {
    if (tap == NULL || frame_width < UI_MIN_WIDTH || frame_height < UI_MIN_HEIGHT) {
        return state;
    }
    UiKey keys[UI_FOOT_KEYS];
    const uint32_t count = UiResultsKeys(run, state, keys);
    const uint32_t key = UiFootKeyAt(tap, frame_width, frame_height, keys, count);
    if (key != UINT32_MAX) {
        return keys[key].input != UI_INPUT_NONE ? UiStep(state, (UiInput)keys[key].input, run)
                                                : state;
    }
    if (run == NULL || run->group_count == 0u) {
        return state;
    }
    // The list as the screen drew it, which is from the cursor alone.
    UiState next = state;
    if (next.group >= run->group_count) {
        next.group = run->group_count - 1u;
    }
    const uint32_t cases = run->groups[next.group].count;
    if (next.index >= cases) {
        next.index = cases - 1u;
    }
    if (next.level == UI_LEVEL_OVERVIEW) {
        const uint32_t rows = UiListRows(UI_LEVEL_OVERVIEW, frame_height);
        const uint32_t total = UiOverviewRow(run, run->group_count - 1u) + 1u;
        const uint32_t first = UiListFirst(UiOverviewRow(run, next.group), total, rows);
        const uint32_t shown = total - first < rows ? total - first : rows;
        const uint32_t row = TapRow(tap, frame_width, UI_LIST_TOP, UI_ROW_HEIGHT, shown);
        const uint32_t group = row != UINT32_MAX ? GroupOnRow(run, first + row) : UINT32_MAX;
        if (group == UINT32_MAX) {
            return state;
        }
        if (group == next.group) {
            return UiStep(next, UI_INPUT_OPEN, run);
        }
        next.group = group;
        next.changed = true;
        return next;
    }
    if (next.level == UI_LEVEL_GROUP) {
        const uint32_t rows = UiListRows(UI_LEVEL_GROUP, frame_height);
        const uint32_t first = UiListFirst(next.index, cases, rows);
        const uint32_t shown = cases - first < rows ? cases - first : rows;
        const uint32_t row = TapRow(tap, frame_width, UI_LIST_TOP, UI_ROW_HEIGHT, shown);
        if (row == UINT32_MAX) {
            return state;
        }
        if (first + row == next.index) {
            return UiStep(next, UI_INPUT_OPEN, run);
        }
        next.index = first + row;
        next.changed = true;
        return next;
    }
    return state;
}

UiInput UiInputFromButtons(uint32_t buttons, uint32_t frame_index) {
    if (frame_index < UI_SETTLING_FRAMES) {
        return UI_INPUT_NONE;
    }
    // Leaving first, then going back, so that a mask with everything in it leaves, and one with
    // back and a direction in it does not carry the person somewhere they were leaving.
    static const struct {
        uint32_t button;
        UiInput input;
    } ORDER[] = {
        { UI_BUTTON_QUIT, UI_INPUT_QUIT },
        { UI_BUTTON_RUN, UI_INPUT_QUIT },
        { UI_BUTTON_BACK, UI_INPUT_BACK },
        { UI_BUTTON_OPEN, UI_INPUT_OPEN },
        { UI_BUTTON_CYCLE_VIEW, UI_INPUT_CYCLE_VIEW },
        { UI_BUTTON_AGAIN, UI_INPUT_AGAIN },
        { UI_BUTTON_NEXT_FAILURE, UI_INPUT_NEXT_FAILURE },
        { UI_BUTTON_PREVIOUS_FAILURE, UI_INPUT_PREVIOUS_FAILURE },
        { UI_BUTTON_UP, UI_INPUT_UP },
        { UI_BUTTON_DOWN, UI_INPUT_DOWN },
        { UI_BUTTON_LEFT, UI_INPUT_LEFT },
        { UI_BUTTON_RIGHT, UI_INPUT_RIGHT },
    };
    for (size_t i = 0; i < sizeof(ORDER) / sizeof(ORDER[0]); ++i) {
        if ((buttons & ORDER[i].button) != 0u) {
            return ORDER[i].input;
        }
    }
    return UI_INPUT_NONE;
}

uint32_t UiRepeatButtons(UiRepeat *repeat, uint32_t held, uint32_t down, uint32_t frame_index) {
    const uint32_t directions = held & UI_BUTTONS_REPEATING;
    if (repeat == NULL) {
        return down;
    }
    if (frame_index < UI_SETTLING_FRAMES) {
        repeat->held = 0u;
        repeat->frames = 0u;
        return down;
    }
    // Only what this frame pressed: a direction held from before, or since before the pad
    // settled, is not what the person just asked for.
    if ((down & UI_BUTTONS_REPEATING) != 0u) {
        repeat->held = down & UI_BUTTONS_REPEATING;
        repeat->frames = 0u;
        return down;
    }
    if (repeat->held == 0u || (directions & repeat->held) != repeat->held) {
        repeat->held = 0u;
        repeat->frames = 0u;
        return down;
    }
    ++repeat->frames;
    if (repeat->frames >= UI_REPEAT_DELAY_FRAMES &&
        (repeat->frames - UI_REPEAT_DELAY_FRAMES) % UI_REPEAT_EVERY_FRAMES == 0u) {
        return down | repeat->held;
    }
    return down;
}

uint32_t UiListRows(UiLevel level, uint32_t height) {
    const uint32_t lines = level == UI_LEVEL_OVERVIEW ? PREVIEW_LINES : DETAIL_LINES;
    // The pane under the list with its lines, a gap with a rule in it, and the foot.
    const uint32_t bottom = UI_BODY_BOTTOM(height);
    const uint32_t under = lines * LINE + 16u;
    if (bottom < UI_LIST_TOP + under + UI_ROW_HEIGHT) {
        return 1u;
    }
    return (bottom - UI_LIST_TOP - under) / UI_ROW_HEIGHT;
}

uint32_t UiListFirst(uint32_t selected, uint32_t total, uint32_t rows) {
    if (rows == 0u || total <= rows) {
        return 0u;
    }
    const uint32_t half = rows / 2u;
    const uint32_t first = selected > half ? selected - half : 0u;
    return first > total - rows ? total - rows : first;
}

uint32_t UiOverviewRow(const UiRun *run, uint32_t group) {
    uint32_t row = 0;
    for (uint32_t g = 0; run != NULL && g <= group && g < run->group_count; ++g) {
        if (g == 0u || run->groups[g].family != run->groups[g - 1u].family) {
            ++row;
        }
        if (g < group) {
            ++row;
        }
    }
    return row;
}

static const uint8_t *ResultColour(ReportResult result) {
    return result == REPORT_FAIL      ? FAILED
           : result == REPORT_CRASHED ? CRASHED
           : result == REPORT_SKIP    ? SKIPPED
                                      : PASSED;
}

/// What to say where a case's line carried no detail, which is only unremarkable for a pass.
static const char *NoDetail(ReportResult result) {
    return result == REPORT_FAIL   ? "no detail: the line says only that it failed"
           : Skipped(result)       ? "no detail: the line gives no reason"
                                   : "no detail: the case passed";
}

static const char *ResultWord(ReportResult result) {
    return result == REPORT_FAIL ? "FAIL" : Skipped(result) ? "SKIP" : "PASS";
}

// ---- The launcher's look -------------------------------------------------------------------
//
// ScarletNX's launcher in its YoRHa theme, at the scale of two it opens in on a frame of 720
// lines (src/gui/themes/yorha/theme.ini, src/gui/main.c): the measures are its, the drawing this
// program's own, with its VGA font and nothing from elsewhere.

#define SC UI_TEXT_SCALE
#define CW UI_CELL_WIDTH
#define CH UI_CELL_HEIGHT
#define BH UI_BUTTON_HEIGHT
// The frame's edge, a cell of the font at its own size, and where the lists start across it:
// two cells in, the cursor in the margin before them.
#define EDGE 16u
#define LIST_X (EDGE + 2u * CW)
#define RULE_Y (UI_TABS_TOP + BH + 6u)
// The frieze: a rule two pixels thick, a notch every 52 pixels (the theme's spacing of 26 at its
// scale) and three dots between two notches.
#define FRIEZE_SPACING 52u

static uint32_t Grain(uint32_t x, uint32_t y) {
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/// The page of @p look at one pixel: its colour, the grid (or the run's lines), a diagonal every
/// 330 pixels a third of the way to the dim colour, the corners down by up to three in a hundred,
/// as a convex screen's, and a grain of three either way.
static void PageColourOf(const Look *look, uint32_t x, uint32_t y, uint32_t width,
                         uint32_t height, uint8_t out[4]) {
    const bool line = look->grid ? x % GRID == 0u || y % GRID == 0u : y % 4u == 3u;
    const uint8_t *const base = line ? look->ground : look->page;
    const bool diagonal = (x + 3300u - y % 3300u) % 330u == 0u;
    const int64_t dx = (int64_t)x - (int64_t)width / 2;
    const int64_t dy = (int64_t)y - (int64_t)height / 2;
    const int64_t half_w = width > 1u ? (int64_t)width / 2 : 1;
    const int64_t half_h = height > 1u ? (int64_t)height / 2 : 1;
    // Nought at the middle, 200 at a corner.
    const int64_t far = dx * dx * 100 / (half_w * half_w) + dy * dy * 100 / (half_h * half_h);
    const int grain = (int)(Grain(x, y) % 7u) - 3;
    for (int k = 0; k < 3; ++k) {
        int64_t value = base[k];
        if (diagonal) {
            value += ((int64_t)look->dim[k] - value) / 3;
        }
        value -= value * far * 3 / 20000;
        value += grain;
        out[k] = (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
    }
    out[3] = 0xFF;
}

void UiPageColour(uint32_t x, uint32_t y, uint32_t frame_width, uint32_t frame_height,
                  uint8_t out[4]) {
    PageColourOf(&MENU, x, y, frame_width, frame_height, out);
}

/// The page with its grain under a rectangle, following the frame, so a part drawn again meets
/// the rest.
static void Backdrop(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t width,
                     uint32_t height) {
    for (uint32_t row = y; row < y + height && row < frame->height; ++row) {
        uint8_t *const line = frame->pixels + (size_t)row * frame->pitch;
        for (uint32_t column = x; column < x + width && column < frame->width; ++column) {
            PageColourOf(g_look, column, row, frame->width, frame->height,
                         line + (size_t)column * 4u);
        }
    }
}

/// The frieze along the top and the bottom of a rectangle, the bottom's the top's upside down.
static void Frieze(const UiFrame *frame, uint32_t x, uint32_t top, uint32_t width,
                   uint32_t height) {
    for (int bottom = 0; bottom < 2; ++bottom) {
        const uint32_t rule = bottom ? top + height - 6u : top + 4u;
        FillRect(frame, x, rule, width, 2u, TEXT);
        for (uint32_t at = 0; at < width; at += FRIEZE_SPACING) {
            const uint32_t notch = bottom ? rule - 4u : rule + 2u;
            const uint32_t dots = bottom ? rule - 10u : rule + 6u;
            const uint32_t low = bottom ? rule - 16u : rule + 12u;
            const uint32_t left = at >= 4u ? at - 4u : 0u;
            FillRect(frame, x + left, notch, at >= 4u ? (at + 4u < width ? 8u : width - left) : 4u,
                     4u, TEXT);
            if (at + 32u <= width) {
                FillRect(frame, x + at + 20u, dots, 4u, 4u, TEXT);
                FillRect(frame, x + at + 28u, dots, 4u, 4u, TEXT);
                FillRect(frame, x + at + 24u, low, 4u, 4u, TEXT);
            }
        }
    }
}

/// Text at the look's size; hands back where the next word along the line would start.
static uint32_t Big(const UiFrame *frame, uint32_t x, uint32_t y, const uint8_t colour[4],
                    const char *text) {
    UiText(frame, x, y, SC, colour, text);
    return x + (uint32_t)strlen(text) * CW;
}

/// Big text cut to end before @p right, a tilde where it was cut.
static uint32_t BigFit(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t right,
                       const uint8_t colour[4], const char *text) {
    char fitted[200];
    Fit(fitted, sizeof(fitted), text, right > x ? (right - x) / CW : 0u);
    return Big(frame, x, y, colour, fitted);
}

/// One glyph of code page 437 at the look's size, the cursor and the arrows among them.
static void BigGlyph(const UiFrame *frame, uint32_t x, uint32_t y, const uint8_t colour[4],
                     unsigned char glyph) {
    DrawGlyph(frame, x, y, SC, colour, glyph);
}

/// Where tab @p index's button is: a column wider than its name either side, a column apart.
static UiRect TabRect(uint32_t index) {
    uint32_t x = EDGE;
    for (uint32_t i = 0; i < index; ++i) {
        x += ((uint32_t)strlen(UiTabName((UiTab)i)) + 2u) * CW + CW;
    }
    const UiRect rect = { x, UI_TABS_TOP, ((uint32_t)strlen(UiTabName((UiTab)index)) + 2u) * CW,
                          BH };
    return rect;
}

const char *UiTabName(UiTab tab) {
    switch (tab) {
    case UI_TAB_START:
        return "Start";
    case UI_TAB_SUITES:
        return "Suites";
    case UI_TAB_DEMOS:
        return "Demos";
    case UI_TAB_RESULTS:
        return "Results";
    case UI_TAB_OPTIONS:
    case UI_TABS:
        break;
    }
    return "Options";
}

/// The tab @p tap went down and came up on, or UI_TABS.
static UiTab TabAt(const UiTap *tap) {
    for (uint32_t i = 0; i < UI_TABS; ++i) {
        const UiRect rect = TabRect(i);
        if (tap->x0 >= rect.x && tap->x0 < rect.x + rect.width && tap->y0 >= rect.y &&
            tap->y0 < rect.y + rect.height && tap->x1 >= rect.x &&
            tap->x1 < rect.x + rect.width && tap->y1 >= rect.y && tap->y1 < rect.y + rect.height) {
            return (UiTab)i;
        }
    }
    return UI_TABS;
}

/// The head: the frieze, the page, the tabs with @p shown inverted (none past the last), the rule
/// under them, and the title four times the font with its shadow, @p right at its right.
static void Head(const UiFrame *frame, uint32_t shown, const char *title, const char *right) {
    Backdrop(frame, 0, 0, frame->width, frame->height);
    Frieze(frame, 0, 0, frame->width, frame->height);
    for (uint32_t i = 0; i < UI_TABS && shown <= UI_TABS; ++i) {
        if (shown == UI_TABS) {
            break;
        }
        const UiRect rect = TabRect(i);
        FillRect(frame, rect.x, rect.y, rect.width, rect.height, i == shown ? TEXT : BORDER);
        Big(frame, rect.x + CW, rect.y + 2u * SC, i == shown ? BACKGROUND : TEXT,
            UiTabName((UiTab)i));
    }
    FillRect(frame, EDGE, RULE_Y, frame->width - 2u * EDGE, SC, BORDER);
    UiText(frame, EDGE + 4u, UI_TITLE_TOP + 4u, 2u * SC, BORDER, title);
    UiText(frame, EDGE, UI_TITLE_TOP, 2u * SC, TEXT, title);
    if (right != NULL && right[0] != '\0') {
        const uint32_t title_end = EDGE + (uint32_t)strlen(title) * 2u * CW + 2u * CW;
        char fitted[96];
        const uint32_t room = frame->width - EDGE > title_end ? (frame->width - EDGE - title_end) / CW
                                                              : 0u;
        Fit(fitted, sizeof(fitted), right, room);
        const uint32_t width = (uint32_t)strlen(fitted) * CW;
        Big(frame, frame->width - EDGE - width, UI_TITLE_TOP + FONTVGA_HEIGHT * 2u - CH / 2u,
            DIM, fitted);
    }
}

/// A box with a head: the page's colour in a frame of the dim one, its head a band of the ink
/// with its name in the page's colour and @p right against its right. Hands back where what is
/// in it starts.
static uint32_t Box(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                    const char *name, const char *right, const uint8_t *head) {
    FillRect(frame, x, y, width, height, BACKGROUND);
    Outline(frame, x, y, width, height, BORDER);
    Outline(frame, x + 1u, y + 1u, width - 2u, height - 2u, BORDER);
    if (name == NULL) {
        return y + 8u;
    }
    FillRect(frame, x, y, width, BH, head != NULL ? head : TEXT);
    BigFit(frame, x + CW / 2u, y + 2u * SC, x + width - CW / 2u, BACKGROUND, name);
    if (right != NULL) {
        Big(frame, x + width - CW / 2u - (uint32_t)strlen(right) * CW, y + 2u * SC, BACKGROUND,
            right);
    }
    return y + BH + 4u;
}

/// A row of a list as the launcher draws one: a box on the dim colour, or under the cursor the
/// box inverted, a rule over and under it, and the cursor in the margin. Hands back its ink.
static const uint8_t *ListRow(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t width,
                              uint32_t height, bool selected) {
    if (selected) {
        FillRect(frame, x, y - 4u, width, SC, TEXT);
        FillRect(frame, x, y + height + 2u, width, SC, TEXT);
        FillRect(frame, x, y, width, height, TEXT);
        BigGlyph(frame, x - 2u * CW, y + 2u, TEXT, (unsigned char)FONTVGA_CURSOR);
    } else {
        FillRect(frame, x, y, width, height, BORDER);
    }
    return selected ? BACKGROUND : TEXT;
}

typedef enum {
    MARK_NONE,
    MARK_SOME,
    MARK_ALL,
} Mark;

/// The theme's square, as whether the row is in the next run: hollow, half, full.
static void Marker(const UiFrame *frame, uint32_t x, uint32_t y, Mark mark,
                   const uint8_t colour[4]) {
    const uint32_t top = y + (UI_ROW_BOX - 14u) / 2u;
    Outline(frame, x, top, 14u, 14u, colour);
    Outline(frame, x + 1u, top + 1u, 12u, 12u, colour);
    if (mark == MARK_ALL) {
        FillRect(frame, x + 4u, top + 4u, 6u, 6u, colour);
    } else if (mark == MARK_SOME) {
        FillRect(frame, x + 4u, top + 6u, 6u, 2u, colour);
    }
}

UiRect UiFootKey(uint32_t frame_width, uint32_t frame_height, const UiKey *keys, uint32_t count,
                 uint32_t index) {
    const UiRect none = { 0, 0, 0, 0 };
    if (keys == NULL || index >= count || frame_width < UI_MIN_WIDTH ||
        frame_height < UI_MIN_HEIGHT) {
        return none;
    }
    uint32_t text = 0;
    for (uint32_t i = 0; i < count; ++i) {
        text += ((uint32_t)strlen(keys[i].key) + 1u + (uint32_t)strlen(keys[i].what)) * CW;
    }
    // Two cells between keys, or one where two would push the first off the foot: the results of
    // a run the browser can run again name seven.
    const uint32_t room = frame_width - 2u * EDGE - 4u * CW;
    const uint32_t gap = text + (count - 1u) * 2u * CW <= room ? 2u * CW : CW;
    const uint32_t width = text + (count - 1u) * gap;
    if (width > room) {
        return none;
    }
    uint32_t x = frame_width - EDGE - CW - width;
    for (uint32_t i = 0; i < index; ++i) {
        x += ((uint32_t)strlen(keys[i].key) + 1u + (uint32_t)strlen(keys[i].what)) * CW + gap;
    }
    const UiRect rect = {
        x - SC, UI_FOOT_TOP(frame_height) + 8u + CH,
        ((uint32_t)strlen(keys[index].key) + 1u + (uint32_t)strlen(keys[index].what)) * CW +
            2u * SC,
        CH
    };
    return rect;
}

uint32_t UiFootKeyAt(const UiTap *tap, uint32_t frame_width, uint32_t frame_height,
                     const UiKey *keys, uint32_t count) {
    for (uint32_t i = 0; tap != NULL && i < count; ++i) {
        const UiRect rect = UiFootKey(frame_width, frame_height, keys, count, i);
        if (rect.width > 0u && Inside(rect, tap->x0, tap->y0) && Inside(rect, tap->x1, tap->y1)) {
            return i;
        }
    }
    return UINT32_MAX;
}

/// The foot: a box with the ink's bar at its left, @p tip on its first row in @p tip_colour, and
/// on its second the keys against its right, each key inverted and what it does after it.
static void Foot(const UiFrame *frame, const char *tip, const uint8_t *tip_colour,
                 const UiKey *keys, uint32_t count) {
    const uint32_t y = UI_FOOT_TOP(frame->height);
    FillRect(frame, EDGE, y, frame->width - 2u * EDGE, UI_FOOT_HEIGHT, BACKGROUND);
    Outline(frame, EDGE, y, frame->width - 2u * EDGE, UI_FOOT_HEIGHT, BORDER);
    FillRect(frame, EDGE + 6u, y + 6u, 6u, UI_FOOT_HEIGHT - 12u, TEXT);
    if (tip != NULL) {
        BigFit(frame, EDGE + 3u * CW, y + 6u, frame->width - EDGE - CW,
               tip_colour != NULL ? tip_colour : TEXT, tip);
    }
    for (uint32_t i = 0; i < count; ++i) {
        const UiRect rect = UiFootKey(frame->width, frame->height, keys, count, i);
        if (rect.width == 0u) {
            continue;
        }
        const uint32_t kw = (uint32_t)strlen(keys[i].key) * CW;
        FillRect(frame, rect.x, rect.y, kw + 2u * SC, CH, TEXT);
        Big(frame, rect.x + SC, rect.y, BACKGROUND, keys[i].key);
        Big(frame, rect.x + SC + kw + CW, rect.y, TEXT, keys[i].what);
    }
}

/// The verdict word on its colour at the look's size.
static uint32_t BigVerdict(const UiFrame *frame, uint32_t x, uint32_t y, ReportResult result) {
    const char *const word = ResultWord(result);
    const uint32_t width = (uint32_t)strlen(word) * CW;
    FillRect(frame, x - 2u, y, width + 4u, CH, ResultColour(result));
    return Big(frame, x, y, BACKGROUND, word);
}


/// A verdict as the launcher draws a row that failed: the word on its colour, in the page's.
static uint32_t SayVerdict(const UiFrame *frame, uint32_t x, uint32_t y, ReportResult result) {
    const char *const word = ResultWord(result);
    FillRect(frame, x - 3u, y - 1u, (uint32_t)strlen(word) * GLYPH + 6u, FONTVGA_HEIGHT + 2u,
             ResultColour(result));
    return Say(frame, x, y, BACKGROUND, word);
}

/// A row of a list as the launcher draws one: a box on the dim colour, or for the row the cursor
/// is on, the box inverted on the ink and the cursor in the margin beside it. Hands back the
/// colour the row's words take.
static const uint8_t *DrawRowBox(const UiFrame *frame, uint32_t y, uint32_t height,
                                 bool selected) {
    FillRect(frame, MARGIN - 4u, y + 1u, frame->width - 2u * (MARGIN - 4u), height - 2u,
             selected ? TEXT : BORDER);
    if (selected) {
        DrawGlyph(frame, MARGIN / 2u - 4u, y + (height - FONTVGA_HEIGHT) / 2u, 1u, TEXT,
                  (unsigned char)FONTVGA_CURSOR);
    }
    return selected ? BACKGROUND : TEXT;
}

/// The box a list under it sits in: the page's colour inside a frame of the dim one.
static void DrawPane(const UiFrame *frame, uint32_t top, uint32_t bottom) {
    if (bottom <= top + 2u) {
        return;
    }
    FillRect(frame, MARGIN / 2u, top, frame->width - MARGIN, bottom - top, BACKGROUND);
    Outline(frame, MARGIN / 2u, top, frame->width - MARGIN, bottom - top, BORDER);
}

/// A group's verdict in one word: failed if anything did, passed if anything did, and skipped
/// only when nothing was asked at all.
static ReportResult GroupResult(uint32_t passed, uint32_t failed) {
    return failed > 0u ? REPORT_FAIL : passed > 0u ? REPORT_PASS : REPORT_SKIP;
}

/// The counts along a line, each in its own colour and dim when it is nought; crashes only
/// when there are any, apart from the skips as the summary counts them.
static uint32_t SayCounts(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t passed,
                          uint32_t failed, uint32_t skipped, uint32_t crashed) {
    char text[32];
    snprintf(text, sizeof(text), "%u passed", (unsigned)passed);
    x = Say(frame, x, y, passed > 0u ? PASSED : DIM, text) + 2u * GLYPH;
    snprintf(text, sizeof(text), "%u failed", (unsigned)failed);
    x = Say(frame, x, y, failed > 0u ? FAILED : DIM, text) + 2u * GLYPH;
    snprintf(text, sizeof(text), "%u skipped", (unsigned)skipped);
    x = Say(frame, x, y, skipped > 0u ? SKIPPED : DIM, text);
    if (crashed == 0u) {
        return x;
    }
    snprintf(text, sizeof(text), "%u crashed", (unsigned)crashed);
    return Say(frame, x + 2u * GLYPH, y, CRASHED, text);
}

/// The proportions of a group at a glance: failures first and never thinner than two pixels,
/// so one failure among a hundred passes still shows.
static void DrawBar(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t width,
                    const UiGroup *group) {
    const uint32_t total = group->passed + group->failed + group->skipped + group->crashed;
    if (total == 0u || width < 8u) {
        return;
    }
    uint32_t failed = group->failed > 0u ? group->failed * width / total : 0u;
    uint32_t crashed = group->crashed > 0u ? group->crashed * width / total : 0u;
    uint32_t skipped = group->skipped > 0u ? group->skipped * width / total : 0u;
    if (group->failed > 0u && failed < 2u) {
        failed = 2u;
    }
    if (group->crashed > 0u && crashed < 2u) {
        crashed = 2u;
    }
    if (group->skipped > 0u && skipped < 2u) {
        skipped = 2u;
    }
    // The two pixel floors can add up to more than the bar; the largest share gives way.
    while (failed + crashed + skipped > width) {
        if (failed >= skipped && failed >= crashed) {
            --failed;
        } else if (skipped >= crashed) {
            --skipped;
        } else {
            --crashed;
        }
    }
    if (group->passed == 0u) {
        // Nothing passed: the others fill the bar between them.
        if (group->skipped > 0u) {
            skipped = width - failed - crashed;
        } else if (group->crashed > 0u) {
            crashed = width - failed;
        } else {
            failed = width;
        }
    }
    const uint32_t passed = width - failed - crashed - skipped;
    FillRect(frame, x, y, failed, 8u, FAILED);
    FillRect(frame, x + failed, y, crashed, 8u, CRASHED);
    FillRect(frame, x + failed + crashed, y, skipped, 8u, SKIPPED);
    FillRect(frame, x + failed + crashed + skipped, y, passed, 8u, PASSED);
}

// The overview's columns, two cells apart, so a failure count is found by looking straight down
// and the counts spread across the row rather than bunch at its left.
#define COLUMN (2u * GLYPH)

/// A group's row. @p crashes_column is set when anything in the run crashed: a column of its own
/// then, and a run with none keeps the three it always had.
static void DrawGroupRow(const UiFrame *frame, const UiGroup *group, uint32_t y, bool selected,
                         bool crashes_column) {
    const uint8_t *const ink = DrawRowBox(frame, y, UI_ROW_HEIGHT, selected);
    DrawGlyph(frame, MARGIN, y + 2u, 1u, ink, (unsigned char)FONTVGA_MARKER);
    char text[32];
    Fit(text, sizeof(text), group->suite, 12u);
    Say(frame, MARGIN + 2u * GLYPH, y + 2u, selected || group->failed == 0u ? ink : FAILED, text);
    uint32_t x = MARGIN + 16u * COLUMN;
    snprintf(text, sizeof(text), "%4u passed", (unsigned)group->passed);
    Say(frame, x, y + 2u, selected ? ink : group->passed > 0u ? PASSED : DIM, text);
    x += 12u * COLUMN;
    snprintf(text, sizeof(text), "%3u failed", (unsigned)group->failed);
    Say(frame, x, y + 2u, selected ? ink : group->failed > 0u ? FAILED : DIM, text);
    x += 11u * COLUMN;
    snprintf(text, sizeof(text), "%3u skipped", (unsigned)group->skipped);
    Say(frame, x, y + 2u, selected ? ink : group->skipped > 0u ? SKIPPED : DIM, text);
    if (crashes_column) {
        x += 12u * COLUMN;
        snprintf(text, sizeof(text), "%3u crashed", (unsigned)group->crashed);
        Say(frame, x, y + 2u, selected ? ink : group->crashed > 0u ? CRASHED : DIM, text);
    }
    const uint32_t bar = MARGIN + (crashes_column ? 64u : 52u) * COLUMN;
    if (frame->width > bar + MARGIN) {
        DrawBar(frame, bar, y + 6u, frame->width - MARGIN - bar, group);
    }
}

/// The result on its colour, and the case's name after it in @p ink.
static void DrawCaseLine(const UiFrame *frame, uint32_t x, uint32_t y, const ReportRecord *record,
                         uint32_t name_columns, const uint8_t ink[4]) {
    SayVerdict(frame, x, y, record->result);
    char name[200];
    Fit(name, sizeof(name), record->name, name_columns);
    Say(frame, x + 5u * GLYPH, y, ink, name);
}

/// Under the overview's list: what the selected group holds, its failures first, then what the
/// program ended inside, then its skips.
static void ComposePreview(const UiFrame *frame, const UiRun *run, uint32_t index, uint32_t y) {
    const UiGroup *const group = &run->groups[index];
    char text[96];
    uint32_t x = Say(frame, MARGIN, y, TEXT, group->suite);
    snprintf(text, sizeof(text), " in %s: ", UiFamilyName(group->family));
    x = Say(frame, x, y, DIM, text);
    if (group->failed > 0u) {
        snprintf(text, sizeof(text), "%u of %u failed", (unsigned)group->failed,
                 (unsigned)group->count);
        SayFitted(frame, x, y, FAILED, text);
    } else if (group->crashed > 0u) {
        snprintf(text, sizeof(text), "nothing failed, %u of %u skipped as crashed",
                 (unsigned)group->crashed, (unsigned)group->count);
        SayFitted(frame, x, y, CRASHED, text);
    } else if (group->skipped > 0u) {
        snprintf(text, sizeof(text), "nothing failed, %u of %u skipped",
                 (unsigned)group->skipped, (unsigned)group->count);
        SayFitted(frame, x, y, SKIPPED, text);
    } else {
        snprintf(text, sizeof(text), "all %u passed", (unsigned)group->count);
        SayFitted(frame, x, y, PASSED, text);
        Say(frame, MARGIN, y + LINE, DIM, "A opens the list of its cases");
        return;
    }

    const ReportResult shown = group->failed > 0u    ? REPORT_FAIL
                               : group->crashed > 0u ? REPORT_CRASHED
                                                     : REPORT_SKIP;
    const uint32_t wanted = group->failed > 0u    ? group->failed
                            : group->crashed > 0u ? group->crashed
                                                  : group->skipped;
    const uint32_t room = PREVIEW_LINES - 1u;
    const uint32_t columns = Columns(frame) > 5u ? Columns(frame) - 5u : 1u;
    uint32_t listed = 0;
    for (uint32_t i = 0; i < group->count && listed < room; ++i) {
        const ReportRecord *const record = UiCaseAt(run, index, i);
        if (record == NULL || record->result != shown) {
            continue;
        }
        if (listed + 1u == room && wanted > room) {
            snprintf(text, sizeof(text), "and %u more: A opens the list",
                     (unsigned)(wanted - listed));
            Say(frame, MARGIN, y + (listed + 1u) * LINE, DIM, text);
            break;
        }
        DrawCaseLine(frame, MARGIN, y + (listed + 1u) * LINE, record, columns, TEXT);
        ++listed;
    }
}

/// The suites that failed, crashed or skipped, as one line: which is what the overview says
/// first.
static void ListGroups(char *out, size_t out_size, const UiRun *run, ReportResult which) {
    size_t used = strlen(out);
    const char *separator = "";
    for (uint32_t g = 0; g < run->group_count && used + 1u < out_size; ++g) {
        const UiGroup *const group = &run->groups[g];
        const uint32_t count = which == REPORT_FAIL      ? group->failed
                               : which == REPORT_CRASHED ? group->crashed
                                                         : group->skipped;
        if (count == 0u) {
            continue;
        }
        snprintf(out + used, out_size - used, "%s%s", separator, group->suite);
        used = strlen(out);
        separator = ", ";
    }
}

/// Under the head's box: the line that says the screens do not hold the whole run.
#define NOTE_Y (UI_LIST_TOP - 20u)

/// That a list goes on past its top or its bottom, at the right of the band over it and on the
/// frame of the box under it, on the page's colour so the frame does not run through the words.
/// @p pane is that frame's top.
static void DrawMore(const UiFrame *frame, bool above, bool below, uint32_t pane) {
    const uint32_t x = frame->width - MARGIN - 10u * GLYPH;
    if (above) {
        FillRect(frame, x - GLYPH, UI_LIST_TOP - FONTVGA_HEIGHT - 2u, 12u * GLYPH,
                 FONTVGA_HEIGHT, BACKGROUND);
        Say(frame, x, UI_LIST_TOP - FONTVGA_HEIGHT - 2u, DIM, "more above");
    }
    if (below) {
        const uint32_t y = pane - FONTVGA_HEIGHT / 2u;
        FillRect(frame, x - GLYPH, y, 12u * GLYPH, FONTVGA_HEIGHT, BACKGROUND);
        Say(frame, x, y, DIM, "more below");
    }
}

/// The band over a results list: the verdict, then the counts in their colours, at the look's
/// size; @p right against its right in the page's colour.
static void CountsBand(const UiFrame *frame, ReportResult verdict, const char *what,
                       uint32_t passed, uint32_t failed, uint32_t skipped, uint32_t crashed,
                       const char *right) {
    const uint32_t x0 = MARGIN / 2u;
    const uint32_t width = frame->width - MARGIN;
    FillRect(frame, x0, UI_BODY_TOP, width, BH, TEXT);
    const uint32_t y = UI_BODY_TOP + 2u * SC;
    uint32_t x = BigVerdict(frame, x0 + CW / 2u + 2u, y, verdict) + CW;
    x = Big(frame, x, y, BACKGROUND, what) + CW;
    char text[32];
    const struct {
        uint32_t count;
        const char *word;
        const uint8_t *colour;
    } PARTS[4] = {
        { passed, "passed", PASSED },
        { failed, "failed", FAILED },
        { skipped, "skipped", SKIPPED },
        { crashed, "crashed", CRASHED },
    };
    const uint32_t right_x =
        right != NULL ? x0 + width - CW / 2u - (uint32_t)strlen(right) * CW : x0 + width;
    for (uint32_t i = 0; i < 4u; ++i) {
        if (i == 3u && crashed == 0u) {
            break;
        }
        snprintf(text, sizeof(text), "%u", (unsigned)PARTS[i].count);
        if (x + ((uint32_t)strlen(text) + strlen(PARTS[i].word) + 2u) * CW > right_x) {
            break;
        }
        x = Big(frame, x, y, PARTS[i].count > 0u ? PARTS[i].colour : DIM, text);
        x = Big(frame, x + CW, y, BACKGROUND, PARTS[i].word) + 2u * CW;
    }
    if (right != NULL) {
        Big(frame, right_x, y, BACKGROUND, right);
    }
}

static void ResultsHead(const UiFrame *frame, const UiRun *run, UiState state) {
    Head(frame, state.browser ? UI_TAB_RESULTS : UI_TABS, "EDENPROBE  /  RESULTS",
         run != NULL ? run->heading : NULL);
}

static void ResultsFoot(const UiFrame *frame, const UiRun *run, UiState state, const char *tip,
                        const uint8_t *tip_colour) {
    UiKey keys[UI_FOOT_KEYS];
    const uint32_t count = UiResultsKeys(run, state, keys);
    Foot(frame, tip, tip_colour, keys, count);
}

static void ComposeOverview(const UiFrame *frame, const UiRun *run, UiState state) {
    const uint32_t shown = run->passed + run->failed + run->skipped + run->crashed;
    char text[256];
    snprintf(text, sizeof(text), "%u cases:", (unsigned)shown);
    CountsBand(frame, GroupResult(run->passed, run->failed), text, run->passed, run->failed,
               run->skipped, run->crashed, NULL);

    // The tip: where it failed, or crashed, or skipped; and before all that, a run whose cases
    // did not all reach the screens says so where the person is, rather than leaving the counts
    // above to disagree with the report's summary in silence.
    const uint8_t *tip_colour = FAILED;
    if (run->reported > shown) {
        char memory[48] = "";
        char suites[48] = "";
        if (run->reported > run->count) {
            snprintf(memory, sizeof(memory), " %u did not fit in memory",
                     (unsigned)(run->reported - run->count));
        }
        if (run->unplaced > 0u) {
            snprintf(suites, sizeof(suites), "%s %u are in suites past the first %u",
                     memory[0] != '\0' ? ";" : "", (unsigned)run->unplaced,
                     (unsigned)UI_MAX_GROUPS);
        }
        snprintf(text, sizeof(text), "The screens hold %u of the run's %u cases:%s%s",
                 (unsigned)shown, (unsigned)run->reported, memory, suites);
        tip_colour = SKIPPED;
    } else if (run->failed > 0u) {
        snprintf(text, sizeof(text), "Failed in: ");
        ListGroups(text, sizeof(text), run, REPORT_FAIL);
    } else if (run->crashed > 0u) {
        snprintf(text, sizeof(text), "Nothing failed; crashed in: ");
        ListGroups(text, sizeof(text), run, REPORT_CRASHED);
        tip_colour = CRASHED;
    } else if (run->skipped > 0u) {
        snprintf(text, sizeof(text), "Nothing failed; skipped in: ");
        ListGroups(text, sizeof(text), run, REPORT_SKIP);
        tip_colour = PASSED;
    } else {
        snprintf(text, sizeof(text), "Nothing failed");
        tip_colour = PASSED;
    }

    // Every family heading and every group as one list, scrolled to keep the cursor in view.
    const uint32_t rows = UiListRows(UI_LEVEL_OVERVIEW, frame->height);
    const uint32_t selected = UiOverviewRow(run, state.group);
    const uint32_t total = UiOverviewRow(run, run->group_count - 1u) + 1u;
    const uint32_t first = UiListFirst(selected, total, rows);
    DrawPane(frame, UI_LIST_TOP - 6u, UI_LIST_TOP + rows * UI_ROW_HEIGHT + 4u);
    uint32_t row = 0;
    for (uint32_t g = 0; g < run->group_count; ++g) {
        if (g == 0u || run->groups[g].family != run->groups[g - 1u].family) {
            if (row >= first && row < first + rows) {
                const uint32_t y = UI_LIST_TOP + (row - first) * UI_ROW_HEIGHT;
                const uint32_t end =
                    Say(frame, MARGIN, y + 2u, DIM, UiFamilyName(run->groups[g].family));
                if (frame->width > end + GLYPH + MARGIN) {
                    FillRect(frame, end + GLYPH, y + 10u, frame->width - MARGIN - end - GLYPH,
                             1u, BORDER);
                }
            }
            ++row;
        }
        if (row >= first && row < first + rows) {
            DrawGroupRow(frame, &run->groups[g], UI_LIST_TOP + (row - first) * UI_ROW_HEIGHT,
                         g == state.group, run->crashed > 0u);
        }
        ++row;
    }
    const uint32_t below = UI_BODY_BOTTOM(frame->height) - PREVIEW_LINES * LINE;
    DrawPane(frame, below - 8u, UI_BODY_BOTTOM(frame->height));
    DrawMore(frame, first > 0u, first + rows < total, below - 8u);
    ComposePreview(frame, run, state.group, below);
    ResultsFoot(frame, run, state, text, tip_colour);
}

static void ComposeGroup(const UiFrame *frame, const UiRun *run, UiState state) {
    const UiGroup *const group = &run->groups[state.group];
    const uint32_t rows = UiListRows(UI_LEVEL_GROUP, frame->height);
    const uint32_t first = UiListFirst(state.index, group->count, rows);
    const uint32_t last = first + rows < group->count ? first + rows : group->count;
    char right[48];
    snprintf(right, sizeof(right), "%u / %u", (unsigned)(state.index + 1u),
             (unsigned)group->count);
    CountsBand(frame, GroupResult(group->passed, group->failed), group->suite, group->passed,
               group->failed, group->skipped, group->crashed, right);

    // Room at the end of the row for the word that says there are pictures to open.
    DrawPane(frame, UI_LIST_TOP - 6u, UI_LIST_TOP + rows * UI_ROW_HEIGHT + 4u);
    const uint32_t name_x = UI_MARK_LEFT + 5u * GLYPH;
    const uint32_t name_right = name_x + MARGIN + 4u * GLYPH;
    const uint32_t name_columns =
        frame->width > name_right ? (frame->width - name_right) / GLYPH : 1u;
    for (uint32_t i = first; i < last; ++i) {
        const ReportRecord *const record = UiCaseAt(run, state.group, i);
        const uint32_t y = UI_LIST_TOP + (i - first) * UI_ROW_HEIGHT;
        const bool selected = i == state.index;
        const uint8_t *const ink = DrawRowBox(frame, y, UI_ROW_HEIGHT, selected);
        DrawGlyph(frame, MARGIN, y + 2u, 1u, ink, (unsigned char)FONTVGA_MARKER);
        DrawCaseLine(frame, UI_MARK_LEFT, y + 2u, record, name_columns, ink);
        if (record->picture >= 0) {
            Say(frame, frame->width - MARGIN - 3u * GLYPH, y + 2u, selected ? ink : DIM, "pic");
        }
    }

    // The selected case under the list: its whole name, as much of its detail as fits, and what
    // opening it will show.
    const ReportRecord *const record = UiCaseAt(run, state.group, state.index);
    const uint32_t below = UI_BODY_BOTTOM(frame->height) - DETAIL_LINES * LINE;
    DrawPane(frame, below - 8u, UI_BODY_BOTTOM(frame->height));
    DrawMore(frame, first > 0u, last < group->count, below - 8u);
    const uint32_t columns = Columns(frame);
    const uint32_t named =
        TextWrapped(frame, MARGIN, below, 1u, LINE, TEXT, record->name, columns, 2u);
    const uint32_t room = DETAIL_LINES - named;
    const uint32_t detail_y = below + named * LINE;
    uint32_t needed = 0;
    if (record->detail[0] == '\0') {
        Say(frame, MARGIN, detail_y, DIM, NoDetail(record->result));
    } else {
        needed = UiWrappedLines(record->detail, columns);
        TextWrapped(frame, MARGIN, detail_y, 1u, LINE, DIM, record->detail, columns, room);
    }
    const char *const hint = record->picture >= 0 ? "A opens its pictures"
                             : needed > room      ? "The detail runs on: A shows all of it"
                                                  : "A opens it on a page of its own";
    char tip[96];
    snprintf(tip, sizeof(tip), "%s family: %s", UiFamilyName(group->family), hint);
    ResultsFoot(frame, run, state, tip, TEXT);
}

UiRect UiPanelRect(uint32_t frame_width, uint32_t frame_height, uint32_t width, uint32_t height,
                   UiView view, uint32_t panel) {
    const UiRect none = { 0, 0, 0, 0 };
    if (width == 0u || height == 0u || frame_width < UI_MIN_WIDTH ||
        frame_height < UI_MIN_HEIGHT) {
        return none;
    }
    // The key and two lines of the detail under the pictures, above the foot.
    const uint32_t bottom = UI_BODY_BOTTOM(frame_height) - 2u * LINE - 36u;
    const uint32_t down = bottom > UI_PANEL_TOP ? (bottom - UI_PANEL_TOP) / height : 1u;
    const bool all = view == UI_VIEW_ALL;
    const uint32_t room = all ? (frame_width - 4u * MARGIN) / 3u : frame_width - 2u * MARGIN;
    uint32_t scale = room / width;
    scale = down < scale ? down : scale;
    scale = scale == 0u ? 1u : scale;
    const uint32_t size = width * scale;
    uint32_t x;
    if (all) {
        const uint32_t gap = frame_width > 2u * MARGIN + 3u * size
                                 ? (frame_width - 2u * MARGIN - 3u * size) / 2u
                                 : MARGIN;
        x = MARGIN + (panel < 3u ? panel : 2u) * (size + gap);
    } else {
        x = frame_width > size ? (frame_width - size) / 2u : 0u;
    }
    const UiRect rect = { x, UI_PANEL_TOP, size, height * scale };
    return rect;
}

/// The case's pictures, laid out as the detailed mode always has: the three side by side or
/// one alone, the colours of a verdict named under them, and the detail under that.
static void ComposePictures(const UiFrame *frame, const PictureRecord *record,
                            const ReportRecord *line, UiView view) {
    const uint32_t columns = Columns(frame);
    static const UiView PANELS[3] = { UI_VIEW_GOT, UI_VIEW_EXPECTED, UI_VIEW_DIFF };
    const uint32_t panels = view == UI_VIEW_ALL ? 3u : 1u;
    UiRect rect = { 0, 0, 0, 0 };
    for (uint32_t panel = 0; panel < panels; ++panel) {
        rect = UiPanelRect(frame->width, frame->height, record->width, record->height, view,
                           panel);
        const UiView shown = view == UI_VIEW_ALL ? PANELS[panel] : view;
        Say(frame, rect.x, rect.y - 20u, DIM, ViewLabel(shown));
        DrawPanel(frame, rect.x, rect.y, rect.width / record->width, record, shown);
    }
    const uint32_t bottom = rect.y + rect.height;

    // The key sits between the pictures and the sentence, because it explains the one and is
    // read on the way to the other.
    DrawKey(frame, MARGIN, bottom + UI_KEY_GAP);

    const uint32_t detail_y = bottom + 36u;
    const uint32_t limit = UI_BODY_BOTTOM(frame->height);
    const uint32_t room_for_lines = limit > detail_y + LINE ? (limit - detail_y) / LINE : 1u;
    if (line->detail[0] != '\0') {
        TextWrapped(frame, MARGIN, detail_y, 1u, LINE, TEXT, line->detail, columns,
                    room_for_lines);
    } else {
        Say(frame, MARGIN, detail_y, DIM, NoDetail(line->result));
    }
}

/// A case with no pictures, as a page: its whole name and its whole detail.
static void ComposeDetail(const UiFrame *frame, const UiGroup *group, const ReportRecord *line) {
    const uint32_t columns = Columns(frame);
    uint32_t y = UI_BODY_TOP + BH + 12u;
    y += TextWrapped(frame, MARGIN, y, 1u, PAGE_LINE, TEXT, line->name, columns, 3u) *
             PAGE_LINE +
         12u;
    Say(frame, MARGIN, y, DIM, Skipped(line->result) ? "why it was skipped" : "detail");
    y += PAGE_LINE;
    // One line kept back for the note below.
    const uint32_t limit = UI_BODY_BOTTOM(frame->height);
    const uint32_t room = limit > y + 2u * PAGE_LINE ? (limit - y) / PAGE_LINE - 1u : 1u;
    if (line->detail[0] == '\0') {
        Say(frame, MARGIN, y, DIM, NoDetail(line->result));
        y += PAGE_LINE;
    } else {
        y += TextWrapped(frame, MARGIN, y, 1u, PAGE_LINE, TEXT, line->detail, columns, room) *
             PAGE_LINE;
    }
    // A drawing case with no pictures stopped before it had any, which is worth saying where a
    // person would otherwise look for them.
    if (group->family == UI_FAMILY_GRAPHICS && line->picture < 0) {
        Say(frame, MARGIN, y + 8u, DIM, "no pictures were kept for this case");
    }
}

static void ComposeCase(const UiFrame *frame, const UiRun *run, UiState state,
                        const PictureRecord *picture) {
    const UiGroup *const group = &run->groups[state.group];
    const ReportRecord *const line = UiCaseAt(run, state.group, state.index);

    // Where this case sits among its group's failures as well as among its cases, so a person
    // stepping through the failures knows how many are left.
    uint32_t rank = 0;
    for (uint32_t i = 0; i <= state.index; ++i) {
        if (CaseFailed(run, state.group, i)) {
            ++rank;
        }
    }
    char header[160];
    if (line->result == REPORT_FAIL) {
        snprintf(header, sizeof(header), "%s   case %u of %u   failure %u of %u here   tier %s",
                 group->suite, (unsigned)(state.index + 1u), (unsigned)group->count,
                 (unsigned)rank, (unsigned)group->failed, line->tier);
    } else {
        snprintf(header, sizeof(header), "%s   case %u of %u   %u failed here   tier %s",
                 group->suite, (unsigned)(state.index + 1u), (unsigned)group->count,
                 (unsigned)group->failed, line->tier);
    }
    // The verdict first, at the band's start, because it is the one thing on this screen that
    // must be there however long the rest of the line runs; the case's name after it.
    const uint32_t x0 = MARGIN / 2u;
    FillRect(frame, x0, UI_BODY_TOP, frame->width - MARGIN, BH, TEXT);
    const uint32_t after = BigVerdict(frame, x0 + CW / 2u + 2u, UI_BODY_TOP + 2u * SC,
                                      line->result) +
                           CW;
    BigFit(frame, after, UI_BODY_TOP + 2u * SC, frame->width - MARGIN, BACKGROUND, line->name);

    if (picture != NULL) {
        ComposePictures(frame, picture, line, state.view);
    } else {
        ComposeDetail(frame, group, line);
    }
    ResultsFoot(frame, run, state, header, line->result == REPORT_FAIL ? FAILED : TEXT);
}

void UiCompose(const UiFrame *frame, const UiRun *run, UiState state,
               const PictureRecord *picture) {
    if (frame == NULL || frame->pixels == NULL) {
        return;
    }
    g_look = &MENU;
    // Every subtraction in the layout is from a frame at least this big; a smaller one would
    // wrap round and draw a line four thousand million pixels long.
    if (frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        Page(frame, 0, 0, frame->width, frame->height);
        UiText(frame, 0, 0, 1, TEXT, "frame too small");
        return;
    }
    ResultsHead(frame, run, state);
    if (run == NULL || run->group_count == 0u) {
        Big(frame, LIST_X, UI_BODY_TOP + 6u, TEXT, "No case was kept for the screens");
        char text[96] = "";
        if (run != NULL && run->reported > 0u) {
            snprintf(text, sizeof(text), "The run reported %u, and none of them fit in memory",
                     (unsigned)run->reported);
        }
        ResultsFoot(frame, run, state, text, DIM);
        return;
    }
    // A state from anywhere is drawn as the navigation would have left it, never read past the
    // end of a group.
    if (state.group >= run->group_count) {
        state.group = run->group_count - 1u;
    }
    if (state.index >= run->groups[state.group].count) {
        state.index = run->groups[state.group].count - 1u;
    }
    switch (state.level) {
    case UI_LEVEL_OVERVIEW:
        ComposeOverview(frame, run, state);
        break;
    case UI_LEVEL_GROUP:
        ComposeGroup(frame, run, state);
        break;
    case UI_LEVEL_CASE:
        ComposeCase(frame, run, state, picture);
        break;
    }
}

bool UiWaitsForPad(uint32_t frame_index, bool connected) {
    return !connected && frame_index < UI_PAD_WAIT_FRAMES;
}

bool UiLeavesUnattended(uint64_t elapsed_ns, bool pressed) {
    return !pressed && elapsed_ns >= UI_UNATTENDED_NANOSECONDS;
}

uint32_t UiUnattendedSecondsLeft(uint64_t elapsed_ns, bool pressed) {
    if (pressed || elapsed_ns >= UI_UNATTENDED_NANOSECONDS) {
        return 0;
    }
    const uint64_t left = UI_UNATTENDED_NANOSECONDS - elapsed_ns;
    return (uint32_t)((left + 999999999ull) / 1000000000ull);
}

/// The countdown's row, between the last line anything above can reach and the band of the
/// keys, so neither is ever drawn over: the page again, and @p line on it when there is one.
static void DrawCountdown(const UiFrame *frame, const char *line) {
    const uint32_t top = frame->height - FOOT;
    Page(frame, 0, top, frame->width, UI_COUNTDOWN_ROW);
    if (line != NULL) {
        char fitted[160];
        Fit(fitted, sizeof(fitted), line, (UiFootRight(frame->width) - MARGIN) / GLYPH);
        Say(frame, MARGIN, top + (UI_COUNTDOWN_ROW - FONTVGA_HEIGHT) / 2u, FAILED, fitted);
    }
}

void UiComposeCountdown(const UiFrame *frame, uint32_t seconds_left) {
    if (frame == NULL || frame->pixels == NULL || frame->width < UI_MIN_WIDTH ||
        frame->height < UI_MIN_HEIGHT || seconds_left == 0u) {
        return;
    }
    g_look = &MENU;
    // Over the foot's first row, the tip's, which it stands in for until a press.
    const uint32_t y = UI_FOOT_TOP(frame->height);
    FillRect(frame, EDGE + 14u, y + 4u, frame->width - 2u * EDGE - 16u, CH + 2u, BACKGROUND);
    char line[128];
    snprintf(line, sizeof(line),
             "Nothing pressed yet: this closes by itself in %u s; a button or a touch keeps it",
             (unsigned)seconds_left);
    BigFit(frame, EDGE + 3u * CW, y + 6u, frame->width - EDGE - CW, SKIPPED, line);
}

// The rows' column: its width, a row's box and the step to the next, which fit the eleven rows
// between the head and the foot; and the box beside them that says what the row does.
#define START_WIDTH 512u
#define START_BOX 34u
#define START_PITCH 41u
#define START_HELP_HEIGHT 230u

bool UiStartShown(const UiStartInfo *info) {
    return info != NULL &&
           ((info->families_present & UI_START_FAMILIES) != 0u || info->listed_count > 0u);
}

UiStartState UiStartOpen(const RunMode *from_file, uint32_t present) {
    UiStartState state;
    memset(&state, 0, sizeof(state));
    state.cursor = UI_START_ROW_BEGIN;
    state.changed = true;
    if (from_file != NULL) {
        state.retry = from_file->retry;
        state.families = ModeFamiliesToRun(from_file, present) & UI_START_FAMILIES;
        state.listen = from_file->listen;
        state.quick = !from_file->detailed;
        state.no_timing = from_file->no_timing;
    }
    return state;
}

/// The family a row turns on and off, or nought for a row that is not a family.
static uint32_t RowFamily(uint32_t row) {
    switch (row) {
    case UI_START_ROW_GRAPHICS:
        return MODE_FAMILY_GRAPHICS;
    case UI_START_ROW_FS:
        return MODE_FAMILY_FS;
    case UI_START_ROW_AUDIO:
    case UI_START_ROW_LISTEN:
        return MODE_FAMILY_AUDIO;
    case UI_START_ROW_NET:
        return MODE_FAMILY_NET;
    case UI_START_ROW_PAD:
        return MODE_FAMILY_PAD;
    default:
        return 0u;
    }
}

/// Whether a row can be turned on: a family this build has, a list with something in it.
static bool RowAvailable(const UiStartInfo *info, uint32_t row) {
    const uint32_t family = RowFamily(row);
    if (family != 0u) {
        return (info->families_present & family) != 0u;
    }
    if (row == UI_START_ROW_RETRY) {
        return info->listed_count > 0u;
    }
    return row < UI_START_ROWS;
}

static bool RowOn(const UiStartState *state, uint32_t row) {
    switch (row) {
    case UI_START_ROW_GRAPHICS:
    case UI_START_ROW_FS:
    case UI_START_ROW_AUDIO:
    case UI_START_ROW_NET:
    case UI_START_ROW_PAD:
        return (state->families & RowFamily(row)) != 0u;
    case UI_START_ROW_LISTEN:
        return state->listen;
    case UI_START_ROW_QUICK:
        return state->quick;
    case UI_START_ROW_NOTIME:
        return state->no_timing;
    case UI_START_ROW_RETRY:
        return state->retry;
    case UI_START_ROW_SAVE:
        return state->save;
    default:
        return false;
    }
}

static void Toggle(UiStartState *state, const UiStartInfo *info, uint32_t row) {
    if (!RowAvailable(info, row)) {
        return;
    }
    switch (row) {
    case UI_START_ROW_GRAPHICS:
    case UI_START_ROW_FS:
    case UI_START_ROW_NET:
    case UI_START_ROW_PAD:
        state->families ^= RowFamily(row);
        break;
    case UI_START_ROW_AUDIO:
        // The sequence plays before the audio suite's cases, so it goes when they do.
        state->families ^= MODE_FAMILY_AUDIO;
        state->listen = state->listen && (state->families & MODE_FAMILY_AUDIO) != 0u;
        break;
    case UI_START_ROW_LISTEN:
        state->listen = !state->listen;
        state->families |= state->listen ? MODE_FAMILY_AUDIO : 0u;
        break;
    case UI_START_ROW_QUICK:
        state->quick = !state->quick;
        break;
    case UI_START_ROW_NOTIME:
        state->no_timing = !state->no_timing;
        break;
    case UI_START_ROW_RETRY:
        state->retry = !state->retry;
        break;
    case UI_START_ROW_SAVE:
        state->save = !state->save;
        break;
    default:
        return;
    }
    state->changed = true;
}

UiStartState UiStartStep(UiStartState state, uint32_t pressed, uint32_t frame_index,
                         const UiStartInfo *info) {
    if (frame_index < UI_SETTLING_FRAMES || info == NULL || state.begun) {
        return state;
    }
    // X with Y in one edge is not a choice. The first frames are ignored because a pad that
    // arrives with the window reports its whole mask as newly pressed; one that connects
    // after those frames does the same in a single later edge, which would turn the crash list
    // on with no mode file asking. A still begins, and the choice stays as it was.
    if ((pressed & UI_START_Y) != 0u && (pressed & UI_START_TOGGLE_RETRY) != 0u) {
        pressed &= ~UI_START_TOGGLE_RETRY;
    }
    const bool select = (pressed & UI_START_SELECT) != 0u;
    if ((pressed & UI_START_BEGIN) != 0u || (select && state.cursor == UI_START_ROW_BEGIN)) {
        state.begun = true;
        state.changed = true;
        return state;
    }
    // L or R: the browser's tab after or before this one, the run not begun. Both in one edge
    // is a pad's whole mask, and nobody's choice.
    const uint32_t tabs = pressed & (UI_START_TAB_PREVIOUS | UI_START_TAB_NEXT);
    if (tabs == UI_START_TAB_NEXT || tabs == UI_START_TAB_PREVIOUS) {
        state.tab_asked = tabs == UI_START_TAB_NEXT ? (uint32_t)UI_TAB_SUITES
                                                    : (uint32_t)UI_TAB_OPTIONS;
        state.changed = true;
        return state;
    }
    const uint32_t moves = pressed & (UI_START_UP | UI_START_DOWN);
    if (select && moves != 0u) {
        // The same whole mask, away from begin the run: nobody chose that row.
    } else if (select) {
        Toggle(&state, info, state.cursor);
    } else if (moves == UI_START_UP || moves == UI_START_DOWN) {
        state.cursor = Around(state.cursor, moves == UI_START_UP ? -1 : 1, UI_START_ROWS);
        state.changed = true;
    }
    if ((pressed & UI_START_TOGGLE_RETRY) != 0u) {
        Toggle(&state, info, UI_START_ROW_RETRY);
    }
    return state;
}

RunMode UiStartMode(const UiStartState *state, RunMode from_file, uint32_t present) {
    RunMode mode = from_file;
    if (state == NULL) {
        return mode;
    }
    mode.retry = state->retry;
    mode.retry_by_screen = mode.retry && !from_file.retry;
    const uint32_t file_families = ModeFamiliesToRun(&from_file, present) & UI_START_FAMILIES;
    const uint32_t families = state->families & UI_START_FAMILIES & present;
    const bool families_changed = families != file_families || state->listen != from_file.listen;
    if (families_changed) {
        // Named whole, cpu with them, so even a run of nothing but cpu has a word to keep; a
        // family the screen has no row for stays as the file named it, the ones `all` named
        // among them, and `all` goes, or it would name the rows turned off again.
        mode.families_named = families | MODE_FAMILY_CPU |
                              (from_file.families_named & ~UI_START_FAMILIES) |
                              (ModeFamiliesNamed(&from_file, present) & ~UI_START_FAMILIES);
        mode.all = false;
        mode.listen = state->listen;
    }
    const bool timing_changed = state->no_timing != from_file.no_timing;
    mode.no_timing = state->no_timing;
    const bool quick_changed = state->quick != !from_file.detailed;
    if (quick_changed) {
        mode.quick = state->quick;
        mode.detailed = !state->quick;
        mode.detailed_by_button = false;
    }
    mode.chosen_on_screen = families_changed || timing_changed || quick_changed;
    mode.saved_by_screen = state->save;
    return mode;
}

bool UiStartTimesOut(uint64_t elapsed_ns, bool pressed) {
    return !pressed && elapsed_ns >= UI_START_NANOSECONDS;
}

uint32_t UiStartSecondsLeft(uint64_t elapsed_ns, bool pressed) {
    if (pressed || elapsed_ns >= UI_START_NANOSECONDS) {
        return 0u;
    }
    return (uint32_t)((UI_START_NANOSECONDS - elapsed_ns + 999999999ull) / 1000000000ull);
}

uint32_t UiStartWarningTop(uint32_t frame_height) {
    // At the bottom of the crash list's box, above the foot.
    const uint32_t height = UI_START_WARNING_LINES * LINE + 8u;
    return UI_BODY_BOTTOM(frame_height) > height ? UI_BODY_BOTTOM(frame_height) - height : 0u;
}

uint32_t UiStartRowTop(uint32_t row) {
    return UI_BODY_TOP + 6u + row * START_PITCH;
}

UiRect UiStartRowBox(uint32_t row) {
    const UiRect box = { LIST_X, UiStartRowTop(row), START_WIDTH, START_BOX };
    return box;
}

uint32_t UiStartNvnTop(void) {
    // At the bottom of the box beside the rows, under what the row does.
    return UI_BODY_TOP + START_HELP_HEIGHT - 2u * LINE - 6u;
}

UiRect UiStartNvnBox(uint32_t frame_width) {
    const uint32_t x = LIST_X + START_WIDTH + 2u * CW + CW / 2u;
    const UiRect box = { x, UiStartNvnTop(), frame_width > x + EDGE + CW ? frame_width - x - EDGE - CW : 0u,
                         2u * LINE };
    return box;
}

// What each row is called and what it does, in the order of UiStartRow; the crash list's row says
// how many cases it holds, so its words are written where it is drawn.
static const struct {
    const char *name;
    const char *what;
} START_ROWS[UI_START_ROWS] = {
    { "begin the run", "A here, or plus from any row, begins it as the rows say" },
    { "graphics", "the drawing suites, clear to blit2d" },
    { "fs", "reads from the memory card, timed" },
    { "audio", "the audio renderer, nearly silent" },
    { "listen", "about 40 s of sound to hear, before the audio suite" },
    { "net", "the online services, against sdmc:/edenprobe/net" },
    { "pad", "the controller, then the touch screen: needs a person" },
    { "quick", "leave when the report closes, with no results screens" },
    { "notime", "each drawing suite once, with no timing passes" },
    { "crash list", NULL },
    { "save", "write these choices into sdmc:/edenprobe/mode" },
};

// Each row's icon, in the order of UiStartRow.
static const Icon START_ICONS[UI_START_ROWS] = {
    ICON_PLAY,    ICON_IMAGE,   ICON_FOLDER, ICON_ACTIVITY,    ICON_HEADPHONES, ICON_WIFI,
    ICON_GAMEPAD, ICON_LOG_OUT, ICON_GAUGE,  ICON_ROTATE_CCW, ICON_SAVE,
};

// Across the row, in cells from the margin: the launcher's marker, the icon, the box, the name,
// and what it does.
#define START_WHAT_COLUMN 24u

/// One row of the menu, as a row of the launcher's: a box, inverted under the cursor, its marker
/// and icon, its name, and at its right whether it is on; in the soft colour where it cannot be
/// turned on.
static void DrawStartRow(const UiFrame *frame, uint32_t row, bool selected, bool available,
                         bool on) {
    const uint32_t y = UiStartRowTop(row);
    const uint8_t *const ink = ListRow(frame, LIST_X, y, START_WIDTH, START_BOX, selected);
    const uint8_t *const words = available || selected ? ink : DIM;
    BigGlyph(frame, LIST_X + CW / 2u, y + 1u, words, (unsigned char)FONTVGA_MARKER);
    DrawIcon(frame, LIST_X + 2u * CW, y + (START_BOX - 16u) / 2u, START_ICONS[row], words);
    char name[32];
    snprintf(name, sizeof(name), "%s", START_ROWS[row].name);
    name[0] = (char)(name[0] >= 'a' && name[0] <= 'z' ? name[0] - 'a' + 'A' : name[0]);
    Big(frame, LIST_X + 4u * CW, y + 1u, words, name);
    if (row != UI_START_ROW_BEGIN) {
        const bool risky = row == UI_START_ROW_RETRY;
        const char *const value = on ? "On" : "Off";
        const uint8_t *const colour = selected     ? ink
                                      : !available ? DIM
                                      : on         ? (risky ? SKIPPED : PASSED)
                                                   : DIM;
        Big(frame, LIST_X + START_WIDTH - CW / 2u - (uint32_t)strlen(value) * CW, y + 1u, colour,
            value);
    }
}

/// A list under its heading, in @p room lines at most at the font's own size: as many names as
/// fit, and a line saying how many more there are when not all of them do.
static uint32_t DrawStartList(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t right,
                              uint32_t room, uint32_t count, const char *const *suites,
                              const char *const *names) {
    if (count == 0u || room < 1u) {
        return y;
    }
    const uint32_t held = count < UI_START_LIST_MAX ? count : UI_START_LIST_MAX;
    const uint32_t fits = held <= room && held == count ? held : (room > 1u ? room - 1u : 0u);
    const uint32_t shown = fits < held ? fits : held;
    const uint32_t columns = right > x + 13u * GLYPH ? (right - x - 13u * GLYPH) / GLYPH : 1u;
    for (uint32_t i = 0; i < shown; ++i) {
        char suite[16];
        Fit(suite, sizeof(suite), suites[i], 10u);
        Say(frame, x, y, DIM, suite);
        char name[200];
        Fit(name, sizeof(name), names[i], columns);
        Say(frame, x + 11u * GLYPH, y, TEXT, name);
        y += LINE;
    }
    if (shown < count) {
        char more[48];
        snprintf(more, sizeof(more), "and %u more", (unsigned)(count - shown));
        Say(frame, x, y, DIM, more);
        y += LINE;
    }
    return y;
}

uint32_t UiStartKeys(UiKey *keys) {
    static const UiKey KEYS[] = {
        { "Up/Down", "select", 0 },
        { "A", "choose", UI_START_SELECT },
        { "Plus", "begin", UI_START_BEGIN },
        { "X", "crash list", UI_START_TOGGLE_RETRY },
        { "L/R", "tabs", 0 },
    };
    const uint32_t count = (uint32_t)(sizeof(KEYS) / sizeof(KEYS[0]));
    for (uint32_t i = 0; i < count; ++i) {
        keys[i] = KEYS[i];
    }
    return count;
}

/// What the start tab draws under the head, which the browser's start tab draws too.
static void ComposeStartBody(const UiFrame *frame, const UiStartInfo *info, UiStartState state,
                             const char *tip, const uint8_t *tip_colour, const UiKey *keys,
                             uint32_t key_count) {
    for (uint32_t row = 0; row < UI_START_ROWS; ++row) {
        DrawStartRow(frame, row, state.cursor == row, RowAvailable(info, row),
                     RowOn(&state, row));
    }

    // Beside the rows: what the one under the cursor does, and the crash list.
    const uint32_t x = LIST_X + START_WIDTH + 2u * CW;
    const uint32_t width = frame->width - EDGE - x;
    char name[32];
    snprintf(name, sizeof(name), "%s", START_ROWS[state.cursor].name);
    name[0] = (char)(name[0] >= 'a' && name[0] <= 'z' ? name[0] - 'a' + 'A' : name[0]);
    uint32_t y = Box(frame, x, UI_BODY_TOP, width, START_HELP_HEIGHT, name, NULL, NULL);
    char what[160];
    if (state.cursor == UI_START_ROW_RETRY) {
        snprintf(what, sizeof(what), "The crash list's %u case%s again; X turns it from any row",
                 (unsigned)info->listed_count, info->listed_count == 1u ? "" : "s");
    } else {
        snprintf(what, sizeof(what), "%s", START_ROWS[state.cursor].what);
        what[0] = (char)(what[0] >= 'a' && what[0] <= 'z' ? what[0] - 'a' + 'A' : what[0]);
    }
    const uint32_t columns = (width - CW) / CW;
    y += TextWrapped(frame, x + CW / 2u, y, SC, CH, TEXT, what, columns, 3u) * CH + 4u;
    TextWrapped(frame, x + CW / 2u, y, SC, CH, DIM,
                "The judge and the cpu suite run whatever is chosen", columns, 2u);
    if (info->nvn_named) {
        // Said where the person sees it before the run: the case's skip line says it after.
        const UiRect box = UiStartNvnBox(frame->width);
        TextWrapped(frame, box.x, box.y, 1u, LINE, SKIPPED, UI_START_NVN_SAID, box.width / GLYPH,
                    2u);
    }

    const bool warned = state.retry && info->listed_count > 0u;
    const uint32_t list_top = UI_BODY_TOP + START_HELP_HEIGHT + 10u;
    char count[24];
    snprintf(count, sizeof(count), "%u case%s", (unsigned)info->listed_count,
             info->listed_count == 1u ? "" : "s");
    y = Box(frame, x, list_top, width, UI_BODY_BOTTOM(frame->height) - list_top,
            "On the crash list", count, warned ? FAILED : NULL);
    const uint32_t warning_top = UiStartWarningTop(frame->height);
    const uint32_t room = warning_top > y + 4u ? (warning_top - y - 4u) / LINE : 0u;
    if (info->listed_count == 0u) {
        Say(frame, x + CW / 2u, y, DIM, "nothing has ended a program on this machine");
    }
    DrawStartList(frame, x + CW / 2u, y, x + width - CW / 2u, room, info->listed_count,
                  info->listed_suite, info->listed_name);
    if (warned) {
        static const char *const WARNING[UI_START_WARNING_LINES] = {
            "these cases ended an emulator's program; this one may end too",
            "if it does, start edenprobe again: the run goes on after the case",
            "that ended it, which is then skipped as crashed and listed",
        };
        FillRect(frame, x + 6u, warning_top, 3u, UI_START_WARNING_LINES * LINE, FAILED);
        for (uint32_t i = 0; i < UI_START_WARNING_LINES; ++i) {
            SayFitted(frame, x + CW, warning_top + i * LINE, FAILED, WARNING[i]);
        }
    }
    Foot(frame, tip, tip_colour, keys, key_count);
}

void UiComposeStart(const UiFrame *frame, const UiStartInfo *info, UiStartState state,
                    uint32_t seconds_left) {
    if (frame == NULL || frame->pixels == NULL || info == NULL) {
        return;
    }
    g_look = &MENU;
    if (frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        Page(frame, 0, 0, frame->width, frame->height);
        UiText(frame, 0, 0, 1, TEXT, "frame too small");
        return;
    }
    Head(frame, UI_TAB_START, "EDENPROBE  /  START", info->heading);
    char tip[160];
    const uint8_t *colour = TEXT;
    if (seconds_left > 0u) {
        snprintf(tip, sizeof(tip),
                 "Nothing pressed yet: the run begins in %u s as the mode file says; a button or "
                 "a touch keeps this",
                 (unsigned)seconds_left);
        colour = SKIPPED;
    } else {
        snprintf(tip, sizeof(tip), "Choose what runs, then begin it, or L/R to the suites");
    }
    UiKey keys[UI_FOOT_KEYS];
    const uint32_t count = UiStartKeys(keys);
    ComposeStartBody(frame, info, state, tip, colour, keys, count);
}

UiStartState UiStartTap(UiStartState state, const UiTap *tap, uint32_t frame_width,
                        uint32_t frame_height, const UiStartInfo *info) {
    if (tap == NULL || info == NULL || state.begun || frame_width < UI_MIN_WIDTH ||
        frame_height < UI_MIN_HEIGHT) {
        return state;
    }
    const UiTab tab = TabAt(tap);
    if (tab != UI_TABS) {
        if (tab != UI_TAB_START) {
            state.tab_asked = (uint32_t)tab;
            state.changed = true;
        }
        return state;
    }
    UiKey keys[UI_FOOT_KEYS];
    const uint32_t count = UiStartKeys(keys);
    const uint32_t key = UiFootKeyAt(tap, frame_width, frame_height, keys, count);
    if (key != UINT32_MAX) {
        return keys[key].input != 0
                   ? UiStartStep(state, (uint32_t)keys[key].input, UI_SETTLING_FRAMES, info)
                   : state;
    }
    // A row: both ends of the tap on its box or in the cursor's margin beside it.
    uint32_t row = UINT32_MAX;
    for (uint32_t r = 0; r < UI_START_ROWS; ++r) {
        const UiRect box = { LIST_X - 2u * CW, UiStartRowTop(r), START_WIDTH + 2u * CW,
                             START_BOX };
        if (Inside(box, tap->x0, tap->y0) && Inside(box, tap->x1, tap->y1)) {
            row = r;
        }
    }
    if (row == UINT32_MAX) {
        return state;
    }
    if (row != state.cursor) {
        state.cursor = row;
        state.changed = true;
        return state;
    }
    // Past the settling frames: the tap itself already waited for them.
    return UiStartStep(state, UI_START_SELECT, UI_SETTLING_FRAMES, info);
}

UiResumeState UiResumeOpen(void) {
    const UiResumeState state = { UI_RESUME_GO_ON, false, false, true };
    return state;
}

UiResumeState UiResumeStep(UiResumeState state, uint32_t pressed, uint32_t frame_index) {
    if (frame_index < UI_SETTLING_FRAMES || state.decided) {
        return state;
    }
    const uint32_t moves = pressed & (UI_RESUME_UP | UI_RESUME_DOWN);
    if ((pressed & UI_RESUME_CHOOSE) != 0u) {
        // With a move in the same edge it is a whole mask, not a choice; see the header.
        if (moves == 0u) {
            state.decided = true;
            state.start_over = state.cursor == UI_RESUME_START_OVER;
            state.changed = true;
        }
        return state;
    }
    if (moves == UI_RESUME_UP) {
        state.cursor = Around(state.cursor, -1, UI_RESUME_CHOICES);
        state.changed = true;
    } else if (moves == UI_RESUME_DOWN) {
        state.cursor = Around(state.cursor, 1, UI_RESUME_CHOICES);
        state.changed = true;
    }
    return state;
}

bool UiResumeTimesOut(uint64_t elapsed_ns, bool pressed) {
    return !pressed && elapsed_ns >= UI_RESUME_NANOSECONDS;
}

uint32_t UiResumeSecondsLeft(uint64_t elapsed_ns, bool pressed) {
    if (pressed || elapsed_ns >= UI_RESUME_NANOSECONDS) {
        return 0u;
    }
    return (uint32_t)((UI_RESUME_NANOSECONDS - elapsed_ns + 999999999ull) / 1000000000ull);
}

// Below the sentence of where it stopped, which takes up to three lines, and the three lines of
// how far the run got, with a line between.
#define RESUME_WHERE_LINES 3u
#define RESUME_ROWS_TOP (UI_SMALL_LIST_TOP + (RESUME_WHERE_LINES + 4u) * LINE)

uint32_t UiResumeRowTop(uint32_t row) {
    return RESUME_ROWS_TOP + row * 2u * LINE;
}

void UiComposeResume(const UiFrame *frame, const UiResumeInfo *info, UiResumeState state,
                     uint32_t seconds_left) {
    if (frame == NULL || frame->pixels == NULL || info == NULL) {
        return;
    }
    g_look = &MENU;
    Page(frame, 0, 0, frame->width, frame->height);
    if (frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        UiText(frame, 0, 0, 1, TEXT, "frame too small");
        return;
    }
    SayFitted(frame, MARGIN, HEAD_Y, TEXT, info->heading != NULL ? info->heading : "edenprobe");
    DrawSecondBox(frame);
    SayFitted(frame, MARGIN, SECOND_Y, SKIPPED,
              "the last run in this folder stopped before its end");

    TextWrapped(frame, MARGIN, UI_SMALL_LIST_TOP, 1u, LINE, TEXT,
                info->where != NULL && info->where[0] != '\0' ? info->where
                                                              : "where it stopped is not known",
                Columns(frame), RESUME_WHERE_LINES);
    uint32_t y = UI_SMALL_LIST_TOP + RESUME_WHERE_LINES * LINE;
    char text[160];
    snprintf(text, sizeof(text), "this is launch %u of the run; %u of its %u steps are done",
             (unsigned)info->launch, (unsigned)info->steps_done, (unsigned)info->steps);
    SayFitted(frame, MARGIN, y, DIM, text);
    y += LINE;
    const uint32_t x = Say(frame, MARGIN, y, DIM, "so far: ");
    SayCounts(frame, x, y, info->passed, info->failed, info->skipped, info->crashed);
    y += LINE;
    snprintf(text, sizeof(text), "folder: %s", info->folder != NULL ? info->folder : "");
    SayFitted(frame, MARGIN, y, DIM, text);

    static const char *const CHOICES[UI_RESUME_CHOICES][2] = {
        { "go on from where it stopped",
          "the report keeps what it has, and a case the program ended inside is skipped" },
        { "start over",
          "a new report in this folder; a case the program ended inside stays on the crash list" },
    };
    // Two rows of the launcher's, each two lines tall: the choice beside its icon, and what it
    // does under it.
    static const Icon CHOICE_ICONS[UI_RESUME_CHOICES] = { ICON_PLAY, ICON_ROTATE_CCW };
    for (uint32_t row = 0; row < UI_RESUME_CHOICES; ++row) {
        const uint32_t top = UiResumeRowTop(row);
        const uint8_t *const ink = DrawRowBox(frame, top, 2u * LINE, state.cursor == row);
        DrawGlyph(frame, MARGIN, top + 2u, 1u, ink, (unsigned char)FONTVGA_MARKER);
        DrawIcon(frame, MARGIN + 2u * GLYPH, top + 2u, CHOICE_ICONS[row], ink);
        SayFitted(frame, MARGIN + 5u * GLYPH, top + 2u, ink, CHOICES[row][0]);
        SayFitted(frame, MARGIN + 5u * GLYPH, top + LINE + 1u, ink, CHOICES[row][1]);
    }

    snprintf(text, sizeof(text),
             "nothing pressed yet: the run goes on in %u s, a button or a touch keeps this "
             "screen",
             (unsigned)seconds_left);
    DrawCountdown(frame, seconds_left > 0u ? text : NULL);
    static const Key KEYS[] = {
        { "up/down", "choose" },
        { "A", "confirm" },
        { "tap", "a choice twice" },
    };
    DrawKeys(frame, KEYS, sizeof(KEYS) / sizeof(KEYS[0]), frame->width - MARGIN);
}

UiResumeState UiResumeTap(UiResumeState state, const UiTap *tap, uint32_t frame_width,
                          uint32_t frame_height) {
    if (tap == NULL || state.decided || frame_width < UI_MIN_WIDTH ||
        frame_height < UI_MIN_HEIGHT) {
        return state;
    }
    const uint32_t row =
        TapRow(tap, frame_width, UiResumeRowTop(0), 2u * LINE, UI_RESUME_CHOICES);
    if (row == UINT32_MAX) {
        return state;
    }
    if (row != state.cursor) {
        state.cursor = row;
        state.changed = true;
        return state;
    }
    return UiResumeStep(state, UI_RESUME_CHOOSE, UI_SETTLING_FRAMES);
}

bool UiProgressDue(bool drawn_before, uint64_t drawn_ns, uint64_t now_ns, bool step_began) {
    return !drawn_before || step_began || now_ns < drawn_ns ||
           now_ns - drawn_ns >= UI_PROGRESS_NANOSECONDS;
}

void UiProgressCountReset(UiProgressCount *count) {
    if (count != NULL) {
        memset(count, 0, sizeof(*count));
    }
}

void UiProgressCountCase(UiProgressCount *count, uint32_t pass, bool started) {
    if (count == NULL) {
        return;
    }
    if (pass != count->pass) {
        // A pass begins with its first case: the one before it counted every case there is.
        count->previous = count->count;
        count->count = 0u;
        count->pass = pass;
        count->open = false;
    }
    if (started || !count->open) {
        ++count->count;
    }
    count->open = started;
}

size_t UiProgressPassWords(char *out, size_t out_size, uint32_t pass, uint32_t passes) {
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (pass == 0u || passes == 0u) {
        return 0;
    }
    const char *const what = pass == 1u        ? "the warm-up"
                             : pass < passes ? "a rehearsal, timed"
                                             : "the verdict pass, timed and reported";
    snprintf(out, out_size, "pass %u of %u: %s", (unsigned)pass, (unsigned)passes, what);
    return strlen(out);
}

size_t UiElapsedWords(char *out, size_t out_size, uint64_t ns) {
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    const uint64_t seconds = ns / 1000000000ull;
    if (seconds < 60u) {
        snprintf(out, out_size, "%u s", (unsigned)seconds);
    } else if (seconds < 3600u) {
        snprintf(out, out_size, "%u min %u s", (unsigned)(seconds / 60u),
                 (unsigned)(seconds % 60u));
    } else {
        snprintf(out, out_size, "%u h %u min", (unsigned)(seconds / 3600u),
                 (unsigned)(seconds % 3600u / 60u));
    }
    return strlen(out);
}

/// The steps done and the one under way, in proportion, across the frame below the step line:
/// the running screen's bar, which the listening screen draws too. The launcher's run screen
/// fills its bar in the ink colour and leaves the rest in the dim one; the step under way is in
/// the soft colour between them.
static void DrawStepsBarAt(const UiFrame *frame, uint32_t step, uint32_t steps, uint32_t x,
                           uint32_t top, uint32_t height) {
    const uint32_t width = frame->width - x - (x == LIST_X ? EDGE : x);
    FillRect(frame, x, top, width, height, BORDER);
    if (steps > 0u && step > 0u) {
        const uint32_t done = (uint32_t)((uint64_t)width * (step - 1u) / steps);
        const uint32_t under_way = (uint32_t)((uint64_t)width * step / steps) - done;
        FillRect(frame, x, top, done, height, TEXT);
        FillRect(frame, x + done, top, under_way, height, DIM);
    }
}

static void DrawStepsBar(const UiFrame *frame, uint32_t step, uint32_t steps) {
    DrawStepsBarAt(frame, step, steps, LIST_X, UI_PROGRESS_BAR_TOP, UI_PROGRESS_BAR_HEIGHT);
}

void UiComposeProgress(const UiFrame *frame, const UiProgress *progress) {
    if (frame == NULL || frame->pixels == NULL || progress == NULL) {
        return;
    }
    g_look = &RUN;
    if (frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        Page(frame, 0, 0, frame->width, frame->height);
        UiText(frame, 0, 0, 1, TEXT, "frame too small");
        return;
    }
    // The launcher's run screen: its colours, the title and the program at its right, the step
    // and its bar, the case under way, the counts.
    Head(frame, UI_TABS, "EDENPROBE  /  RUNNING", progress->heading);
    const uint32_t right = frame->width - EDGE;
    char text[200];
    const uint32_t step = progress->step < progress->steps ? progress->step : progress->steps;
    snprintf(text, sizeof(text), "Step %u of %u: %s", (unsigned)step, (unsigned)progress->steps,
             progress->step_name != NULL ? progress->step_name : "");
    BigFit(frame, LIST_X, UI_BODY_TOP + 2u, right, TEXT, text);

    DrawStepsBar(frame, step, progress->steps);

    uint32_t y = UI_PROGRESS_BAR_TOP + UI_PROGRESS_BAR_HEIGHT + 14u;
    if (UiProgressPassWords(text, sizeof(text), progress->pass, progress->passes) > 0u) {
        text[0] = (char)(text[0] >= 'a' && text[0] <= 'z' ? text[0] - 'a' + 'A' : text[0]);
        BigFit(frame, LIST_X, y, right, DIM, text);
    }
    y += CH;
    if (progress->name != NULL && progress->case_number > 0u) {
        if (progress->case_total >= progress->case_number) {
            snprintf(text, sizeof(text), "Case %u of %u", (unsigned)progress->case_number,
                     (unsigned)progress->case_total);
        } else {
            snprintf(text, sizeof(text), "Case %u", (unsigned)progress->case_number);
        }
        // The suite as the case's line names it, when it is not the step's own name: the net
        // step's six suites.
        const bool other = progress->suite != NULL && progress->step_name != NULL &&
                           strcmp(progress->suite, progress->step_name) != 0;
        if (other) {
            const size_t used = strlen(text);
            snprintf(text + used, sizeof(text) - used, ", in %s", progress->suite);
        }
        BigFit(frame, LIST_X, y, right, DIM, text);
        TextWrapped(frame, LIST_X, y + CH, SC, CH, TEXT, progress->name, (right - LIST_X) / CW,
                    2u);
    }
    y += 3u * CH + 10u;
    uint32_t x = Big(frame, LIST_X, y, DIM, "So far: ");
    const struct {
        uint32_t count;
        const char *word;
        const uint8_t *colour;
    } PARTS[4] = {
        { progress->passed, "passed", PASSED },
        { progress->failed, "failed", FAILED },
        { progress->skipped, "skipped", SKIPPED },
        { progress->crashed, "crashed", CRASHED },
    };
    for (uint32_t i = 0; i < 4u; ++i) {
        if (i == 3u && progress->crashed == 0u) {
            break;
        }
        snprintf(text, sizeof(text), "%u %s", (unsigned)PARTS[i].count, PARTS[i].word);
        x = Big(frame, x, y, PARTS[i].count > 0u ? PARTS[i].colour : DIM, text) + 2u * CW;
    }
    y += CH;
    char elapsed[32];
    UiElapsedWords(elapsed, sizeof(elapsed), progress->elapsed_ns);
    snprintf(text, sizeof(text), "Running for %s", elapsed);
    BigFit(frame, LIST_X, y, right, DIM, text);
    snprintf(text, sizeof(text), "folder: %s", progress->folder != NULL ? progress->folder : "");
    SayFitted(frame, LIST_X, y + CH + 4u, DIM, text);

    Foot(frame,
         progress->quick ? "Nothing to press: the program leaves when the run ends"
                         : "Nothing to press: the results come up when the run ends",
         DIM, NULL, 0u);
}

// The listening sequence, one row per screen, in the order a run with no button plays them.
// The guide is what a person is listening for; the log lines stay the sequence's own.
static const char *const LISTEN_A_LOW =
    "duller than the high beep, most in the hiss and the top of the sweep, means default "
    "quality is low-passing. the same sound means it is not.";
static const char *const LISTEN_A_HIGH =
    "the control. compare its hiss with the take before it.";
static const char *const LISTEN_B_PHRASE =
    "three low notes on the left, then three high notes on the right. the sink records them.";
static const char *const LISTEN_B_LEFT =
    "the low notes again means the sink recorded them. silence means it recorded nothing.";
static const char *const LISTEN_B_RIGHT =
    "the high notes again, on the right. silence means nothing was recorded.";
static const char *const LISTEN_C_LOW =
    "where mode 1 is read from its byte, this take is the quieter one. the same volume as "
    "mode 1 means the mode is ignored.";
static const char *const LISTEN_C_HIGH =
    "about 7 dB louder than mode 0 where the mode byte is read. the same volume means it is not.";
static const char *const LISTEN_REPEAT = "Y repeat   X next part";
static const char *const LISTEN_REPEAT_TAKE = "Y repeat this take   X next part";

static const struct {
    const char *mark;
    const char *name;
    const char *take;
    const char *guide;
    bool numbered;
    bool peaks;
} LISTEN[] = {
    { NULL, "listening", "three rising notes",
      "then part a, the resampler; part b, the circular buffer sink; part c, the limiter.",
      false, false },
    { "a", "resampler", "one beep, then four takes",
      "each take is noise bursts, a sweep to 16 kHz and a second of hiss.", false, false },
    { "a", "resampler", "take 1 of 4, low beep: default quality", LISTEN_A_LOW, true, false },
    { "a", "resampler", "take 2 of 4, high beep: high quality", LISTEN_A_HIGH, true, false },
    { "a", "resampler", "take 3 of 4, low beep: default quality", LISTEN_A_LOW, true, false },
    { "a", "resampler", "take 4 of 4, high beep: high quality", LISTEN_A_HIGH, true, false },
    { "b", "circular buffer sink", "two beeps, then the phrase", LISTEN_B_PHRASE, false, false },
    { "b", "circular buffer sink", "recording",
      "low notes on the left, then high notes on the right.", false, false },
    { "b", "circular buffer sink", "low beep: left channel of the recording, on the left",
      LISTEN_B_LEFT, true, true },
    { "b", "circular buffer sink", "high beep: right channel of the recording, on the right",
      LISTEN_B_RIGHT, true, true },
    { "c", "limiter", "three beeps, then four takes",
      "three notes through the limiter, threshold 0.2.", false, false },
    { "c", "limiter", "take 1 of 4, low beep: mode 0", LISTEN_C_LOW, true, false },
    { "c", "limiter", "take 2 of 4, high beep: mode 1", LISTEN_C_HIGH, true, false },
    { "c", "limiter", "take 3 of 4, low beep: mode 0", LISTEN_C_LOW, true, false },
    { "c", "limiter", "take 4 of 4, high beep: mode 1", LISTEN_C_HIGH, true, false },
    { NULL, "listening", "three falling notes", "the cases that follow are nearly silent.",
      false, false },
};

_Static_assert(sizeof(LISTEN) / sizeof(LISTEN[0]) == UI_LISTEN_CLOSING + 1u,
               "one row per listening screen");

UiListenInput UiListenInputFromButtons(uint32_t buttons, uint32_t frame_index) {
    if (frame_index < UI_SETTLING_FRAMES) {
        return UI_LISTEN_NONE;
    }
    // Moving on wins over hearing it again, so a mask with both does one thing.
    if ((buttons & UI_BUTTON_LISTEN_SKIP) != 0u) {
        return UI_LISTEN_SKIP;
    }
    if ((buttons & UI_BUTTON_LISTEN_REPEAT) != 0u) {
        return UI_LISTEN_REPEAT;
    }
    return UI_LISTEN_NONE;
}

void UiListenCopyOf(UiListenScreen screen, double left, double right, UiListenCopy *text,
                    char *guide, size_t guide_size) {
    if (text == NULL) {
        return;
    }
    if ((unsigned)screen >= sizeof(LISTEN) / sizeof(LISTEN[0])) {
        text->mark = NULL;
        text->name = "";
        text->take = "";
        text->guide = "";
        text->buttons = LISTEN_REPEAT;
        return;
    }
    text->mark = LISTEN[screen].mark;
    text->name = LISTEN[screen].name;
    text->take = LISTEN[screen].take;
    text->buttons = LISTEN[screen].numbered ? LISTEN_REPEAT_TAKE : LISTEN_REPEAT;
    text->guide = LISTEN[screen].guide;
    if (LISTEN[screen].peaks && guide != NULL && guide_size > 0u) {
        snprintf(guide, guide_size, "%s peak %.3f on the left and %.3f on the right (0 is silence).",
                 LISTEN[screen].guide, left, right);
        text->guide = guide;
    }
}

UiListenScreen UiListenNext(UiListenScreen screen) {
    if (screen >= UI_LISTEN_CLOSING) {
        return UI_LISTEN_DONE;
    }
    return (UiListenScreen)(screen + 1);
}

UiListenScreen UiListenSkip(UiListenScreen screen) {
    if (screen <= UI_LISTEN_OPENING) {
        return UI_LISTEN_A_BEEP;
    }
    if (screen <= UI_LISTEN_A4) {
        return UI_LISTEN_B_BEEP;
    }
    if (screen <= UI_LISTEN_B_RIGHT) {
        return UI_LISTEN_C_BEEP;
    }
    if (screen <= UI_LISTEN_C4) {
        return UI_LISTEN_CLOSING;
    }
    return UI_LISTEN_DONE;
}

void UiComposeListen(const UiFrame *frame, UiListenScreen screen, double left, double right) {
    if (frame == NULL || frame->pixels == NULL) {
        return;
    }
    g_look = &RUN;
    if (frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        Page(frame, 0, 0, frame->width, frame->height);
        UiText(frame, 0, 0, 1, TEXT, "frame too small");
        return;
    }
    char guide[192];
    UiListenCopy text;
    UiListenCopyOf(screen, left, right, &text, guide, sizeof(guide));

    // The running screen's layout, which comes before and after it: the title, what is playing
    // at its right, the step and its bar, and what to listen for below the bar.
    char line[200];
    if (text.mark != NULL) {
        snprintf(line, sizeof(line), "part %s: %s", text.mark, text.name);
    } else {
        snprintf(line, sizeof(line), "%s", screen == UI_LISTEN_OPENING ? "before the parts"
                                                                       : "after the parts");
    }
    Head(frame, UI_TABS, "EDENPROBE  /  LISTENING", line);

    const uint32_t steps = UI_LISTEN_CLOSING + 1u;
    const uint32_t step = screen < steps ? (uint32_t)screen + 1u : steps;
    snprintf(line, sizeof(line), "Step %u of %u: %s", (unsigned)step, (unsigned)steps,
             text.take);
    BigFit(frame, LIST_X, UI_BODY_TOP + 2u, frame->width - EDGE, TEXT, line);
    DrawStepsBar(frame, step, steps);

    const uint32_t y = UI_PROGRESS_BAR_TOP + UI_PROGRESS_BAR_HEIGHT + 14u;
    Big(frame, LIST_X, y, DIM, "Listen for");
    const uint32_t top = y + CH + 4u;
    const uint32_t bottom = UI_BODY_BOTTOM(frame->height);
    const uint32_t room = bottom > top + LINE ? (bottom - top) / LINE : 1u;
    TextWrapped(frame, LIST_X, top, 1u, LINE, TEXT, text.guide,
                (frame->width - EDGE - LIST_X) / GLYPH, room);

    // Its buttons as the sentence names them, "Y repeat   X next part": a key and what it does,
    // apart from the next by more than one space.
    UiKey keys[4];
    char words[4][64];
    uint32_t count = 0;
    for (const char *at = text.buttons; *at != '\0' && count < 4u;) {
        const char *end = strstr(at, "  ");
        const size_t length = end != NULL ? (size_t)(end - at) : strlen(at);
        const size_t taken = length < sizeof(words[0]) ? length : sizeof(words[0]) - 1u;
        memcpy(words[count], at, taken);
        words[count][taken] = '\0';
        char *const space = strchr(words[count], ' ');
        if (space != NULL) {
            *space = '\0';
        }
        keys[count].key = words[count];
        keys[count].what = space != NULL ? space + 1 : "";
        keys[count].input = 0;
        ++count;
        at += length;
        while (*at == ' ') {
            ++at;
        }
    }
    Foot(frame, "About 40 seconds of sound to hear, before the audio suite's cases", DIM, keys,
         count);
}

// The pad screen's two stick boxes, side by side at the right of the live lines: each stick's
// square of travel, with a mark where it is now.
#define PAD_BOX 112u
#define PAD_MARK 8u

static void DrawStickBox(const UiFrame *frame, uint32_t x, uint32_t y, const char *label,
                         int32_t stick_x, int32_t stick_y) {
    FillRect(frame, x, y, PAD_BOX, PAD_BOX, BORDER);
    FillRect(frame, x + 2u, y + 2u, PAD_BOX - 4u, PAD_BOX - 4u, BACKGROUND);
    FillRect(frame, x + PAD_BOX / 2u, y + 2u, 1u, PAD_BOX - 4u, BORDER);
    FillRect(frame, x + 2u, y + PAD_BOX / 2u, PAD_BOX - 4u, 1u, BORDER);
    // Up on the stick is up on the screen, which is the other way from the rows.
    const int64_t span = (int64_t)(PAD_BOX - PAD_MARK);
    int64_t cx = ((int64_t)stick_x + PAD_STICK_FULL) * span / (2 * PAD_STICK_FULL);
    int64_t cy = ((int64_t)PAD_STICK_FULL - stick_y) * span / (2 * PAD_STICK_FULL);
    cx = cx < 0 ? 0 : cx > span ? span : cx;
    cy = cy < 0 ? 0 : cy > span ? span : cy;
    FillRect(frame, x + (uint32_t)cx, y + (uint32_t)cy, PAD_MARK, PAD_MARK, TEXT);
    Say(frame, x, y + PAD_BOX + 4u, DIM, label);
}

void UiComposePad(const UiFrame *frame, const UiPadScreen *screen) {
    if (frame == NULL || frame->pixels == NULL || screen == NULL) {
        return;
    }
    g_look = &RUN;
    Page(frame, 0, 0, frame->width, frame->height);
    if (frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        UiText(frame, 0, 0, 1, TEXT, "frame too small");
        return;
    }
    // The running screen's layout, as the listening screen keeps it: the word and the program on
    // the top row, a dim second row, the prompt and its bar where a list would start.
    Say(frame, MARGIN, HEAD_Y, TEXT, "controller");
    SayFitted(frame, MARGIN + 12u * GLYPH, HEAD_Y, DIM,
              screen->heading != NULL ? screen->heading : "edenprobe");
    SayFitted(frame, MARGIN, SECOND_Y, DIM,
              "press what the screen asks; nothing for 10 s skips the prompt");

    char line[200];
    const uint32_t prompt = screen->prompt < screen->prompts ? screen->prompt : screen->prompts;
    snprintf(line, sizeof(line), "prompt %u of %u", (unsigned)prompt, (unsigned)screen->prompts);
    SayFitted(frame, MARGIN, UI_SMALL_LIST_TOP, TEXT, line);
    DrawStepsBarAt(frame, prompt, screen->prompts, MARGIN, UI_PAD_BAR_TOP, UI_PAD_BAR_HEIGHT);

    // Large enough to read from a sofa: six times the cell, as long as it fits across.
    uint32_t y = UI_PAD_BAR_TOP + UI_PAD_BAR_HEIGHT + 24u;
    const char *const big = screen->big != NULL ? screen->big : "";
    const uint32_t room = frame->width - 2u * MARGIN;
    uint32_t scale = UI_PAD_BIG;
    while (scale > 1u && (uint32_t)strlen(big) * CELL * scale > room) {
        --scale;
    }
    UiText(frame, MARGIN, y, scale, TEXT, big);
    y += FONTVGA_HEIGHT * UI_PAD_BIG + 16u;
    y += TextWrapped(frame, MARGIN, y, 1u, LINE, TEXT, screen->how, Columns(frame), 2u) * LINE;
    if (screen->state != NULL) {
        SayFitted(frame, MARGIN, y + 4u, DIM, screen->state);
    }

    // What the controller sends now, in the bits the suite reads, beside the two sticks.
    const uint32_t live = y + 2u * LINE + 8u;
    const uint32_t boxes = frame->width - MARGIN - 2u * PAD_BOX - 24u;
    char held[160];
    PadButtonWords(held, sizeof(held), screen->held);
    snprintf(line, sizeof(line), "held: %s", held);
    TextWrapped(frame, MARGIN, live, 1u, LINE, TEXT, line, (boxes - MARGIN) / GLYPH, 2u);
    snprintf(line, sizeof(line), "left stick  x %6d  y %6d", (int)screen->left_x,
             (int)screen->left_y);
    Say(frame, MARGIN, live + 2u * LINE, DIM, line);
    snprintf(line, sizeof(line), "right stick x %6d  y %6d", (int)screen->right_x,
             (int)screen->right_y);
    Say(frame, MARGIN, live + 3u * LINE, DIM, line);
    snprintf(line, sizeof(line), "connected: %s",
             screen->controllers != NULL ? screen->controllers : "nothing");
    TextWrapped(frame, MARGIN, live + 4u * LINE, 1u, LINE, DIM, line, (boxes - MARGIN) / GLYPH,
                2u);
    DrawStickBox(frame, boxes, live, "left", screen->left_x, screen->left_y);
    DrawStickBox(frame, boxes + PAD_BOX + 24u, live, "right", screen->right_x, screen->right_y);

    if (screen->last != NULL) {
        const uint32_t last = live + PAD_BOX + 2u * LINE;
        const uint32_t x = Say(frame, MARGIN, last, DIM, "before: ");
        TextWrapped(frame, x, last, 1u, LINE, ResultColour(screen->last_result), screen->last,
                    (frame->width - MARGIN - x) / GLYPH, 2u);
    }
    DrawFootNote(frame,
                 "Home and Capture are left out: the system takes them. A question: up for yes, "
                 "down for no, or tap",
                 KeysRight(frame, 2u));
    if (screen->asks_yes_no) {
        static const char *const ANSWERS[2] = { "yes", "no" };
        DrawTouchBar(frame, ANSWERS, 2u);
    }
}

/// A ring with a cross through it, centred on @p x, @p y.
static void DrawMark(const UiFrame *frame, uint32_t x, uint32_t y, const uint8_t colour[4]) {
    const int32_t r = (int32_t)UI_TOUCH_MARK_RADIUS;
    for (int32_t dy = -r; dy <= r; ++dy) {
        for (int32_t dx = -r; dx <= r; ++dx) {
            const int32_t d2 = dx * dx + dy * dy;
            if (d2 <= r * r && d2 >= (r - 3) * (r - 3) && (int32_t)x + dx >= 0 &&
                (int32_t)y + dy >= 0) {
                PutPixel(frame, (uint32_t)((int32_t)x + dx), (uint32_t)((int32_t)y + dy), colour);
            }
        }
    }
    const uint32_t arm = UI_TOUCH_MARK_RADIUS + 8u;
    if (x >= arm && y >= arm) {
        FillRect(frame, x - arm, y - 1u, 2u * arm, 2u, colour);
        FillRect(frame, x - 1u, y - arm, 2u, 2u * arm, colour);
    }
}

void UiComposeTouch(const UiFrame *frame, const UiTouchScreen *screen) {
    if (frame == NULL || frame->pixels == NULL || screen == NULL) {
        return;
    }
    g_look = &RUN;
    Page(frame, 0, 0, frame->width, frame->height);
    if (frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        UiText(frame, 0, 0, 1, TEXT, "frame too small");
        return;
    }
    Say(frame, MARGIN, HEAD_Y, TEXT, "touch");
    SayFitted(frame, MARGIN + 7u * GLYPH, HEAD_Y, DIM,
              screen->heading != NULL ? screen->heading : "edenprobe");
    char line[200];
    const uint32_t prompt = screen->prompt < screen->prompts ? screen->prompt : screen->prompts;
    snprintf(line, sizeof(line), "prompt %u of %u: touch what the screen asks; nothing for 10 s "
                                 "skips it",
             (unsigned)prompt, (unsigned)screen->prompts);
    SayFitted(frame, MARGIN, SECOND_Y, DIM, line);

    // Between the top targets and the middle one, four times the cell as long as it fits.
    uint32_t y = UI_TOUCH_WORDS_TOP;
    const char *const big = screen->big != NULL ? screen->big : "";
    uint32_t scale = 4u;
    while (scale > 1u && (uint32_t)strlen(big) * CELL * scale > frame->width - 2u * MARGIN) {
        --scale;
    }
    UiText(frame, MARGIN, y, scale, TEXT, big);
    y += FONTVGA_HEIGHT * 4u + 16u;
    y += TextWrapped(frame, MARGIN, y, 1u, LINE, TEXT, screen->how, Columns(frame), 2u) * LINE;
    if (screen->state != NULL) {
        SayFitted(frame, MARGIN, y + 4u, DIM, screen->state);
    }

    // Between the middle target and the drag's row.
    if (screen->now != NULL) {
        SayFitted(frame, MARGIN, UI_TOUCH_LIVE_TOP, DIM, screen->now);
    }
    if (screen->last != NULL) {
        const uint32_t last = UI_TOUCH_LIVE_TOP + LINE + 12u;
        const uint32_t x = Say(frame, MARGIN, last, DIM, "before: ");
        TextWrapped(frame, x, last, 1u, LINE, ResultColour(screen->last_result), screen->last,
                    (frame->width - MARGIN - x) / GLYPH, 2u);
    }

    const uint32_t marks = screen->marks < 2u ? screen->marks : 2u;
    if (marks == 2u) {
        // The drag's row, from one end to the other.
        const uint32_t from = screen->mark_x[0] < screen->mark_x[1] ? screen->mark_x[0]
                                                                     : screen->mark_x[1];
        const uint32_t to = screen->mark_x[0] < screen->mark_x[1] ? screen->mark_x[1]
                                                                   : screen->mark_x[0];
        FillRect(frame, from, screen->mark_y[0], to - from, 2u, BORDER);
    }
    for (uint32_t i = 0; i < marks; ++i) {
        DrawMark(frame, screen->mark_x[i], screen->mark_y[i], SKIPPED);
    }
    const uint32_t touches = screen->touches < 4u ? screen->touches : 4u;
    for (uint32_t i = 0; i < touches; ++i) {
        const uint32_t tx = screen->touch_x[i] > 6u ? screen->touch_x[i] - 6u : 0u;
        const uint32_t ty = screen->touch_y[i] > 6u ? screen->touch_y[i] - 6u : 0u;
        FillRect(frame, tx, ty, 12u, 12u, PASSED);
    }
    DrawFootNote(frame, "lift every finger between prompts; a docked console's touch screen is off",
                 frame->width - MARGIN);
}

PadAnswer UiPadTap(const UiTap *tap, uint32_t frame_width, uint32_t frame_height,
                   bool asks_yes_no) {
    if (tap == NULL || !asks_yes_no) {
        return PAD_ANSWER_NONE;
    }
    const uint32_t box = TapBox(tap, frame_width, frame_height, 2u);
    return box == 0u ? PAD_ANSWER_YES : box == 1u ? PAD_ANSWER_NO : PAD_ANSWER_NONE;
}

// ---- The browser ------------------------------------------------------------------------------

// The suites the browser lists, each FAMILIES row of a family the browser shows, the families in
// their order and each one's suites in the table's: worked out the first time it is asked.
static uint8_t browse_suites[UI_BROWSE_SUITES];
static uint32_t browse_suite_count;

static void ListBrowseSuites(void) {
    if (browse_suite_count > 0u) {
        return;
    }
    for (uint32_t family = 0; family < UI_FAMILY_OTHER; ++family) {
        for (size_t i = 0; i < sizeof(FAMILIES) / sizeof(FAMILIES[0]); ++i) {
            if (FAMILIES[i].family == (UiFamily)family &&
                browse_suite_count < UI_BROWSE_SUITES) {
                browse_suites[browse_suite_count++] = (uint8_t)i;
            }
        }
    }
}

_Static_assert(sizeof(FAMILIES) / sizeof(FAMILIES[0]) <= UI_BROWSE_SUITES,
               "every suite the screens know has a row in the browser");

uint32_t UiBrowseSuiteCount(void) {
    ListBrowseSuites();
    return browse_suite_count;
}

const char *UiBrowseSuite(uint32_t index) {
    ListBrowseSuites();
    return index < browse_suite_count ? FAMILIES[browse_suites[index]].suite : NULL;
}

UiFamily UiBrowseSuiteFamily(uint32_t index) {
    ListBrowseSuites();
    return index < browse_suite_count ? FAMILIES[browse_suites[index]].family : UI_FAMILY_OTHER;
}

uint32_t UiBrowseSuiteIndex(const char *suite) {
    for (uint32_t i = 0; suite != NULL && i < UiBrowseSuiteCount(); ++i) {
        if (strcmp(UiBrowseSuite(i), suite) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

/// The families the browser lists, in their order: every one but other.
#define BROWSE_FAMILIES ((uint32_t)UI_FAMILY_OTHER)

/// The suites of @p family: how many, and the index of the @p n th.
static uint32_t FamilySuites(UiFamily family) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < UiBrowseSuiteCount(); ++i) {
        count += UiBrowseSuiteFamily(i) == family ? 1u : 0u;
    }
    return count;
}

static uint32_t FamilySuiteAt(UiFamily family, uint32_t n) {
    for (uint32_t i = 0; i < UiBrowseSuiteCount(); ++i) {
        if (UiBrowseSuiteFamily(i) == family && n-- == 0u) {
            return i;
        }
    }
    return UINT32_MAX;
}

/// Where suite @p suite sits among its family's.
static uint32_t PlaceInFamily(uint32_t suite) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < suite && i < UiBrowseSuiteCount(); ++i) {
        n += UiBrowseSuiteFamily(i) == UiBrowseSuiteFamily(suite) ? 1u : 0u;
    }
    return n;
}

uint32_t UiBrowseCases(const UiBrowseInfo *info, uint32_t suite) {
    const char *const name = UiBrowseSuite(suite);
    uint32_t count = 0;
    for (uint32_t i = 0; info != NULL && name != NULL && i < info->catalogue_count; ++i) {
        count += strcmp(info->catalogue[i].suite, name) == 0 ? 1u : 0u;
    }
    return count;
}

uint32_t UiBrowseCaseAt(const UiBrowseInfo *info, uint32_t suite, uint32_t n) {
    const char *const name = UiBrowseSuite(suite);
    for (uint32_t i = 0; info != NULL && name != NULL && i < info->catalogue_count; ++i) {
        if (strcmp(info->catalogue[i].suite, name) == 0 && n-- == 0u) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool Bit(const uint8_t *bits, uint32_t index) {
    return (bits[index / 8u] >> (index % 8u) & 1u) != 0u;
}

static void SetBit(uint8_t *bits, uint32_t index, bool on) {
    if (on) {
        bits[index / 8u] = (uint8_t)(bits[index / 8u] | (1u << (index % 8u)));
    } else {
        bits[index / 8u] = (uint8_t)(bits[index / 8u] & ~(1u << (index % 8u)));
    }
}

/// The judge's suites always run, so they are never marked.
static bool Markable(uint32_t suite) {
    return UiBrowseSuiteFamily(suite) != UI_FAMILY_JUDGE;
}

/// How much of suite @p suite the marks hold.
static Mark SuiteMark(const UiBrowseState *state, const UiBrowseInfo *info, uint32_t suite) {
    if (Bit(state->suite_marks, suite)) {
        return MARK_ALL;
    }
    const uint32_t cases = UiBrowseCases(info, suite);
    for (uint32_t n = 0; n < cases; ++n) {
        const uint32_t at = UiBrowseCaseAt(info, suite, n);
        if (at < UI_MAX_CASES && Bit(state->case_marks, at)) {
            return MARK_SOME;
        }
    }
    return MARK_NONE;
}

static Mark FamilyMark(const UiBrowseState *state, const UiBrowseInfo *info, UiFamily family) {
    const uint32_t count = FamilySuites(family);
    uint32_t whole = 0;
    bool some = false;
    for (uint32_t n = 0; n < count; ++n) {
        const Mark mark = SuiteMark(state, info, FamilySuiteAt(family, n));
        whole += mark == MARK_ALL ? 1u : 0u;
        some = some || mark != MARK_NONE;
    }
    return count > 0u && whole == count ? MARK_ALL : some ? MARK_SOME : MARK_NONE;
}

static void MarkSuite(UiBrowseState *state, const UiBrowseInfo *info, uint32_t suite, bool on) {
    if (!Markable(suite)) {
        return;
    }
    SetBit(state->suite_marks, suite, on);
    const uint32_t cases = UiBrowseCases(info, suite);
    for (uint32_t n = 0; n < cases; ++n) {
        const uint32_t at = UiBrowseCaseAt(info, suite, n);
        if (at < UI_MAX_CASES) {
            SetBit(state->case_marks, at, false);
        }
    }
}

/// X on the row under the cursor: a family or a suite whole, or off when it was; a case alone,
/// and in a suite marked whole, every case of it but this one; a demo.
static void ToggleMark(UiBrowseState *state, const UiBrowseInfo *info) {
    if (state->tab == UI_TAB_DEMOS) {
        if (state->demo < info->demo_count && state->demo < 32u) {
            state->demo_marks ^= 1u << state->demo;
        }
        return;
    }
    if (state->level == 0u) {
        const UiFamily family = (UiFamily)state->family;
        const bool on = FamilyMark(state, info, family) != MARK_ALL;
        for (uint32_t n = 0; n < FamilySuites(family); ++n) {
            MarkSuite(state, info, FamilySuiteAt(family, n), on);
        }
    } else if (state->level == 1u) {
        MarkSuite(state, info, state->suite, SuiteMark(state, info, state->suite) != MARK_ALL);
    } else if (Markable(state->suite)) {
        const uint32_t at = UiBrowseCaseAt(info, state->suite, state->item);
        if (at >= UI_MAX_CASES) {
            return;
        }
        if (Bit(state->suite_marks, state->suite)) {
            SetBit(state->suite_marks, state->suite, false);
            for (uint32_t n = 0; n < UiBrowseCases(info, state->suite); ++n) {
                const uint32_t other = UiBrowseCaseAt(info, state->suite, n);
                if (other < UI_MAX_CASES) {
                    SetBit(state->case_marks, other, other != at);
                }
            }
        } else {
            SetBit(state->case_marks, at, !Bit(state->case_marks, at));
        }
    }
    state->changed = true;
}

const uint32_t UI_DEMO_FRAMES[UI_DEMO_FRAME_CHOICES] = { 120u, 600u, 3600u, 36000u };

UiOptions UiOptionsFrom(const RunMode *mode) {
    UiOptions options;
    memset(&options, 0, sizeof(options));
    options.timing = mode == NULL || !mode->no_timing;
    options.every_picture = mode != NULL && mode->every_case;
    options.retry = mode != NULL && mode->retry;
    options.frames = UI_DEMO_FRAMES[1];
    const uint32_t asked = mode != NULL ? mode->frames : MODE_FRAMES_DEFAULT;
    for (uint32_t i = UI_DEMO_FRAME_CHOICES; i-- > 0u;) {
        if (UI_DEMO_FRAMES[i] >= asked) {
            options.frames = UI_DEMO_FRAMES[i];
        }
    }
    options.music = true;
    options.motion = true;
    options.hud = true;
    return options;
}

UiBrowseState UiBrowseOpen(UiTab tab, const UiStartState *start, UiOptions options,
                           const UiBrowseInfo *info) {
    UiBrowseState state;
    memset(&state, 0, sizeof(state));
    state.tab = tab < UI_TABS ? tab : UI_TAB_SUITES;
    if (start != NULL) {
        state.start = *start;
        state.start.begun = false;
        state.start.tab_asked = 0u;
    }
    state.options = options;
    state.family = (uint32_t)UI_FAMILY_GRAPHICS;
    state.suite = FamilySuiteAt(UI_FAMILY_GRAPHICS, 0u);
    state.results = UiStart(info != NULL ? info->results : NULL);
    state.results.browser = true;
    state.changed = true;
    return state;
}

/// The start tab's buttons from the browser's.
static uint32_t StartButtons(uint32_t buttons) {
    static const struct {
        uint32_t from;
        uint32_t to;
    } MAP[] = {
        { UI_BUTTON_UP, UI_START_UP },
        { UI_BUTTON_DOWN, UI_START_DOWN },
        { UI_BUTTON_OPEN, UI_START_SELECT },
        { UI_BUTTON_RUN, UI_START_BEGIN },
        { UI_BUTTON_CYCLE_VIEW, UI_START_TOGGLE_RETRY },
        { UI_BUTTON_AGAIN, UI_START_Y },
    };
    uint32_t out = 0;
    for (size_t i = 0; i < sizeof(MAP) / sizeof(MAP[0]); ++i) {
        out |= (buttons & MAP[i].from) != 0u ? MAP[i].to : 0u;
    }
    return out;
}

static void StepSuites(UiBrowseState *state, uint32_t buttons, const UiBrowseInfo *info) {
    const UiFamily family = (UiFamily)state->family;
    if (state->level == 0u) {
        if ((buttons & (UI_BUTTON_UP | UI_BUTTON_DOWN)) != 0u) {
            state->family = Around(state->family, (buttons & UI_BUTTON_UP) != 0u ? -1 : 1,
                                   BROWSE_FAMILIES);
        } else if ((buttons & UI_BUTTON_OPEN) != 0u && FamilySuites(family) > 0u) {
            state->level = 1u;
            state->suite = FamilySuiteAt(family, 0u);
        }
    } else if (state->level == 1u) {
        const uint32_t count = FamilySuites(family);
        const uint32_t place = PlaceInFamily(state->suite);
        if ((buttons & (UI_BUTTON_UP | UI_BUTTON_DOWN)) != 0u && count > 0u) {
            state->suite = FamilySuiteAt(
                family, Around(place, (buttons & UI_BUTTON_UP) != 0u ? -1 : 1, count));
        } else if ((buttons & (UI_BUTTON_LEFT | UI_BUTTON_RIGHT)) != 0u) {
            // The family before or after, on its first suite; one with none is passed over.
            uint32_t next = state->family;
            do {
                next = Around(next, (buttons & UI_BUTTON_LEFT) != 0u ? -1 : 1, BROWSE_FAMILIES);
            } while (FamilySuites((UiFamily)next) == 0u && next != state->family);
            state->family = next;
            state->suite = FamilySuiteAt((UiFamily)next, 0u);
        } else if ((buttons & UI_BUTTON_OPEN) != 0u) {
            state->level = 2u;
            state->item = 0u;
        } else if ((buttons & UI_BUTTON_BACK) != 0u) {
            state->level = 0u;
        }
    } else {
        const uint32_t cases = UiBrowseCases(info, state->suite);
        if ((buttons & (UI_BUTTON_UP | UI_BUTTON_DOWN)) != 0u && cases > 0u) {
            state->item = Around(state->item, (buttons & UI_BUTTON_UP) != 0u ? -1 : 1, cases);
        } else if ((buttons & (UI_BUTTON_LEFT | UI_BUTTON_RIGHT)) != 0u) {
            const uint32_t count = FamilySuites(family);
            state->suite = FamilySuiteAt(
                family, Around(PlaceInFamily(state->suite),
                               (buttons & UI_BUTTON_LEFT) != 0u ? -1 : 1, count));
            state->item = 0u;
        } else if ((buttons & UI_BUTTON_OPEN) != 0u && cases > 0u &&
                   Markable(state->suite)) {
            state->ask = UI_ASK_RUN_ONE;
        } else if ((buttons & UI_BUTTON_BACK) != 0u) {
            state->level = 1u;
        }
    }
}

/// The results tab after its own step or tap: B on its overview goes to the suites, and its run
/// again and its quit are the browser's to do.
static UiBrowseState ResultsAsked(UiBrowseState state) {
    if (state.results.back) {
        state.results.back = false;
        state.tab = UI_TAB_SUITES;
    }
    if (state.results.again) {
        state.results.again = false;
        state.ask = UI_ASK_AGAIN;
    }
    if (state.results.quit) {
        state.results.quit = false;
        state.ask = UI_ASK_QUIT;
    }
    return state;
}

UiBrowseState UiBrowseStep(UiBrowseState state, uint32_t buttons, uint32_t frame_index,
                           const UiBrowseInfo *info) {
    state.ask = UI_ASK_NONE;
    if (frame_index < UI_SETTLING_FRAMES || info == NULL || buttons == 0u) {
        return state;
    }
    const UiBrowseState before = state;
    state.pressed = true;
    // Leaving first, as the results do; then the tabs, which every tab has.
    const uint32_t tabs = buttons & (UI_BUTTON_TAB_PREVIOUS | UI_BUTTON_TAB_NEXT);
    if ((buttons & UI_BUTTON_QUIT) != 0u) {
        state.ask = UI_ASK_QUIT;
    } else if (tabs == UI_BUTTON_TAB_NEXT || tabs == UI_BUTTON_TAB_PREVIOUS) {
        state.tab = (UiTab)Around(state.tab, tabs == UI_BUTTON_TAB_NEXT ? 1 : -1, UI_TABS);
    } else if (tabs != 0u) {
        // Both in one edge: a pad's whole mask, nobody's choice.
    } else if (state.tab == UI_TAB_START) {
        state.start = UiStartStep(state.start, StartButtons(buttons), UI_SETTLING_FRAMES,
                                  info->start);
        if (state.start.begun) {
            state.start.begun = false;
            state.ask = UI_ASK_BEGIN;
        }
    } else if (state.tab == UI_TAB_RESULTS) {
        state.results.browser = true;
        state.results = UiStep(state.results,
                               UiInputFromButtons(buttons & ~UI_BUTTON_RUN, UI_SETTLING_FRAMES),
                               info->results);
        state = ResultsAsked(state);
    } else if ((buttons & UI_BUTTON_RUN) != 0u) {
        state.ask = UI_ASK_RUN;
    } else if ((buttons & UI_BUTTON_CYCLE_VIEW) != 0u && state.tab != UI_TAB_OPTIONS) {
        ToggleMark(&state, info);
    } else if (state.tab == UI_TAB_SUITES) {
        StepSuites(&state, buttons, info);
    } else if (state.tab == UI_TAB_DEMOS) {
        if ((buttons & (UI_BUTTON_UP | UI_BUTTON_DOWN)) != 0u && info->demo_count > 0u) {
            state.demo =
                Around(state.demo, (buttons & UI_BUTTON_UP) != 0u ? -1 : 1, info->demo_count);
        } else if ((buttons & UI_BUTTON_OPEN) != 0u && info->demo_count > 0u) {
            state.ask = UI_ASK_RUN_ONE;
        }
    } else if (state.tab == UI_TAB_OPTIONS) {
        if ((buttons & (UI_BUTTON_UP | UI_BUTTON_DOWN)) != 0u) {
            state.option = Around(state.option, (buttons & UI_BUTTON_UP) != 0u ? -1 : 1,
                                  UI_OPTIONS);
        } else if ((buttons & (UI_BUTTON_LEFT | UI_BUTTON_RIGHT | UI_BUTTON_OPEN)) != 0u) {
            UiOptions *const o = &state.options;
            switch (state.option) {
            case UI_OPTION_TIMING:
                o->timing = !o->timing;
                break;
            case UI_OPTION_PICTURES:
                o->every_picture = !o->every_picture;
                break;
            case UI_OPTION_RETRY:
                o->retry = !o->retry;
                break;
            case UI_OPTION_FRAMES: {
                uint32_t at = 0;
                while (at + 1u < UI_DEMO_FRAME_CHOICES && UI_DEMO_FRAMES[at] < o->frames) {
                    ++at;
                }
                at = Around(at, (buttons & UI_BUTTON_LEFT) != 0u ? -1 : 1,
                            UI_DEMO_FRAME_CHOICES);
                o->frames = UI_DEMO_FRAMES[at];
                break;
            }
            case UI_OPTION_MUSIC:
                o->music = !o->music;
                break;
            case UI_OPTION_MOTION:
                o->motion = !o->motion;
                break;
            default:
                o->hud = !o->hud;
                break;
            }
        }
    }
    if ((buttons & UI_BUTTON_BACK) != 0u && before.tab == state.tab &&
        state.tab != UI_TAB_RESULTS && state.tab != UI_TAB_START &&
        (state.tab != UI_TAB_SUITES || before.level == 0u)) {
        state.tab = UI_TAB_START;
    }
    state.changed = state.changed || memcmp(&before, &state, sizeof(state)) != 0;
    return state;
}

/// The rows the suites tab lists at its level, and the one the cursor is on.
static uint32_t SuitesRows(const UiBrowseState *state, const UiBrowseInfo *info,
                           uint32_t *cursor) {
    if (state->level == 0u) {
        *cursor = state->family;
        return BROWSE_FAMILIES;
    }
    if (state->level == 1u) {
        *cursor = PlaceInFamily(state->suite);
        return FamilySuites((UiFamily)state->family);
    }
    *cursor = state->item;
    return UiBrowseCases(info, state->suite);
}

/// The list's first row and how many it shows, at the rows' step from @p top to the body's foot.
static uint32_t ListWindow(uint32_t cursor, uint32_t total, uint32_t top, uint32_t height,
                           uint32_t *first) {
    const uint32_t bottom = UI_BODY_BOTTOM(height);
    const uint32_t rows = bottom > top + UI_ROW_PITCH ? (bottom - top) / UI_ROW_PITCH : 1u;
    *first = UiListFirst(cursor, total, rows);
    return total - *first < rows ? total - *first : rows;
}

// The lists' column in the suites, demos and options tabs, and the top of their rows under the
// band that names what they list.
#define BROWSE_LIST_WIDTH 560u
#define BROWSE_CASES_WIDTH 688u
#define BROWSE_ROWS_TOP (UI_BODY_TOP + BH + 12u)

UiBrowseState UiBrowseTap(UiBrowseState state, const UiTap *tap, uint32_t frame_width,
                          uint32_t frame_height, const UiBrowseInfo *info) {
    state.ask = UI_ASK_NONE;
    if (tap == NULL || info == NULL || frame_width < UI_MIN_WIDTH ||
        frame_height < UI_MIN_HEIGHT) {
        return state;
    }
    state.pressed = true;
    const UiTab tab = TabAt(tap);
    if (tab != UI_TABS) {
        state.changed = state.changed || tab != state.tab;
        state.tab = tab;
        return state;
    }
    if (state.tab == UI_TAB_RESULTS) {
        // Its foot's keys are the results screen's, which name its own inputs.
        state.results.browser = true;
        state.results = UiTapStep(state.results, tap, info->results, frame_width, frame_height);
        return ResultsAsked(state);
    }
    UiKey keys[UI_FOOT_KEYS];
    const uint32_t count = UiBrowseKeys(&state, info, keys);
    const uint32_t key = UiFootKeyAt(tap, frame_width, frame_height, keys, count);
    if (key != UINT32_MAX) {
        return keys[key].input != 0
                   ? UiBrowseStep(state, (uint32_t)keys[key].input, UI_SETTLING_FRAMES, info)
                   : state;
    }
    if (state.tab == UI_TAB_START) {
        UiStartState start = UiStartTap(state.start, tap, frame_width, frame_height, info->start);
        if (start.begun) {
            start.begun = false;
            state.ask = UI_ASK_BEGIN;
        }
        state.changed = state.changed || start.changed;
        state.start = start;
        return state;
    }
    // A row of the list: both ends on its box, or in the cursor's margin beside it.
    uint32_t cursor = 0;
    uint32_t total = 0;
    uint32_t width = BROWSE_LIST_WIDTH;
    if (state.tab == UI_TAB_SUITES) {
        total = SuitesRows(&state, info, &cursor);
        width = state.level == 2u ? BROWSE_CASES_WIDTH : BROWSE_LIST_WIDTH;
    } else if (state.tab == UI_TAB_DEMOS) {
        total = info->demo_count;
        cursor = state.demo;
    } else {
        total = UI_OPTIONS;
        cursor = state.option;
    }
    uint32_t first = 0;
    const uint32_t shown = ListWindow(cursor, total, BROWSE_ROWS_TOP, frame_height, &first);
    // The focused row of a suite's cases is as tall as its name, which a tap does not reach
    // past: rows are told apart by their step.
    for (uint32_t r = 0; r < shown; ++r) {
        const UiRect box = { LIST_X - 2u * CW, BROWSE_ROWS_TOP + r * UI_ROW_PITCH,
                             width + 2u * CW, UI_ROW_BOX };
        if (!Inside(box, tap->x0, tap->y0) || !Inside(box, tap->x1, tap->y1)) {
            continue;
        }
        if (first + r == cursor) {
            return UiBrowseStep(state, UI_BUTTON_OPEN, UI_SETTLING_FRAMES, info);
        }
        if (state.tab == UI_TAB_SUITES && state.level == 0u) {
            state.family = first + r;
        } else if (state.tab == UI_TAB_SUITES && state.level == 1u) {
            state.suite = FamilySuiteAt((UiFamily)state.family, first + r);
        } else if (state.tab == UI_TAB_SUITES) {
            state.item = first + r;
        } else if (state.tab == UI_TAB_DEMOS) {
            state.demo = first + r;
        } else {
            state.option = first + r;
        }
        state.changed = true;
        return state;
    }
    return state;
}

uint32_t UiBrowseKeys(const UiBrowseState *state, const UiBrowseInfo *info, UiKey *keys) {
    if (state->tab == UI_TAB_RESULTS) {
        UiState results = state->results;
        results.browser = true;
        return UiResultsKeys(info != NULL ? info->results : NULL, results, keys);
    }
    uint32_t count = 0;
    const UiKey select = { "Up/Down", "select", 0 };
    const UiKey quit = { "Minus", "quit", UI_BUTTON_QUIT };
    const UiKey tabs = { "L/R", "tabs", 0 };
    keys[count++] = select;
    if (state->tab == UI_TAB_START) {
        const UiKey choose = { "A", "choose", UI_BUTTON_OPEN };
        const UiKey begin = { "Plus", "begin", UI_BUTTON_RUN };
        const UiKey retry = { "X", "crash list", UI_BUTTON_CYCLE_VIEW };
        keys[count++] = choose;
        keys[count++] = begin;
        keys[count++] = retry;
        keys[count++] = tabs;
    } else if (state->tab == UI_TAB_OPTIONS) {
        const UiKey change = { "A", "change", UI_BUTTON_OPEN };
        keys[count++] = change;
        keys[count++] = tabs;
        const UiKey back = { "B", "back", UI_BUTTON_BACK };
        keys[count++] = back;
    } else {
        const bool runs_one =
            state->tab == UI_TAB_DEMOS || (state->tab == UI_TAB_SUITES && state->level == 2u);
        const UiKey open = { "A", runs_one ? "run it" : "open", UI_BUTTON_OPEN };
        const UiKey mark = { "X", "mark", UI_BUTTON_CYCLE_VIEW };
        const UiKey run = { "Plus", "run", UI_BUTTON_RUN };
        keys[count++] = open;
        keys[count++] = mark;
        keys[count++] = run;
        if (state->tab == UI_TAB_SUITES && state->level > 0u) {
            const UiKey back = { "B", "back", UI_BUTTON_BACK };
            keys[count++] = back;
        } else {
            keys[count++] = tabs;
        }
    }
    keys[count++] = quit;
    return count;
}

/// A family in a sentence, for the box beside the list.
static const char *FamilyWords(UiFamily family) {
    switch (family) {
    case UI_FAMILY_JUDGE:
        return "The judge: the comparisons every family leans on. It runs before every run, so "
               "it has no mark.";
    case UI_FAMILY_CPU:
        return "The processor's instructions, held to what they must give. No device.";
    case UI_FAMILY_PAD:
        return "The controller, then the touch screen: a person presses what the screen names.";
    case UI_FAMILY_GRAPHICS:
        return "The drawing suites, clear to copy, and tex8k: what the graphics unit draws, "
               "pixel by pixel.";
    case UI_FAMILY_NVN:
        return "The clear suite's question asked through the program's NVN layer.";
    case UI_FAMILY_FS:
        return "Reads from the memory card, timed and checked.";
    case UI_FAMILY_KERNEL:
        return "The scheduler and the kernel's answers to waits, events, handles and memory.";
    case UI_FAMILY_DISPLAY:
        return "The buffer history of a layer of the display service.";
    case UI_FAMILY_VIDEO:
        return "The video image compositor and the VP9 decoder, a job per case.";
    case UI_FAMILY_AUDIO:
        return "The audio renderer: its resampler, its sinks and its limiter, nearly silent.";
    case UI_FAMILY_SERVICE:
        return "Commands a game sends to the system's services, each answer held to reden's.";
    case UI_FAMILY_NET:
        return "The online services, nifm to friend, against what sdmc:/edenprobe/net names.";
    case UI_FAMILY_GAME:
        return "Everything at once, as a game does it: the scene and the Wimsy levels.";
    case UI_FAMILY_LIBC:
        return "The C library and what it asks of the system.";
    case UI_FAMILY_DEMO:
        return "Demos to watch, each frame a graphics workload a game runs, chosen frames judged "
               "and every frame timed.";
    case UI_FAMILY_MUSIC:
        return "The demos' music: each track rendered and held to the host's samples, then "
               "played through the audio renderer.";
    case UI_FAMILY_OTHER:
    case UI_FAMILY_COUNT:
        break;
    }
    return "Suites the screens know no family for.";
}

/// The catalogue's counts for a suite, or for every suite of a family when @p suite is
/// UINT32_MAX.
static void CatalogueCounts(const UiBrowseInfo *info, UiFamily family, uint32_t suite,
                            uint32_t counts[4]) {
    memset(counts, 0, 4u * sizeof(counts[0]));
    const char *const name = suite != UINT32_MAX ? UiBrowseSuite(suite) : NULL;
    for (uint32_t i = 0; i < info->catalogue_count; ++i) {
        const ReportRecord *const record = &info->catalogue[i];
        if (name != NULL ? strcmp(record->suite, name) != 0
                         : UiFamilyOf(record->suite) != family) {
            continue;
        }
        ++counts[record->result == REPORT_PASS     ? 0
                 : record->result == REPORT_FAIL   ? 1
                 : record->result == REPORT_SKIP   ? 2
                                                   : 3];
    }
}

/// At a row's right: how its last run went, "?" when the card has none.
static void RowVerdict(const UiFrame *frame, uint32_t right, uint32_t y, const uint32_t counts[4],
                       bool selected, const uint8_t *ink) {
    char text[32];
    const uint8_t *colour = DIM;
    if (counts[0] + counts[1] + counts[2] + counts[3] == 0u) {
        snprintf(text, sizeof(text), "?");
    } else if (counts[1] > 0u) {
        snprintf(text, sizeof(text), "%u failed", (unsigned)counts[1]);
        colour = FAILED;
    } else if (counts[0] == 0u) {
        snprintf(text, sizeof(text), "skipped");
        colour = SKIPPED;
    } else {
        snprintf(text, sizeof(text), "pass");
        colour = PASSED;
    }
    Big(frame, right - (uint32_t)strlen(text) * CW, y, selected ? ink : colour, text);
}

/// The box that says what the next run holds, under the box beside the list.
static void NextRunBox(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t width,
                       const UiBrowseState *state, const UiBrowseInfo *info) {
    if (y + BH + CH > UI_BODY_BOTTOM(frame->height)) {
        return;
    }
    uint32_t at = Box(frame, x, y, width, UI_BODY_BOTTOM(frame->height) - y, "Next run",
                      "Plus runs it", NULL);
    const uint32_t bottom = UI_BODY_BOTTOM(frame->height);
    uint32_t whole = 0;
    uint32_t cases = 0;
    for (uint32_t i = 0; i < UiBrowseSuiteCount(); ++i) {
        whole += Bit(state->suite_marks, i) ? 1u : 0u;
    }
    for (uint32_t i = 0; i < info->catalogue_count && i < UI_MAX_CASES; ++i) {
        cases += Bit(state->case_marks, i) ? 1u : 0u;
    }
    uint32_t demos = 0;
    for (uint32_t i = 0; i < info->demo_count && i < 32u; ++i) {
        demos += (state->demo_marks >> i & 1u) != 0u ? 1u : 0u;
    }
    char text[96];
    if (whole + cases + demos == 0u) {
        Big(frame, x + CW / 2u, at, DIM, "Nothing marked: Plus runs");
        Big(frame, x + CW / 2u, at + CH, DIM, "the row under the cursor");
        return;
    }
    Big(frame, x + CW / 2u, at, TEXT, "The judge, first");
    at += CH;
    const struct {
        uint32_t count;
        const char *one;
        const char *many;
    } LINES[3] = {
        { whole, "suite whole", "suites whole" },
        { cases, "case alone", "cases alone" },
        { demos, "demo", "demos" },
    };
    for (uint32_t i = 0; i < 3u && at + CH <= bottom; ++i) {
        if (LINES[i].count > 0u) {
            snprintf(text, sizeof(text), "%u %s", (unsigned)LINES[i].count,
                     LINES[i].count == 1u ? LINES[i].one : LINES[i].many);
            Big(frame, x + CW / 2u, at, TEXT, text);
            at += CH;
        }
    }
}

static void ComposeSuites(const UiFrame *frame, const UiBrowseInfo *info,
                          const UiBrowseState *state, char *tip, size_t tip_size) {
    const uint32_t right_edge = frame->width - EDGE;
    uint32_t cursor = 0;
    const uint32_t total = SuitesRows(state, info, &cursor);
    const uint32_t width = state->level == 2u ? BROWSE_CASES_WIDTH : BROWSE_LIST_WIDTH;
    // The band over the list: where it is, and how many there are.
    char band[96];
    char count[32];
    if (state->level == 0u) {
        snprintf(band, sizeof(band), "Families");
        snprintf(count, sizeof(count), "%u", (unsigned)total);
    } else if (state->level == 1u) {
        snprintf(band, sizeof(band), "%s", UiFamilyName((UiFamily)state->family));
        snprintf(count, sizeof(count), "%u suites", (unsigned)total);
    } else {
        snprintf(band, sizeof(band), "%s > %s", UiFamilyName((UiFamily)state->family),
                 UiBrowseSuite(state->suite));
        snprintf(count, sizeof(count), total > 0u ? "%u / %u" : "? cases",
                 (unsigned)(state->item + 1u), (unsigned)total);
    }
    band[0] = (char)(band[0] >= 'a' && band[0] <= 'z' ? band[0] - 'a' + 'A' : band[0]);
    FillRect(frame, LIST_X, UI_BODY_TOP, width, BH, TEXT);
    BigFit(frame, LIST_X + CW / 2u, UI_BODY_TOP + 2u * SC,
           LIST_X + width - CW - (uint32_t)strlen(count) * CW, BACKGROUND, band);
    Big(frame, LIST_X + width - CW / 2u - (uint32_t)strlen(count) * CW, UI_BODY_TOP + 2u * SC,
        BACKGROUND, count);

    uint32_t first = 0;
    const uint32_t shown = ListWindow(cursor, total, BROWSE_ROWS_TOP, frame->height, &first);
    uint32_t y = BROWSE_ROWS_TOP;
    for (uint32_t r = 0; r < shown; ++r) {
        const uint32_t row = first + r;
        const bool selected = row == cursor;
        uint32_t counts[4];
        if (state->level == 0u) {
            const UiFamily family = (UiFamily)row;
            const uint8_t *const ink = ListRow(frame, LIST_X, y, width, UI_ROW_BOX, selected);
            Marker(frame, LIST_X + 8u, y, FamilyMark(state, info, family),
                   family == UI_FAMILY_JUDGE && !selected ? DIM : ink);
            char name[24];
            snprintf(name, sizeof(name), "%s", UiFamilyName(family));
            name[0] = (char)(name[0] - 'a' + 'A');
            Big(frame, LIST_X + 2u * CW, y + 2u, ink, name);
            char suites[16];
            snprintf(suites, sizeof(suites), "%3u", (unsigned)FamilySuites(family));
            Big(frame, LIST_X + 14u * CW, y + 2u, selected ? ink : DIM, suites);
            CatalogueCounts(info, family, UINT32_MAX, counts);
            RowVerdict(frame, LIST_X + width - CW / 2u, y + 2u, counts, selected, ink);
            y += UI_ROW_PITCH;
        } else if (state->level == 1u) {
            const uint32_t suite = FamilySuiteAt((UiFamily)state->family, row);
            CatalogueCounts(info, UI_FAMILY_OTHER, suite, counts);
            const uint8_t *const ink = ListRow(frame, LIST_X, y, width, UI_ROW_BOX, selected);
            Marker(frame, LIST_X + 8u, y, SuiteMark(state, info, suite),
                   Markable(suite) || selected ? ink : DIM);
            BigFit(frame, LIST_X + 2u * CW, y + 2u, LIST_X + 14u * CW,
                   selected || counts[1] == 0u ? ink : FAILED, UiBrowseSuite(suite));
            const uint32_t cases = UiBrowseCases(info, suite);
            char text[16];
            if (cases > 0u) {
                snprintf(text, sizeof(text), "%3u", (unsigned)cases);
            } else {
                snprintf(text, sizeof(text), "  ?");
            }
            Big(frame, LIST_X + 14u * CW, y + 2u, selected ? ink : DIM, text);
            RowVerdict(frame, LIST_X + width - CW / 2u, y + 2u, counts, selected, ink);
            y += UI_ROW_PITCH;
        } else {
            const uint32_t at = UiBrowseCaseAt(info, state->suite, row);
            const ReportRecord *const record = &info->catalogue[at];
            // The focused row as tall as its whole name, three rows at most; the others one,
            // cut.
            const uint32_t name_x = LIST_X + 8u * CW;
            const uint32_t columns = (LIST_X + width - CW / 2u - name_x) / CW;
            const uint32_t wrapped = UiWrappedLines(record->name, columns);
            const uint32_t lines = selected ? (wrapped < 3u ? wrapped : 3u) : 1u;
            const uint32_t height = UI_ROW_BOX + (lines > 0u ? lines - 1u : 0u) * CH;
            if (y + height > UI_BODY_BOTTOM(frame->height)) {
                break;
            }
            const uint8_t *const ink = ListRow(frame, LIST_X, y, width, height, selected);
            const bool bring_up = strcmp(record->tier, "none") == 0;
            const Mark mark = Bit(state->suite_marks, state->suite) ||
                                      (at < UI_MAX_CASES && Bit(state->case_marks, at))
                                  ? MARK_ALL
                              : bring_up ? MARK_SOME
                                         : MARK_NONE;
            Marker(frame, LIST_X + 8u, y, mark, bring_up && !selected ? DIM : ink);
            BigVerdict(frame, LIST_X + 2u * CW + 2u, y + 2u, record->result);
            if (selected) {
                TextWrapped(frame, name_x, y + 2u, SC, CH, ink, record->name, columns, 3u);
            } else {
                BigFit(frame, name_x, y + 2u, LIST_X + width - CW / 2u, ink, record->name);
            }
            y += height + (UI_ROW_PITCH - UI_ROW_BOX);
        }
    }
    if (first > 0u) {
        BigGlyph(frame, LIST_X + width + 4u, BROWSE_ROWS_TOP + 2u, DIM,
                 (unsigned char)FONTVGA_UP);
    }
    if (first + shown < total) {
        BigGlyph(frame, LIST_X + width + 4u, y - UI_ROW_PITCH + 2u, DIM,
                 (unsigned char)FONTVGA_DOWN);
    }

    // Beside the list: what the row under the cursor is, and the next run.
    const uint32_t x = LIST_X + width + 2u * CW;
    const uint32_t box_width = right_edge - x;
    const uint32_t columns = (box_width - CW) / CW;
    uint32_t counts[4];
    char text[200];
    if (state->level == 2u && total > 0u) {
        const ReportRecord *const record =
            &info->catalogue[UiBrowseCaseAt(info, state->suite, state->item)];
        uint32_t at = Box(frame, x, UI_BODY_TOP, box_width,
                          UI_BODY_BOTTOM(frame->height) - UI_BODY_TOP, "Last run", NULL, NULL);
        BigVerdict(frame, x + CW / 2u + 2u, at, record->result);
        snprintf(text, sizeof(text), "tier %s", record->tier);
        Big(frame, x + CW / 2u + 6u * CW, at, DIM, text);
        at += CH + 6u;
        Big(frame, x + CW / 2u, at, DIM,
            Skipped(record->result) ? "Why it was skipped:" : "The judge said:");
        at += CH;
        const char *const detail =
            record->detail[0] != '\0' ? record->detail : NoDetail(record->result);
        const uint32_t room = UI_BODY_BOTTOM(frame->height) > at + 3u * CH
                                  ? (UI_BODY_BOTTOM(frame->height) - at - 2u * CH) / CH
                                  : 1u;
        at += TextWrapped(frame, x + CW / 2u, at, SC, CH, TEXT, detail, columns, room) * CH;
        TextWrapped(frame, x + CW / 2u, at + 6u, SC, CH, DIM,
                    "Runs with it: the suite's bring-up cases", columns, 2u);
        snprintf(tip, tip_size, "A runs this case now, alone; X puts it in the next run");
        return;
    }
    if (state->level == 0u) {
        const UiFamily family = (UiFamily)state->family;
        char name[24];
        snprintf(name, sizeof(name), "%s", UiFamilyName(family));
        name[0] = (char)(name[0] - 'a' + 'A');
        snprintf(text, sizeof(text), "%u suites", (unsigned)FamilySuites(family));
        uint32_t at = Box(frame, x, UI_BODY_TOP, box_width, 268u, name, text, NULL);
        at += TextWrapped(frame, x + CW / 2u, at, SC, CH, TEXT, FamilyWords(family), columns,
                          3u) *
                  CH +
              6u;
        CatalogueCounts(info, family, UINT32_MAX, counts);
        snprintf(text, sizeof(text), "Last run: %u passed, %u failed, %u skipped",
                 (unsigned)counts[0], (unsigned)counts[1], (unsigned)(counts[2] + counts[3]));
        BigFit(frame, x + CW / 2u, at, right_edge - CW / 2u, DIM,
               counts[0] + counts[1] + counts[2] + counts[3] > 0u
                   ? text
                   : "Last run: none on this card");
        snprintf(tip, tip_size, "%s: A opens its suites, X puts the whole family in the next run",
                 name);
    } else {
        const uint32_t suite = state->suite;
        const uint32_t cases = UiBrowseCases(info, suite);
        snprintf(text, sizeof(text), cases > 0u ? "%u cases" : "? cases", (unsigned)cases);
        uint32_t at = Box(frame, x, UI_BODY_TOP, box_width, 268u, UiBrowseSuite(suite), text,
                          NULL);
        CatalogueCounts(info, UI_FAMILY_OTHER, suite, counts);
        if (cases == 0u) {
            at += TextWrapped(frame, x + CW / 2u, at, SC, CH, TEXT,
                              "Not on this card yet: run it once, whole, and its cases are "
                              "listed here",
                              columns, 3u) *
                  CH;
        } else {
            snprintf(text, sizeof(text), "Last run: %u passed, %u failed, %u skipped",
                     (unsigned)counts[0], (unsigned)counts[1],
                     (unsigned)(counts[2] + counts[3]));
            BigFit(frame, x + CW / 2u, at, right_edge - CW / 2u, TEXT, text);
            at += CH;
            // Its first failures, at the font's own size.
            for (uint32_t n = 0, listed = 0; n < cases && listed < 4u; ++n) {
                const ReportRecord *const record =
                    &info->catalogue[UiBrowseCaseAt(info, suite, n)];
                if (record->result == REPORT_FAIL) {
                    DrawCaseLine(frame, x + CW / 2u, at + 4u + listed * LINE, record,
                                 (box_width - CW) / GLYPH - 6u, TEXT);
                    ++listed;
                }
            }
        }
        if (info->catalogue_from != NULL && info->catalogue_from[0] != '\0') {
            SayFitted(frame, x + CW / 2u, UI_BODY_TOP + 268u - LINE - 4u, DIM,
                      info->catalogue_from);
        }
        snprintf(tip, tip_size, "%s: A opens its cases, X puts the whole suite in the next run",
                 UiBrowseSuite(suite));
    }
    NextRunBox(frame, x, UI_BODY_TOP + 278u, box_width, state, info);
}

static void ComposeDemos(const UiFrame *frame, const UiBrowseInfo *info,
                         const UiBrowseState *state, char *tip, size_t tip_size) {
    const uint32_t width = 400u;
    if (info->demo_count == 0u) {
        Big(frame, LIST_X, UI_BODY_TOP + 6u, DIM, "This build has no demo");
        snprintf(tip, tip_size, "No demo to run");
        return;
    }
    uint32_t first = 0;
    const uint32_t shown =
        ListWindow(state->demo, info->demo_count, UI_BODY_TOP + 6u, frame->height, &first);
    for (uint32_t r = 0; r < shown; ++r) {
        const uint32_t row = first + r;
        const uint32_t y = UI_BODY_TOP + 6u + r * UI_ROW_PITCH;
        const bool selected = row == state->demo;
        const uint8_t *const ink = ListRow(frame, LIST_X, y, width, UI_ROW_BOX, selected);
        Marker(frame, LIST_X + 8u, y,
               row < 32u && (state->demo_marks >> row & 1u) != 0u ? MARK_ALL : MARK_NONE, ink);
        BigFit(frame, LIST_X + 2u * CW, y + 2u, LIST_X + width - CW / 2u, ink,
               info->demos[row].name);
    }
    const UiDemoRow *const demo = &info->demos[state->demo];
    const uint32_t x = LIST_X + width + 2u * CW;
    const uint32_t box_width = frame->width - EDGE - x;
    const uint32_t columns = (box_width - CW) / CW;
    char right[48];
    snprintf(right, sizeof(right), "%s", demo->suite);
    uint32_t at = Box(frame, x, UI_BODY_TOP, box_width,
                      UI_BODY_BOTTOM(frame->height) - UI_BODY_TOP, demo->name, right, NULL);
    at += TextWrapped(frame, x + CW / 2u, at, SC, CH, TEXT, demo->description, columns, 5u) * CH +
          10u;
    uint32_t counts[4];
    CatalogueCounts(info, UI_FAMILY_OTHER, UiBrowseSuiteIndex(demo->suite), counts);
    char text[96];
    static const char *const FACTS[3] = { "Last run", "Frames", "Music" };
    char values[3][64];
    if (counts[0] + counts[1] + counts[2] + counts[3] > 0u) {
        snprintf(values[0], sizeof(values[0]), "%u passed, %u failed", (unsigned)counts[0],
                 (unsigned)counts[1]);
    } else {
        snprintf(values[0], sizeof(values[0]), "not on this card yet");
    }
    snprintf(values[1], sizeof(values[1]), "%u, in the options", (unsigned)state->options.frames);
    snprintf(values[2], sizeof(values[2]), "%s",
             !state->options.music ? "off, in the options"
             : demo->track != NULL ? demo->track
                                   : "none of its own");
    for (uint32_t i = 0; i < 3u; ++i) {
        Big(frame, x + CW / 2u, at, DIM, FACTS[i]);
        BigFit(frame, x + CW / 2u + 10u * CW, at, frame->width - EDGE - CW / 2u, TEXT, values[i]);
        at += CH;
    }
    snprintf(text, sizeof(text), "While it runs: Plus pauses, B stops it");
    BigFit(frame, x + CW / 2u, at + 8u, frame->width - EDGE - CW / 2u, DIM, text);
    snprintf(tip, tip_size, "%s: A runs it now, with its HUD over the frames", demo->name);
}

static void ComposeOptions(const UiFrame *frame, const UiBrowseState *state, char *tip,
                           size_t tip_size) {
    static const struct {
        const char *name;
        const char *what;
    } ROWS[UI_OPTIONS] = {
        { "Timing", "On: a drawing suite makes 11 passes and its lines carry times. Off: once, "
                    "as the word notime." },
        { "Pictures", "Failures: a picture for each failing drawing case. Every case: passing "
                      "ones too, as the word images." },
        { "Crash list", "On: the cases that ended a program before run again, as the word retry; "
                        "this one may end too." },
        { "Demo frames", "How long a demo runs: 120, 600, 3600 or 36000 frames, as frames=." },
        { "Music", "The browser's track, and each demo's own while it runs; never in a run the "
                   "mode file begins." },
        { "Motion", "A new screen comes in over a tenth of a second, top first. Off: at once." },
        { "HUD", "Over a demo's frames: its numbers and the frame times. Y hides it there." },
    };
    const UiOptions *const o = &state->options;
    for (uint32_t r = 0; r < UI_OPTIONS; ++r) {
        const uint32_t y = UI_BODY_TOP + 6u + r * UI_ROW_PITCH;
        const bool selected = r == state->option;
        const uint8_t *const ink = ListRow(frame, LIST_X, y, 512u, UI_ROW_BOX, selected);
        BigGlyph(frame, LIST_X + CW / 2u, y + 2u, ink, (unsigned char)FONTVGA_MARKER);
        Big(frame, LIST_X + 2u * CW, y + 2u, ink, ROWS[r].name);
        char value[24];
        switch (r) {
        case UI_OPTION_TIMING:
            snprintf(value, sizeof(value), "%s", o->timing ? "On" : "Off");
            break;
        case UI_OPTION_PICTURES:
            snprintf(value, sizeof(value), "%s", o->every_picture ? "Every case" : "Failures");
            break;
        case UI_OPTION_RETRY:
            snprintf(value, sizeof(value), "%s", o->retry ? "On" : "Off");
            break;
        case UI_OPTION_FRAMES:
            snprintf(value, sizeof(value), "%u", (unsigned)o->frames);
            break;
        case UI_OPTION_MUSIC:
            snprintf(value, sizeof(value), "%s", o->music ? "On" : "Off");
            break;
        case UI_OPTION_MOTION:
            snprintf(value, sizeof(value), "%s", o->motion ? "On" : "Off");
            break;
        default:
            snprintf(value, sizeof(value), "%s", o->hud ? "On" : "Off");
            break;
        }
        char shown[32];
        snprintf(shown, sizeof(shown), selected ? "< %s >" : "%s", value);
        Big(frame, LIST_X + 512u - CW / 2u - (uint32_t)strlen(shown) * CW, y + 2u, ink, shown);
    }
    const uint32_t x = LIST_X + 512u + 2u * CW;
    const uint32_t width = frame->width - EDGE - x;
    const uint32_t columns = (width - CW) / CW;
    uint32_t at = Box(frame, x, UI_BODY_TOP, width, 230u, ROWS[state->option].name, NULL, NULL);
    TextWrapped(frame, x + CW / 2u, at, SC, CH, TEXT, ROWS[state->option].what, columns, 5u);
    at = Box(frame, x, UI_BODY_TOP + 240u, width,
             UI_BODY_BOTTOM(frame->height) - UI_BODY_TOP - 240u, "Always", NULL, NULL);
    TextWrapped(frame, x + CW / 2u, at, SC, CH, DIM,
                "The judge runs before every run begun here. The mode file's other words hold: "
                "hooks, machine, skip, label, unban, hang.",
                columns, 5u);
    snprintf(tip, tip_size, "%s: Left/Right or A changes it, for this session",
             ROWS[state->option].name);
}

void UiComposeBrowse(const UiFrame *frame, const UiBrowseInfo *info, const UiBrowseState *state) {
    if (frame == NULL || frame->pixels == NULL || info == NULL || state == NULL) {
        return;
    }
    g_look = &MENU;
    if (frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        Page(frame, 0, 0, frame->width, frame->height);
        UiText(frame, 0, 0, 1, TEXT, "frame too small");
        return;
    }
    if (state->tab == UI_TAB_RESULTS && info->results != NULL) {
        UiState results = state->results;
        results.browser = true;
        const int32_t picture = UiOpenPicture(info->results, results);
        UiCompose(frame, info->results, results,
                  picture >= 0 && info->picture != NULL ? info->picture(picture) : NULL);
        UiComposeCountdown(frame, info->seconds_left);
        return;
    }
    char title[48];
    snprintf(title, sizeof(title), "EDENPROBE  /  %s", UiTabName(state->tab));
    for (char *c = title; *c != '\0'; ++c) {
        *c = (char)(*c >= 'a' && *c <= 'z' ? *c - 'a' + 'A' : *c);
    }
    Head(frame, state->tab, title, info->machine);
    char tip[200] = "";
    const uint8_t *tip_colour = TEXT;
    UiKey keys[UI_FOOT_KEYS];
    const uint32_t count = UiBrowseKeys(state, info, keys);
    if (state->tab == UI_TAB_START && info->start != NULL) {
        snprintf(tip, sizeof(tip), "Begin the run: the families on, whole, in a run of its own");
        ComposeStartBody(frame, info->start, state->start, tip, tip_colour, keys, count);
        UiComposeCountdown(frame, info->seconds_left);
        return;
    }
    switch (state->tab) {
    case UI_TAB_SUITES:
        ComposeSuites(frame, info, state, tip, sizeof(tip));
        break;
    case UI_TAB_DEMOS:
        ComposeDemos(frame, info, state, tip, sizeof(tip));
        break;
    case UI_TAB_OPTIONS:
        ComposeOptions(frame, state, tip, sizeof(tip));
        break;
    default:
        Big(frame, LIST_X, UI_BODY_TOP + 6u, DIM, "No run yet in this session");
        snprintf(tip, sizeof(tip), "Plus, in the suites or the demos, runs what is marked");
        break;
    }
    Foot(frame, tip, tip_colour, keys, count);
    UiComposeCountdown(frame, info->seconds_left);
}

bool UiBrowsePick(const UiBrowseState *state, const UiBrowseInfo *info, UiPick *pick) {
    memset(pick, 0, sizeof(*pick));
    if (state == NULL || info == NULL) {
        return false;
    }
    bool any = false;
    if (state->ask == UI_ASK_BEGIN && info->start != NULL) {
        // The start tab's families whole, cpu with them, as a run from it has always had.
        static const struct {
            uint32_t mode;
            UiFamily family;
        } ROWS[] = {
            { MODE_FAMILY_GRAPHICS, UI_FAMILY_GRAPHICS }, { MODE_FAMILY_FS, UI_FAMILY_FS },
            { MODE_FAMILY_AUDIO, UI_FAMILY_AUDIO },       { MODE_FAMILY_NET, UI_FAMILY_NET },
            { MODE_FAMILY_PAD, UI_FAMILY_PAD },           { MODE_FAMILY_CPU, UI_FAMILY_CPU },
        };
        const uint32_t families = state->start.families | MODE_FAMILY_CPU;
        for (uint32_t i = 0; i < UiBrowseSuiteCount(); ++i) {
            for (size_t r = 0; r < sizeof(ROWS) / sizeof(ROWS[0]); ++r) {
                if ((families & ROWS[r].mode) != 0u && UiBrowseSuiteFamily(i) == ROWS[r].family) {
                    SetBit(pick->suites, i, true);
                    any = true;
                }
            }
        }
        return any;
    }
    const bool alone = state->ask == UI_ASK_RUN_ONE;
    if (!alone) {
        memcpy(pick->suites, state->suite_marks, sizeof(pick->suites));
        memcpy(pick->cases, state->case_marks, sizeof(pick->cases));
        pick->demos = state->demo_marks;
        for (size_t i = 0; i < sizeof(pick->suites) && !any; ++i) {
            any = pick->suites[i] != 0u;
        }
        for (size_t i = 0; i < sizeof(pick->cases) && !any; ++i) {
            any = pick->cases[i] != 0u;
        }
        any = any || pick->demos != 0u;
        if (any) {
            return true;
        }
    }
    // The row under the cursor.
    if (state->tab == UI_TAB_DEMOS && state->demo < info->demo_count && state->demo < 32u) {
        pick->demos = 1u << state->demo;
        return true;
    }
    if (state->tab != UI_TAB_SUITES) {
        return false;
    }
    if (state->level == 0u) {
        for (uint32_t n = 0; n < FamilySuites((UiFamily)state->family); ++n) {
            const uint32_t suite = FamilySuiteAt((UiFamily)state->family, n);
            if (Markable(suite)) {
                SetBit(pick->suites, suite, true);
                any = true;
            }
        }
        return any;
    }
    if (state->level == 1u && Markable(state->suite)) {
        SetBit(pick->suites, state->suite, true);
        return true;
    }
    const uint32_t at = UiBrowseCaseAt(info, state->suite, state->item);
    if (state->level == 2u && Markable(state->suite) && at < UI_MAX_CASES) {
        SetBit(pick->cases, at, true);
        return true;
    }
    return false;
}

bool UiPickHas(const UiPick *pick, const UiBrowseInfo *info, const char *suite,
               const char *name) {
    if (pick == NULL || suite == NULL || UiFamilyOf(suite) == UI_FAMILY_JUDGE) {
        return true;
    }
    const uint32_t index = UiBrowseSuiteIndex(suite);
    if (index != UINT32_MAX && Bit(pick->suites, index)) {
        return true;
    }
    bool suite_picked = false;
    uint32_t found = UINT32_MAX;
    for (uint32_t i = 0; info != NULL && i < info->catalogue_count && i < UI_MAX_CASES; ++i) {
        const ReportRecord *const record = &info->catalogue[i];
        if (strcmp(record->suite, suite) != 0) {
            continue;
        }
        suite_picked = suite_picked || Bit(pick->cases, i);
        if (found == UINT32_MAX && name != NULL && strcmp(record->name, name) == 0) {
            found = i;
        }
    }
    if (found != UINT32_MAX && Bit(pick->cases, found)) {
        return true;
    }
    // A case of a picked case's suite: its bring-up, which the catalogue holds with no tier, or
    // one the catalogue does not hold at all, which may be its bring-up too.
    return suite_picked &&
           (found == UINT32_MAX || strcmp(info->catalogue[found].tier, "none") == 0);
}

bool UiPickHasFamily(const UiPick *pick, UiFamily family) {
    for (uint32_t i = 0; pick != NULL && i < UiBrowseSuiteCount(); ++i) {
        if (UiBrowseSuiteFamily(i) == family && Bit(pick->suites, i)) {
            return true;
        }
    }
    return false;
}

// ---- The demo's HUD and its pause menu ------------------------------------------------------

/// A box of the HUD: opaque, the menu's page in a frame of the ink, its head a band of the ink
/// with the launcher's bar at its left.
static uint32_t HudBox(const UiFrame *frame, uint32_t x, uint32_t y, uint32_t width,
                       uint32_t height, const char *name, const char *right) {
    FillRect(frame, x, y, width, height, BACKGROUND);
    Outline(frame, x, y, width, height, TEXT);
    FillRect(frame, x, y, width, BH, TEXT);
    FillRect(frame, x + 6u, y + 8u, 4u, BH - 16u, BACKGROUND);
    BigFit(frame, x + 2u * CW, y + 2u * SC, x + width - CW / 2u, BACKGROUND, name);
    if (right != NULL) {
        Big(frame, x + width - CW / 2u - (uint32_t)strlen(right) * CW, y + 2u * SC, BACKGROUND,
            right);
    }
    return y + BH + 4u;
}

/// Keys as the foot draws them, in a box of their own centred at @p y.
static void HudKeys(const UiFrame *frame, uint32_t y, const UiKey *keys, uint32_t count) {
    uint32_t width = 0;
    for (uint32_t i = 0; i < count; ++i) {
        width += ((uint32_t)strlen(keys[i].key) + 1u + (uint32_t)strlen(keys[i].what)) * CW +
                 (i + 1u < count ? 2u * CW : 0u);
    }
    const uint32_t box = width + 4u * CW;
    const uint32_t x0 = frame->width > box ? (frame->width - box) / 2u : 0u;
    FillRect(frame, x0, y, box, CH + 12u, BACKGROUND);
    Outline(frame, x0, y, box, CH + 12u, TEXT);
    uint32_t x = x0 + 2u * CW;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t kw = (uint32_t)strlen(keys[i].key) * CW;
        FillRect(frame, x - SC, y + 6u, kw + 2u * SC, CH, TEXT);
        Big(frame, x, y + 6u, BACKGROUND, keys[i].key);
        x = Big(frame, x + kw + CW, y + 6u, TEXT, keys[i].what) + 2u * CW;
    }
}

void UiComposeHud(const UiFrame *frame, const UiHud *hud) {
    if (frame == NULL || frame->pixels == NULL || hud == NULL ||
        frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        return;
    }
    g_look = &MENU;
    char text[64];
    // Top left: the demo and its numbers.
    const uint32_t width = 368u;
    char name[48];
    snprintf(name, sizeof(name), "%s", hud->name != NULL ? hud->name : "demo");
    for (char *c = name; *c != '\0'; ++c) {
        *c = (char)(*c >= 'a' && *c <= 'z' ? *c - 'a' + 'A' : *c);
    }
    uint32_t y = HudBox(frame, EDGE, EDGE, width, BH + 4u + 3u * CH + 8u, name, NULL);
    snprintf(text, sizeof(text), "%u.%u", (unsigned)(hud->fps_tenths / 10u),
             (unsigned)(hud->fps_tenths % 10u));
    uint32_t x = Big(frame, EDGE + CW, y, TEXT, text);
    x = Big(frame, x, y, DIM, " fps  ");
    snprintf(text, sizeof(text), "%u.%u", (unsigned)(hud->frame_us / 1000u),
             (unsigned)(hud->frame_us % 1000u / 100u));
    x = Big(frame, x, y, TEXT, text);
    Big(frame, x, y, DIM, " ms");
    y += CH;
    x = Big(frame, EDGE + CW, y, DIM, "frame ");
    snprintf(text, sizeof(text), "%u / %u", (unsigned)hud->frame, (unsigned)hud->frames);
    Big(frame, x, y, TEXT, text);
    y += CH;
    x = Big(frame, EDGE + CW, y, DIM, "track ");
    BigFit(frame, x, y, EDGE + width - CW / 2u, TEXT,
           hud->track != NULL && hud->track[0] != '\0' ? hud->track : "none");

    // Top right: the frame times, against the display's two periods at 60 a second.
    const uint32_t graph_width = 352u;
    const uint32_t gx = frame->width - EDGE - graph_width;
    y = HudBox(frame, gx, EDGE, graph_width, BH + 4u + 3u * CH + 8u, "FRAME TIMES", "ms");
    const uint32_t base = y + 3u * CH - 4u;
    // Three pixels a millisecond: 33.3 ms is a hundred.
    const uint32_t count = hud->time_count < UI_HUD_TIMES ? hud->time_count : UI_HUD_TIMES;
    uint32_t previous = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t height = hud->times[i] * 3u / 1000u;
        if (height > 3u * CH - 8u) {
            height = 3u * CH - 8u;
        }
        const uint32_t from = i == 0u ? height : previous;
        const uint32_t low = from < height ? from : height;
        const uint32_t high = from < height ? height : from;
        FillRect(frame, gx + 16u + i * 2u, base - high - 1u, 2u, high - low + 2u,
                 hud->times[i] > 33333u ? FAILED : TEXT);
        previous = height;
    }
    FillRect(frame, gx + 12u, base - 50u, graph_width - 24u, 1u, PASSED);
    FillRect(frame, gx + 12u, base - 100u + 8u, graph_width - 24u, 1u, FAILED);
    Say(frame, gx + graph_width - 6u * GLYPH, base - 50u - 17u, PASSED, "16.7");
    Say(frame, gx + graph_width - 6u * GLYPH, base - 92u - 17u, FAILED, "33.3");

    static const UiKey KEYS[3] = {
        { "Plus", "pause", 0 },
        { "B", "stop", 0 },
        { "Y", "hide this", 0 },
    };
    HudKeys(frame, frame->height - EDGE - CH - 12u, KEYS, 3u);
}

UiPauseState UiPauseOpen(const UiPauseInfo *info, uint32_t frames, bool music, bool hud) {
    UiPauseState state;
    memset(&state, 0, sizeof(state));
    state.cursor = UI_PAUSE_RESUME;
    state.level = info != NULL ? info->running : 0u;
    state.frames = frames;
    state.music = music;
    state.hud = hud;
    state.changed = true;
    return state;
}

UiPauseState UiPauseStep(UiPauseState state, uint32_t buttons, const UiPauseInfo *info) {
    const UiPauseState before = state;
    const bool left = (buttons & UI_BUTTON_LEFT) != 0u;
    const bool change = left || (buttons & UI_BUTTON_RIGHT) != 0u;
    if ((buttons & UI_BUTTON_RUN) != 0u) {
        state.resume = true;
    } else if ((buttons & UI_BUTTON_BACK) != 0u) {
        state.back = true;
    } else if ((buttons & (UI_BUTTON_UP | UI_BUTTON_DOWN)) != 0u) {
        state.cursor =
            Around(state.cursor, (buttons & UI_BUTTON_UP) != 0u ? -1 : 1, UI_PAUSE_ROWS);
    } else if (change && state.cursor == UI_PAUSE_LEVEL && info != NULL &&
               info->level_count > 0u) {
        state.level = Around(state.level, left ? -1 : 1, info->level_count);
    } else if (change && state.cursor == UI_PAUSE_FRAMES) {
        uint32_t at = 0;
        while (at + 1u < UI_DEMO_FRAME_CHOICES && UI_DEMO_FRAMES[at] < state.frames) {
            ++at;
        }
        state.frames = UI_DEMO_FRAMES[Around(at, left ? -1 : 1, UI_DEMO_FRAME_CHOICES)];
    } else if ((change || (buttons & UI_BUTTON_OPEN) != 0u) && state.cursor == UI_PAUSE_MUSIC) {
        state.music = !state.music;
    } else if ((change || (buttons & UI_BUTTON_OPEN) != 0u) && state.cursor == UI_PAUSE_HUD) {
        state.hud = !state.hud;
    } else if ((buttons & UI_BUTTON_OPEN) != 0u) {
        switch (state.cursor) {
        case UI_PAUSE_RESUME:
            state.resume = true;
            break;
        case UI_PAUSE_BACK:
            state.back = true;
            break;
        case UI_PAUSE_RESTART:
            state.level = info != NULL ? info->running : state.level;
            state.start_level = true;
            break;
        case UI_PAUSE_LEVEL:
        case UI_PAUSE_FRAMES:
            state.start_level = true;
            break;
        default:
            break;
        }
    }
    state.changed = state.changed || memcmp(&before, &state, sizeof(state)) != 0;
    return state;
}

void UiComposePause(const UiFrame *frame, const UiPauseInfo *info, const UiPauseState *state) {
    if (frame == NULL || frame->pixels == NULL || info == NULL || state == NULL ||
        frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        return;
    }
    // The frozen frame dimmed with nothing blended: every other row of it the run's page.
    for (uint32_t y = 1; y < frame->height; y += 2u) {
        FillRect(frame, 0, y, frame->width, 1u, RUN.page);
    }
    g_look = &MENU;
    const uint32_t width = 1056u;
    const uint32_t height = 540u;
    const uint32_t bx = (frame->width - width) / 2u;
    const uint32_t by = (frame->height - height) / 2u;
    Backdrop(frame, bx, by, width, height);
    Outline(frame, bx, by, width, height, TEXT);
    Outline(frame, bx + 1u, by + 1u, width - 2u, height - 2u, TEXT);
    Frieze(frame, bx, by, width, height);
    UiText(frame, bx + 3u * CW + 4u, by + 38u, 2u * SC, BORDER, "PAUSED");
    UiText(frame, bx + 3u * CW, by + 34u, 2u * SC, TEXT, "PAUSED");
    if (info->name != NULL) {
        Big(frame, bx + width - 3u * CW - (uint32_t)strlen(info->name) * CW, by + 50u, DIM,
            info->name);
    }
    FillRect(frame, bx + 3u * CW, by + 108u, width - 6u * CW, SC, BORDER);

    const UiDemoRow *const level =
        info->levels != NULL && state->level < info->level_count ? &info->levels[state->level]
                                                                 : NULL;
    const uint32_t x = bx + 5u * CW;
    const uint32_t list_width = 432u;
    const uint32_t top = by + 124u;
    static const char *const NAMES[UI_PAUSE_ROWS] = {
        "Resume", "Level", "Frames", "Music", "HUD", "Restart", "Back to the browser",
    };
    for (uint32_t r = 0; r < UI_PAUSE_ROWS; ++r) {
        const uint32_t y = top + r * (UI_ROW_BOX + 4u);
        const bool selected = r == state->cursor;
        const uint8_t *const ink = ListRow(frame, x, y, list_width, UI_ROW_BOX, selected);
        BigGlyph(frame, x + CW / 2u, y + 2u, ink, (unsigned char)FONTVGA_MARKER);
        Big(frame, x + 2u * CW, y + 2u, ink, NAMES[r]);
        char value[48] = "";
        if (r == UI_PAUSE_LEVEL && level != NULL) {
            snprintf(value, sizeof(value), "%s", level->name);
        } else if (r == UI_PAUSE_FRAMES) {
            snprintf(value, sizeof(value), "%u", (unsigned)state->frames);
        } else if (r == UI_PAUSE_MUSIC) {
            snprintf(value, sizeof(value), "%s", state->music ? "On" : "Off");
        } else if (r == UI_PAUSE_HUD) {
            snprintf(value, sizeof(value), "%s", state->hud ? "On" : "Off");
        }
        if (value[0] != '\0') {
            char shown[64];
            snprintf(shown, sizeof(shown), selected && r <= UI_PAUSE_FRAMES ? "< %s >" : "%s",
                     value);
            char fitted[64];
            Fit(fitted, sizeof(fitted), shown, (list_width - 12u * CW) / CW);
            Big(frame, x + list_width - CW / 2u - (uint32_t)strlen(fitted) * CW, y + 2u, ink,
                fitted);
        }
    }
    const uint32_t sx = x + list_width + 2u * CW;
    const uint32_t side = bx + width - 3u * CW - sx;
    const uint32_t columns = (side - CW) / CW;
    uint32_t y = Box(frame, sx, top, side, UI_PAUSE_ROWS * (UI_ROW_BOX + 4u) - 4u,
                     level != NULL ? level->name : "Level", NULL, NULL);
    y += TextWrapped(frame, sx + CW / 2u, y, SC, CH, TEXT,
                     "Left/Right picks another; A starts it, and this one ends where it is.",
                     columns, 3u) *
             CH +
         6u;
    char text[64];
    snprintf(text, sizeof(text), "Frame %u of %u", (unsigned)info->frame, (unsigned)info->frames);
    Big(frame, sx + CW / 2u, y, DIM, text);
    TextWrapped(frame, sx + CW / 2u, y + CH, SC, CH, DIM,
                "The time it is paused is left out of the frame times", columns, 2u);
    static const UiKey KEYS[4] = {
        { "Up/Down", "select", 0 },
        { "Left/Right", "change", 0 },
        { "A", "choose", 0 },
        { "Plus", "resume", 0 },
    };
    uint32_t kx = bx + 5u * CW;
    const uint32_t ky = by + height - 30u - CH;
    for (uint32_t i = 0; i < 4u; ++i) {
        const uint32_t kw = (uint32_t)strlen(KEYS[i].key) * CW;
        FillRect(frame, kx - SC, ky, kw + 2u * SC, CH, TEXT);
        Big(frame, kx, ky, BACKGROUND, KEYS[i].key);
        kx = Big(frame, kx + kw + CW, ky, TEXT, KEYS[i].what) + 2u * CW;
    }
}

uint32_t UiMotionRows(uint32_t step, uint32_t frame_height) {
    if (step >= UI_MOTION_STEPS) {
        return frame_height;
    }
    return (uint32_t)((uint64_t)frame_height * (step + 1u) / UI_MOTION_STEPS);
}
