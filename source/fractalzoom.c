// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#include "check.h"
#include "fractalzoom.h"
#include "mode.h"
#include "picture.h"
#include "report.h"

// The fractal zooms' cases on any way to the graphics unit (fractalzoom.h): each frame a dispatch
// that counts every pixel's iterations and a draw that colours the target from them
// (fractalref.h says the two passes), every frame presented and timed, and the judged frames'
// counts and colours, which the graphics unit copied aside as the frames ran, judged once they
// are over, so judging takes no frame's time.

#define COUNTS_BYTES (FRACTAL_PIXELS * 4u)

static uint32_t present_us[MODE_FRAMES_MAX];
static uint32_t work_us[MODE_FRAMES_MAX];
static uint32_t sorted_us[MODE_FRAMES_MAX];

/// What a run of the frames came to.
static struct {
    uint32_t ran;
    uint32_t presented;
    bool stopped;
    /// How often the person paused it, and for how long in all.
    uint32_t pauses;
    uint64_t paused_us;
} frames_seen;

static uint32_t Microseconds(uint64_t ticks) {
    const uint64_t us = armTicksToNs(ticks) / 1000u;
    return us > UINT32_MAX ? UINT32_MAX : (uint32_t)us;
}

/// Which judged frame @p k is, or FRACTAL_JUDGED when it is none.
static uint32_t JudgedIndex(uint32_t k) {
    for (uint32_t j = 0; j < FRACTAL_JUDGED; ++j) {
        if (FractalJudgedFrame(j) == k) {
            return j;
        }
    }
    return FRACTAL_JUDGED;
}

/// Runs @p run's frames of zoom @p kind, until they are over, the person stops them or the
/// queue faults.
static void RunFrames(const FractalWay *way, FractalKind kind, const DemoRun *run) {
    memset(&frames_seen, 0, sizeof(frames_seen));
    uint64_t previous = armGetSystemTick();
    for (uint32_t k = 0; k < run->frames && way->ready(); ++k) {
        if (run->stopped != NULL && run->stopped()) {
            frames_seen.stopped = true;
            break;
        }
        // Paused after a frame was shown: the last one shown again until the person goes on, and
        // the next frame's time counted from then.
        if (k > 0u && run->paused != NULL && run->paused()) {
            const uint64_t from = armGetSystemTick();
            ++frames_seen.pauses;
            while (way->ready() && run->paused() && (run->stopped == NULL || !run->stopped())) {
                way->show_again();
            }
            frames_seen.paused_us += Microseconds(armGetSystemTick() - from);
            previous += armGetSystemTick() - from;
            if (run->stopped != NULL && run->stopped()) {
                frames_seen.stopped = true;
                break;
            }
        }
        const int image = way->acquire();
        const uint64_t began = armGetSystemTick();
        const uint32_t slot = k % FRACTAL_SLOTS;
        FractalFrameOf(kind, k, way->block(slot));
        way->draw(slot, JudgedIndex(k), image);
        const uint64_t worked = armGetSystemTick() - began;
        // Presented whatever the submit came to: an image taken and never queued back would
        // leave the display's ring one short for the destroy that follows a fault.
        const bool shown = way->present(image) && way->ready();
        frames_seen.presented += shown ? 1u : 0u;
        // After the present was queued: the wait for the display is in the next frame's acquire.
        const uint64_t now = armGetSystemTick();
        present_us[k] = Microseconds(now - previous);
        work_us[k] = Microseconds(worked);
        previous = now;
        ++frames_seen.ran;
    }
}

/// The verdict on judged frame @p j, frame @p frame of zoom @p kind: its counts and its image
/// against the pinned hashes, and, only when one differs, against the frame worked out here.
static void JudgeFrame(const FractalWay *way, FractalKind kind, const char *suite,
                       const char *name, uint32_t j, uint32_t frame) {
    const uint32_t *const got_counts = (const uint32_t *)(const void *)way->kept_counts(j);
    const uint8_t *const got_image = way->kept_image(j);
    const uint32_t counts_hash = FractalHash(got_counts, COUNTS_BYTES);
    const uint32_t image_hash = FractalHash(got_image, FRACTAL_IMAGE_BYTES);
    const uint32_t pinned_counts = FractalPinnedCounts(kind, j);
    const uint32_t pinned_image = FractalPinnedImage(kind, j);
    if (counts_hash == pinned_counts && image_hash == pinned_image) {
        // A run that asks for every case's picture gets this frame's too, every pixel matched.
        uint8_t *const same = PictureWritesEveryCase() ? (uint8_t *)malloc(FRACTAL_IMAGE_BYTES)
                                                       : NULL;
        uint8_t *const matched = same != NULL ? (uint8_t *)calloc(FRACTAL_PIXELS, 1u) : NULL;
        if (same != NULL && matched != NULL) {
            memcpy(same, got_image, FRACTAL_IMAGE_BYTES);
            (void)PictureBeginOwn(FRACTAL_WIDTH, FRACTAL_HEIGHT, PICTURE_COLOUR, got_image, same,
                                  matched);
        }
        ReportCase(suite, name, CHECK_EXACT, true, "");
        free(same);
        free(matched);
        return;
    }

    // Worked out on this processor, for what differs and the picture; and whether it gives the
    // pinned answer here, since a judge that does not would be blamed on the graphics unit.
    static FractalFrame block;
    FractalFrameOf(kind, frame, &block);
    uint32_t *const want = (uint32_t *)malloc(COUNTS_BYTES);
    uint8_t *const expected = (uint8_t *)malloc(FRACTAL_IMAGE_BYTES);
    uint8_t *const verdicts = (uint8_t *)malloc(FRACTAL_PIXELS);
    if (want == NULL || expected == NULL || verdicts == NULL) {
        char detail[240];
        snprintf(detail, sizeof(detail),
                 "counts hash 0x%08x, pinned 0x%08x; image hash 0x%08x, pinned 0x%08x; no memory "
                 "to work the frame out",
                 (unsigned)counts_hash, (unsigned)pinned_counts, (unsigned)image_hash,
                 (unsigned)pinned_image);
        ReportCase(suite, name, CHECK_EXACT, false, detail);
        free(want);
        free(expected);
        free(verdicts);
        return;
    }
    FractalCounts(&block, want);
    FractalImage(&block, want, expected);
    const uint32_t want_counts_hash = FractalHash(want, COUNTS_BYTES);
    const uint32_t want_image_hash = FractalHash(expected, FRACTAL_IMAGE_BYTES);
    FractalCountsVerdict verdict;
    FractalJudgeCounts(&block, got_counts, want, &verdict);
    uint32_t wrong_pixels = 0;
    for (uint32_t i = 0; i < FRACTAL_PIXELS; ++i) {
        const bool same = memcmp(got_image + (size_t)i * 4u, expected + (size_t)i * 4u, 4u) == 0;
        verdicts[i] = (uint8_t)(same ? PICTURE_MATCHED : PICTURE_WRONG);
        wrong_pixels += same ? 0u : 1u;
    }
    (void)PictureBeginOwn(FRACTAL_WIDTH, FRACTAL_HEIGHT, PICTURE_COLOUR, got_image, expected,
                          verdicts);

    char counted[200] = "every count is the processor's";
    if (verdict.wrong > 0u) {
        snprintf(counted, sizeof(counted),
                 "%u of %u counts differ, the first at %u,%u: %u for %u; %u of them what a "
                 "machine that multiplies and adds apart counts, %u one iteration off",
                 (unsigned)verdict.wrong, (unsigned)FRACTAL_PIXELS, (unsigned)verdict.first_x,
                 (unsigned)verdict.first_y, (unsigned)verdict.first_got,
                 (unsigned)verdict.first_want, (unsigned)verdict.apart,
                 (unsigned)verdict.off_by_one);
    }
    char judge[160] = "";
    if (want_counts_hash != pinned_counts || want_image_hash != pinned_image) {
        snprintf(judge, sizeof(judge),
                 "; this processor works the frame out to 0x%08x and 0x%08x, not the pinned "
                 "0x%08x and 0x%08x: the judge differs here, not only the graphics unit",
                 (unsigned)want_counts_hash, (unsigned)want_image_hash, (unsigned)pinned_counts,
                 (unsigned)pinned_image);
    }
    char detail[REPORT_NOTE_MAX + 1];
    snprintf(detail, sizeof(detail), "%s; %u of %u pixels another colour (its picture)%s", counted,
             (unsigned)wrong_pixels, (unsigned)FRACTAL_PIXELS, judge);
    ReportCase(suite, name, CHECK_EXACT, false, detail);
    free(want);
    free(expected);
    free(verdicts);
}

/// One zoom from the bring-up to its times; every case after one that did not happen skipped as
/// depending on it, saying why.
void FractalZoom(const FractalWay *way, FractalKind kind, const DemoRun *run) {
    const char *const suite = FractalSuite(kind);
    static const char BRING_UP[] = "a graphics device with compute, the programs and a way to "
                                   "the display come up";
    char frames_name[96];
    snprintf(frames_name, sizeof(frames_name), "%u frames of the zoom run, every one presented",
             (unsigned)run->frames);
    char judged_names[FRACTAL_JUDGED][96];
    for (uint32_t j = 0; j < FRACTAL_JUDGED; ++j) {
        snprintf(judged_names[j], sizeof(judged_names[j]),
                 "frame %u draws the counts and colours the processor works out",
                 (unsigned)FractalJudgedFrame(j));
    }
    static DemoTimeCase time_cases[DEMO_TIME_CASES];
    (void)DemoTimeCases(NULL, run->frames, time_cases);

    char why[400] = "";
    bool up = false;
    if (ReportStart(suite, BRING_UP)) {
        up = way->up(why, sizeof(why));
        CHECK_TRUE(suite, BRING_UP, CHECK_NONE, up, "%s", why);
    } else {
        snprintf(why, sizeof(why), "the bring-up did not run");
    }
    bool ran = false;
    if (up && ReportStart(suite, frames_name)) {
        RunFrames(way, kind, run);
        ran = frames_seen.ran > 0u;
        char detail[400];
        char paused[96] = "";
        if (frames_seen.pauses > 0u) {
            snprintf(paused, sizeof(paused), ", paused %u times for %llu ms out of the times",
                     (unsigned)frames_seen.pauses,
                     (unsigned long long)(frames_seen.paused_us / 1000u));
        }
        snprintf(detail, sizeof(detail), "%u of %u frames ran, %u presented%s%s; %s",
                 (unsigned)frames_seen.ran, (unsigned)run->frames,
                 (unsigned)frames_seen.presented, paused,
                 frames_seen.stopped ? ", then stopped" : "",
                 way->failure()[0] != '\0' ? way->failure()
                                            : "the graphics queue reported no fault");
        CHECK_TRUE(suite, frames_name, CHECK_NONE,
                   frames_seen.presented == frames_seen.ran &&
                       (frames_seen.ran == run->frames || frames_seen.stopped),
                   "%s", detail);
        static FractalFrame deepest;
        FractalFrameOf(kind, FRACTAL_ZOOM_FRAMES - 1u, &deepest);
        ReportNote("%s: the deepest frame's step is %.1f units in the last place of the farthest "
                   "coordinate a pixel reaches; single precision holds the zoom to there, and a "
                   "deeper one wants perturbation or double precision",
                   suite, FractalStepUlps(&deepest));
    } else if (up) {
        snprintf(why, sizeof(why), "the frames' case did not run, so there are no frames");
    } else {
        ReportSkip(suite, frames_name, REPORT_REASON_DEPENDS,
                   "the zoom did not come up, so its frames did not run");
    }

    for (uint32_t j = 0; j < FRACTAL_JUDGED; ++j) {
        const uint32_t frame = FractalJudgedFrame(j);
        if (!ran) {
            ReportSkip(suite, judged_names[j], REPORT_REASON_DEPENDS,
                       up ? why : "the zoom did not come up, so its frames did not run");
        } else if (frame >= frames_seen.ran) {
            char detail[160];
            if (frames_seen.stopped) {
                snprintf(detail, sizeof(detail), "stopped at frame %u by the person watching",
                         (unsigned)frames_seen.ran);
            } else {
                snprintf(detail, sizeof(detail), "the run's %u frames ended before frame %u%s",
                         (unsigned)frames_seen.ran, (unsigned)frame,
                         way->ready() ? "" : ", at a fault of the graphics queue");
            }
            ReportSkip(suite, judged_names[j],
                       frames_seen.stopped ? REPORT_REASON_ASKED : REPORT_REASON_DEPENDS, detail);
        } else if (ReportStart(suite, judged_names[j])) {
            JudgeFrame(way, kind, suite, judged_names[j], j, frame);
        }
    }

    if (run->timing) {
        static DemoTimes times;
        if (ran) {
            DemoTimesOf(&times, frames_seen.ran, present_us, work_us, sorted_us);
        }
        const uint32_t timed = ran ? DemoTimeCases(&times, run->frames, time_cases) : 0u;
        char detail[160];
        snprintf(detail, sizeof(detail), "%u frames past the warm-up; %u of %u presented",
                 (unsigned)timed, (unsigned)frames_seen.presented, (unsigned)frames_seen.ran);
        for (uint32_t i = 0; i < DEMO_TIME_CASES; ++i) {
            if (!ran) {
                ReportSkip(suite, time_cases[i].name, REPORT_REASON_DEPENDS,
                           up ? why : "the zoom did not come up, so its frames did not run");
            } else if (timed == 0u) {
                ReportSkip(suite, time_cases[i].name, REPORT_REASON_DEPENDS,
                           "the run's one frame is the warm-up, which the times leave out");
            } else if (ReportStart(suite, time_cases[i].name)) {
                ReportCaseTime(suite, time_cases[i].name, CHECK_NONE,
                               frames_seen.presented == frames_seen.ran, detail,
                               time_cases[i].us);
            }
        }
        if (ran) {
            char note[REPORT_NOTE_MAX + 1];
            DemoDescribeTimes(note, sizeof(note), suite, &times);
            ReportNote("%s", note);
        }
    }
    way->close();
}

