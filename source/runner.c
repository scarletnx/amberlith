// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "bans.h"
#include "baseline.h"
#include "journal.h"
#include "perf.h"
#include "picture.h"
#include "platform.h"
#include "report.h"
#include "runner.h"
#include "settle.h"

// Each path under the root, longest name first in the sizes runner.h gives them.
static bool Join(char *out, size_t out_size, const char *root, const char *name) {
    const int written = snprintf(out, out_size, "%s/%s", root, name);
    return written > 0 && (size_t)written < out_size;
}

RunnerRootChoice RunnerOldRoot(bool root_mode, bool root_runs, bool old_mode, bool old_runs) {
    const bool old_used = old_mode || old_runs;
    if (!old_used) {
        return RUNNER_ROOT_NEW;
    }
    return root_mode || root_runs ? RUNNER_ROOT_NEW_BESIDE_OLD : RUNNER_ROOT_OLD;
}

static const char *root_in_use;

const char *RunnerRootInUse(void) {
    return root_in_use;
}

/// Whether @p name is there under @p root, as a file or a folder.
static bool Holds(const char *root, const char *name) {
    char path[RUNNER_ROOT_MAX + 16u];
    struct stat about;
    return Join(path, sizeof(path), root, name) && stat(path, &about) == 0;
}

bool RunnerInit(Runner *runner, const RunnerFrontend *frontend) {
    memset(runner, 0, sizeof(*runner));
    runner->frontend = frontend;
    const char *root = frontend->root;
    if (root == NULL || strlen(root) > RUNNER_ROOT_MAX) {
        return false;
    }
    const char *const old = frontend->old_root;
    if (old != NULL && strlen(old) <= RUNNER_ROOT_MAX) {
        runner->root_choice = RunnerOldRoot(Holds(root, "mode"), Holds(root, "runs"),
                                            Holds(old, "mode"), Holds(old, "runs"));
        if (runner->root_choice == RUNNER_ROOT_OLD) {
            root = old;
        }
    }
    runner->root = root;
    root_in_use = root;
    if (!Join(runner->mode_path, sizeof(runner->mode_path), root, "mode") ||
        !Join(runner->crash_list_path, sizeof(runner->crash_list_path), root, "crashes") ||
        !Join(runner->last_run_path, sizeof(runner->last_run_path), root, "last-run") ||
        !Join(runner->baseline_directory, sizeof(runner->baseline_directory), root, "baseline") ||
        !Join(runner->runs_directory, sizeof(runner->runs_directory), root, "runs")) {
        return false;
    }
    // No console. The report channels stay the interface a verdict is read from, so a bring-up
    // problem on a new machine is never "did the screen work".
    mkdir(root, 0777);
    return true;
}

RunMode RunnerModeFromFile(const char *path) {
    // One byte past the limit, which is how a longer file is told apart from one exactly as
    // long, and which says whether the last word read runs on. Static, because it is four
    // kilobytes and read once.
    static char text[MODE_FILE_MAX + 2u];
    ModeSource source = MODE_SOURCE_NONE;
    size_t read = 0;
    struct stat about;
    if (stat(path, &about) == 0 && S_ISDIR(about.st_mode)) {
        source = MODE_SOURCE_DIRECTORY;
    } else {
        FILE *file = fopen(path, "r");
        if (file != NULL) {
            source = MODE_SOURCE_FILE;
            read = fread(text, 1, MODE_FILE_MAX + 1u, file);
            fclose(file);
        }
    }
    text[read] = '\0';
    const bool longer = read > MODE_FILE_MAX;
    return ModeFromFile(source, text, longer ? MODE_FILE_MAX : read, longer);
}

// Where the run leaves its report on the machine it ran on. Each build, and each label the mode
// file gives, has a folder of its own under runs/, named after its hash: the report, the pictures
// of the failing cases and the journal go there, so one folder is the whole of what a run leaves
// behind and runs of two builds never mix. The mode file, the net file and the crash list stay
// at the root, since they belong to the person and the machine, not a build.
static void PlaceRun(Runner *runner, const RunMode *mode) {
    char folder[MODE_FOLDER_MAX + 1u];
    if (ModeRunFolder(folder, sizeof(folder), runner->frontend->hash, mode) == 0u) {
        snprintf(folder, sizeof(folder), "unknown");
    }
    snprintf(runner->run_directory, sizeof(runner->run_directory), "%s/%s",
             runner->runs_directory, folder);
    snprintf(runner->report_path, sizeof(runner->report_path), "%s/report.jsonl",
             runner->run_directory);
    snprintf(runner->journal_path, sizeof(runner->journal_path), "%s/%s", runner->run_directory,
             JOURNAL_FILE_NAME);
    mkdir(runner->runs_directory, 0777);
    mkdir(runner->run_directory, 0777);
    // The folder the latest launch went to, relative to the root: how a runner, or a person with
    // a file manager, finds the report without knowing the build's hash.
    char last[MODE_FOLDER_MAX + 8u];
    snprintf(last, sizeof(last), "runs/%s\n", folder);
    (void)JournalWriteFile(runner->last_run_path, last);
}

/// What last-run names, when it is a browser's run: read before this launch's names its own.
static void ReadBrowserLast(Runner *runner) {
    runner->browser_last[0] = '\0';
    char text[RUNNER_PATH_MAX];
    const int64_t length = JournalReadFile(runner->last_run_path, text, sizeof(text) - 1u, NULL);
    if (length <= 0) {
        return;
    }
    text[length] = '\0';
    text[strcspn(text, "\r\n")] = '\0';
    if (strstr(text, "/browse/") != NULL) {
        snprintf(runner->browser_last, sizeof(runner->browser_last), "%s", text);
    }
}

void RunnerPlan(Runner *runner, const RunMode *from_file) {
    const RunnerFrontend *const frontend = runner->frontend;
    ReadBrowserLast(runner);
    PlaceRun(runner, from_file);
    PictureSetRoot(runner->run_directory);
    char key[JOURNAL_MODE_MAX];
    ModeKey(key, sizeof(key), from_file);
    JournalStateInit(&runner->journal);
    (void)JournalReadPath(&runner->journal, runner->journal_path);
    CrashListInit(&runner->crashes);
    {
        static char text[CRASH_LIST_MAX * (JOURNAL_SUITE_MAX + JOURNAL_NAME_MAX + 2u) + 1024u];
        const int64_t length = JournalReadFile(runner->crash_list_path, text, sizeof(text), NULL);
        if (length > 0) {
            CrashListParse(&runner->crashes, text, (size_t)length);
        }
    }
    runner->plan = JournalPlanStart(&runner->journal, frontend->version, frontend->build, key,
                                    JournalFileSize(runner->report_path));
    if (from_file->no_journal && runner->plan.start != JOURNAL_FRESH) {
        // Asked for no journal: nothing goes on, and the run starts over unjournaled.
        runner->plan.start = JOURNAL_FRESH;
        runner->plan.launch = 1u;
    }
}

RunMode RunnerModeAtStart(const Runner *runner, const RunMode *from_file) {
    // A launch that goes on with a run keeps what its start screen chose; the label is the
    // file's, since it named the folder the journal is in.
    return runner->plan.start != JOURNAL_FRESH ? ModeWithChosen(*from_file, runner->journal.chosen)
                                               : *from_file;
}

uint32_t RunnerFamilies(const Runner *runner, const RunMode *mode) {
    return ModeFamiliesToRunOf(mode, runner->frontend->present, runner->frontend->asked);
}

static void JournalSimple(JournalKind kind, const char *step, uint32_t number) {
    JournalRecord record;
    memset(&record, 0, sizeof(record));
    record.kind = kind;
    snprintf(record.suite, sizeof(record.suite), "%s", step != NULL ? step : "");
    record.number = number;
    ReportJournalWrite(&record);
}

/// Says which folder the launch runs from when the old one is on the card, naming both.
static void NoteOldRoot(const Runner *runner) {
    const char *const root = runner->frontend->root;
    const char *const old = runner->frontend->old_root;
    if (runner->root_choice == RUNNER_ROOT_OLD) {
        ReportNote("card: %s, edenprobe's, holds a mode file or runs and %s neither, so this "
                   "launch runs from %s whole; renamed %s, it is read from there",
                   old, root, old, root);
    } else if (runner->root_choice == RUNNER_ROOT_NEW_BESIDE_OLD) {
        ReportNote("card: %s, edenprobe's, is on the card beside %s: this launch runs from %s, "
                   "and reads nothing of %s",
                   old, root, root, old);
    }
}

/// Starts the report the way the plan says, and the journal of a run that goes on. A new run's
/// journal is begun by RunnerBeginJournal, once a start screen has chosen what runs; the run
/// before's goes now, so a launch that ends before then does not go on with that run in this
/// report.
static void OpenReport(Runner *runner) {
    const JournalPlan *const plan = &runner->plan;
    if (plan->start == JOURNAL_FRESH) {
        ReportOpen(runner->report_path);
        ReportNote("Amberlith %s (%s) starting", runner->frontend->version, runner->frontend->build);
        ReportNote("folder: %s", runner->run_directory);
        NoteOldRoot(runner);
        (void)remove(runner->journal_path);
        return;
    }
    ReportResume(runner->report_path, plan->offset, plan->start == JOURNAL_RESUME_CASE,
                 &plan->counts);
    if (!ReportJournalOpen(runner->journal_path, false)) {
        ReportNote("journal: %s cannot be written to go on with, so a crash from here costs "
                   "the rest of the run",
                   runner->journal_path);
    }
    JournalSimple(JOURNAL_LAUNCH, NULL, plan->launch);
    if (plan->start == JOURNAL_RESUME_CASE) {
        // The plan's record of the case, kind and all: crashed, or timedout with its seconds.
        ReportJournalWrite(&plan->crash);
        // What the step drew before the program ended: it runs again from its start, and the
        // pictures it leaves are the ones its new lines name.
        (void)PictureRemoveSuite(runner->journal.step.suite);
    } else if (plan->start == JOURNAL_RESUME_STEP) {
        JournalSimple(JOURNAL_DONE, runner->journal.step.suite, 0);
    }
}

void RunnerOpen(Runner *runner, const RunMode *mode) {
    JournalPlan *const plan = &runner->plan;
    OpenReport(runner);
    ReportSetCrashList(&runner->crashes, runner->crash_list_path, mode->retry);
    // A case the program ended inside goes on the crash list whether this run goes on or not:
    // the machine did end inside it. Not one a runner stopped the program inside at its limit:
    // the limit is a launch's, so the case may only have been the one running when it came.
    const bool ended_itself = plan->crashed && !plan->timed_out;
    const bool listed = ended_itself && ReportCrashListAdd(plan->crash.suite, plan->crash.name);
    const bool usual = plan->start == JOURNAL_FRESH && !plan->crashed &&
                       (plan->why_new == JOURNAL_NEW_NO_JOURNAL ||
                        plan->why_new == JOURNAL_NEW_FINISHED);
    if (!usual) {
        char said[REPORT_NOTE_MAX + 1];
        JournalDescribePlan(said, sizeof(said), plan, &runner->journal);
        ReportNote("%s%s", said,
                   ended_itself && !listed ? " (it was on the crash list already)" : "");
    }
    if (plan->start != JOURNAL_FRESH) {
        JournalState *const journal = &runner->journal;
        for (uint32_t i = 0; i < journal->crash_count; ++i) {
            ReportAddCrashed(&journal->crashes[i]);
        }
        if (plan->start == JOURNAL_RESUME_CASE) {
            ReportAddCrashed(&plan->crash);
            if (journal->crash_count < JOURNAL_CRASHES_MAX) {
                journal->crashes[journal->crash_count++] = plan->crash;
            }
        }
    }
}

void RunnerBeginJournal(Runner *runner, const RunMode *from_file, const RunMode *mode) {
    if (mode->no_journal) {
        return;
    }
    if (!ReportJournalOpen(runner->journal_path, true)) {
        ReportNote("journal: %s cannot be written, so a crash costs the rest of the run",
                   runner->journal_path);
        return;
    }
    char run_key[JOURNAL_MODE_MAX];
    char chosen[JOURNAL_MODE_MAX];
    ModeJournalWords(run_key, sizeof(run_key), chosen, sizeof(chosen), from_file, mode);
    JournalRecord record;
    memset(&record, 0, sizeof(record));
    record.kind = JOURNAL_RUN;
    snprintf(record.suite, sizeof(record.suite), "%s", runner->frontend->version);
    snprintf(record.name, sizeof(record.name), "%s", runner->frontend->build);
    snprintf(record.mode, sizeof(record.mode), "%s", run_key);
    ReportJournalWrite(&record);
    if (chosen[0] != '\0') {
        memset(&record, 0, sizeof(record));
        record.kind = JOURNAL_CHOSEN;
        snprintf(record.mode, sizeof(record.mode), "%s", chosen);
        ReportJournalWrite(&record);
    }
    JournalSimple(JOURNAL_LAUNCH, NULL, 1u);
}

_Static_assert(REPORT_SKIP_SUITES_MAX >= MODE_SKIPS_MAX && JOURNAL_SUITE_MAX > MODE_SKIP_NAME_MAX,
               "every suite skip= names reaches the report whole");

void RunnerApplyMode(Runner *runner, const RunMode *mode) {
    ReportSetRetry(mode->retry);
    ReportSetHang(mode->hang);
    ReportSetMachine(mode->machine, mode->machine_version);
    ReportSetUnban(mode->unban);
    const char *skips[MODE_SKIPS_MAX];
    for (uint32_t i = 0; i < mode->skip_count && i < MODE_SKIPS_MAX; ++i) {
        skips[i] = mode->skip[i];
    }
    ReportSetSkipSuites(skips,
                        mode->skip_count < MODE_SKIPS_MAX ? mode->skip_count : MODE_SKIPS_MAX);
    if (runner->plan.start == JOURNAL_FRESH) {
        char described[REPORT_NOTE_MAX + 1];
        ModeDescribeOf(described, sizeof(described), mode, runner->frontend->present,
                       runner->frontend->asked);
        ReportNote("%s", described);
        char bans[REPORT_NOTE_MAX - 8];
        if (BanDescribe(bans, sizeof(bans), mode->machine, mode->machine_version, mode->unban) >
            0u) {
            ReportNote("bans: %s", bans);
        }
    }
}

/// Whether @p step runs in a run of @p families: the steps every mode runs, and its families'.
static bool StepRuns(const RunnerStep *step, uint32_t families) {
    return step->family == 0u || (families & step->family) != 0u;
}

/// Runs one drawing suite for its time and then for its verdict.
///
/// Each suite opens its own graphics device and closes it before it returns, so the passes before
/// the verdict pass leave it nothing to inherit but the emulator's shader cache, which a second
/// `make run` inherits anyway. They are muted: no line, no count, no picture. The verdict pass is
/// the same code with the report open, and each of its lines carries the smallest of its samples.
///
/// A hang in a rehearsal is read from three places: the report ends on this suite's timing note,
/// which says why nothing of the suite follows it, the emulator's log ends on the
/// `AMBERLITH TIMING` line naming the pass and the case that was about to run, and the journal
/// ends on that case's start. With the mode word `notime` none of this happens and the suite
/// runs once, as it did before cases were timed.
static void RunTimed(const char *suite, void (*run)(void), bool timing) {
    if (!timing) {
        run();
        return;
    }
    ReportNote("timing %s: %u warm-up and %u rehearsals before the verdict pass", suite,
               (unsigned)PERF_WARMUPS, (unsigned)PERF_REHEARSALS);

    PerfSuiteBegin();
    for (unsigned i = 0; i < PERF_WARMUPS + PERF_REHEARSALS; ++i) {
        PerfPassBegin(i < PERF_WARMUPS ? PERF_WARMUP : PERF_REHEARSAL);
        JournalSimple(JOURNAL_PASS, suite, PerfPassNumber());
        // After the pass has begun, so the first breadcrumb carries its number.
        ReportRehearse(suite);
        run();
    }
    ReportRehearse(NULL);
    PerfPassBegin(PERF_VERDICT);
    JournalSimple(JOURNAL_PASS, suite, PerfPassNumber());
    run();
    PerfPassBegin(PERF_UNTIMED);
    ReportPerf(suite);
}

/// One step, journaled before and after, with its passes when it is timed.
static void RunStep(const RunnerStep *step, uint32_t number, bool timing,
                    RunnerProgress progress, const RunnerFrontend *frontend) {
    JournalRecord record;
    memset(&record, 0, sizeof(record));
    record.kind = JOURNAL_STEP;
    snprintf(record.suite, sizeof(record.suite), "%s", step->name);
    record.offset = ReportOffset();
    record.counts = ReportCounts();
    ReportJournalWrite(&record);
    if (progress != NULL) {
        progress(number, step->name, step->timed && timing);
    }
    ReportRequireStarts(step->asks);
    if (step->timed) {
        RunTimed(step->name, step->run, timing);
    } else {
        step->run();
    }
    ReportRequireStarts(false);
    if (frontend->step_done != NULL) {
        frontend->step_done(step->name);
    }
    JournalSimple(JOURNAL_DONE, step->name, 0);
}

void RunnerStepsWhere(Runner *runner, bool (*runs)(const RunnerStep *step), bool timing,
                      RunnerProgress progress) {
    const RunnerFrontend *const frontend = runner->frontend;
    BaselineReset();
    uint32_t step_number = 0;
    for (size_t i = 0; i < frontend->step_count; ++i) {
        if (runs != NULL && runs(&frontend->steps[i])) {
            RunStep(&frontend->steps[i], ++step_number, timing, progress, frontend);
        }
    }
}

typedef struct {
    void (*run)(const void *);
    const void *argument;
} Extra;

static const Extra *extra;

static void RunExtra(void) {
    extra->run(extra->argument);
}

void RunnerRunExtra(Runner *runner, uint32_t number, const char *name, void (*run)(const void *),
                    const void *argument, RunnerProgress progress) {
    const Extra that = { run, argument };
    extra = &that;
    const RunnerStep step = { name, RunExtra, 0u, false, true };
    RunStep(&step, number, false, progress, runner->frontend);
    extra = NULL;
}

uint32_t RunnerPlanBrowser(Runner *runner, const RunMode *mode) {
    char folder[MODE_FOLDER_MAX + 1u];
    if (ModeRunFolder(folder, sizeof(folder), runner->frontend->hash, mode) == 0u) {
        snprintf(folder, sizeof(folder), "unknown");
    }
    // Room after it for a run's number, so every path under it fits RUNNER_PATH_MAX.
    char base[RUNNER_PATH_MAX - 8u];
    const int wrote = snprintf(base, sizeof(base), "%s/%s/browse", runner->runs_directory, folder);
    if (wrote < 0 || (size_t)wrote >= sizeof(base)) {
        return 0u;
    }
    mkdir(runner->runs_directory, 0777);
    char launch[RUNNER_PATH_MAX];
    snprintf(launch, sizeof(launch), "%s/%s", runner->runs_directory, folder);
    mkdir(launch, 0777);
    mkdir(base, 0777);
    // The first number with no folder yet, so a run never writes over one before it.
    uint32_t number = 0;
    for (uint32_t n = 1; n <= RUNNER_BROWSER_RUNS_MAX && number == 0u; ++n) {
        char path[RUNNER_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%u", base, (unsigned)n);
        struct stat about;
        if (stat(path, &about) != 0) {
            number = n;
        }
    }
    if (number == 0u) {
        return 0u;
    }
    snprintf(runner->run_directory, sizeof(runner->run_directory), "%s/%u", base,
             (unsigned)number);
    snprintf(runner->report_path, sizeof(runner->report_path), "%s/report.jsonl",
             runner->run_directory);
    snprintf(runner->journal_path, sizeof(runner->journal_path), "%s/%s", runner->run_directory,
             JOURNAL_FILE_NAME);
    mkdir(runner->run_directory, 0777);
    char last[RUNNER_PATH_MAX];
    snprintf(last, sizeof(last), "runs/%s/browse/%u\n", folder, (unsigned)number);
    (void)JournalWriteFile(runner->last_run_path, last);
    PictureSetRoot(runner->run_directory);
    // The crash list as the card has it now: a run of this session may have added to it.
    CrashListInit(&runner->crashes);
    {
        static char text[CRASH_LIST_MAX * (JOURNAL_SUITE_MAX + JOURNAL_NAME_MAX + 2u) + 1024u];
        const int64_t length = JournalReadFile(runner->crash_list_path, text, sizeof(text), NULL);
        if (length > 0) {
            CrashListParse(&runner->crashes, text, (size_t)length);
        }
    }
    JournalStateInit(&runner->journal);
    memset(&runner->plan, 0, sizeof(runner->plan));
    runner->plan.start = JOURNAL_FRESH;
    runner->plan.launch = 1u;
    runner->plan.why_new = JOURNAL_NEW_NO_JOURNAL;
    return number;
}

void RunnerAdoptBrowserCrash(Runner *runner) {
    if (runner->browser_last[0] == '\0') {
        return;
    }
    char path[RUNNER_PATH_MAX + RUNNER_ROOT_MAX + 16u];
    snprintf(path, sizeof(path), "%s/%s/%s", runner->root, runner->browser_last,
             JOURNAL_FILE_NAME);
    static JournalState journal;
    JournalStateInit(&journal);
    if (!JournalReadPath(&journal, path)) {
        return;
    }
    // A case begun and never ended in a run that never finished: the program ended inside it.
    // A runner's time limit is not the case's, and leaves the list alone, as in any run.
    if (journal.finished || !journal.in_case || journal.timed_out) {
        return;
    }
    const bool listed = ReportCrashListAdd(journal.started.suite, journal.started.name);
    ReportNote("browser: the program ended inside %s case \"%s\" in the browser's run %s, so it "
               "goes on the crash list%s",
               journal.started.suite, journal.started.name, runner->browser_last,
               listed ? "" : " (it was there already, or the list could not be written)");
}

void RunnerSteps(Runner *runner, uint32_t families, bool timing, RunnerProgress progress) {
    const RunnerFrontend *const frontend = runner->frontend;
    const JournalPlan *const plan = &runner->plan;
    const JournalState *const journal = &runner->journal;
    BaselineReset();
    uint32_t step_number = 0;
    for (size_t i = 0; i < frontend->step_count; ++i) {
        const RunnerStep *const step = &frontend->steps[i];
        if (!StepRuns(step, families)) {
            continue;
        }
        ++step_number;
        if (plan->start != JOURNAL_FRESH &&
            (JournalStepDone(journal, step->name) ||
             (plan->start == JOURNAL_RESUME_STEP && journal->in_step &&
              strcmp(journal->step.suite, step->name) == 0))) {
            continue;
        }
        RunStep(step, step_number, timing, progress, frontend);
    }
}

void RunnerFinish(Runner *runner, const RunMode *mode, bool timing) {
    const JournalPlan *const plan = &runner->plan;
    {
        // A line that holds here and that a suite which ran never reached names a case this build
        // lacks: it bans nothing, and says so.
        char unreached[REPORT_NOTE_MAX - 96];
        if (ReportBansUnreached(unreached, sizeof(unreached)) > 0u) {
            ReportNote("bans: source/bans.txt bans %s on %s, which the suites that ran never "
                       "reached",
                       unreached, ReportMachine());
        }
    }
    {
        // A suite skip= names that no case of this launch asked to start in is a name this build
        // lacks, or a suite the run left out: the word left nothing out. Said only by a launch
        // that began the run, since one that goes on with it skips the steps the launches before
        // it ran.
        char unreached[REPORT_NOTE_MAX - 160];
        if (plan->start == JOURNAL_FRESH &&
            ReportSkipsUnreached(unreached, sizeof(unreached)) > 0u) {
            ReportNote("skip: no case of %s asked to start in this run, so skip= left nothing of "
                       "it out: a suite this build does not have, or one the families that ran do "
                       "not hold",
                       unreached);
        }
    }
    // Only when some close had to wait: a machine that delivers its copies before the queue is
    // idle, as the console does, gets the same report it always did.
    char settled[REPORT_NOTE_MAX + 1];
    if (SettleDescribe(settled, sizeof(settled), SettleRun())) {
        ReportNote("%s", settled);
    }
    // The note before the summary of a run that took more than one launch or skipped cases as
    // crashed: how many launches, how many such cases, and which ended the program in this run.
    {
        char text[REPORT_NOTE_MAX + 1];
        if (JournalDescribeRun(text, sizeof(text), plan, &runner->journal,
                               (uint32_t)ReportCrashed()) > 0u) {
            ReportNote("%s", text);
        }
    }
    // After every suite and before the summary, which it leaves as it is: a baseline's lines say
    // how this run's times compare, never whether a case passed.
    (void)BaselineFinish(runner->baseline_directory, mode->baseline.name, mode->compare.name,
                         runner->frontend->build, mode->label, timing,
                         plan->start != JOURNAL_FRESH, BaselineToReport());
    ReportSummary();

    ReportClose();
    JournalRecord finished;
    memset(&finished, 0, sizeof(finished));
    finished.kind = JOURNAL_FINISHED;
    finished.counts = ReportCounts();
    ReportJournalWrite(&finished);
    ReportJournalClose();

    // A runner watching the log knows the process is finished and what it decided, without
    // parsing the lines above or waiting out a timeout.
    char done[160];
    snprintf(done, sizeof(done), "AMBERLITH DONE passed=%zu failed=%zu skipped=%zu crashed=%zu",
             ReportPassed(), ReportFailed(), ReportSkipped(), ReportCrashed());
    PlatformLog(done, strlen(done));
}
