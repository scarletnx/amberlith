// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later
/* What threads that ran native code leave when they end: the ones Java made, and native ones that attached to the JVM.
   Each fills what a thread keeps (its JNIEnv and local references, its stack, a looper, a pthread key, a thread-local,
   errno, a signal that waits), and what is left after them is read from the process: who still takes a signal,
   /proc/self/task, its epoll sets, its address space and resident memory. */
#include "probe.h"
#include "report.h"

#include <android/looper.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* the stack each thread touches; and what may be left of the process's noise (a pool's thread, the runtime's heap):
   the threads after the mark touch twice RESIDENT_MIB, and a stack of 8 MiB each is four times ADDRESS_MIB */
enum { THREADS = 2048, STACK_BYTES = 256 * 1024, EPOLLS = 32, ADDRESS_MIB = 1024, RESIDENT_MIB = 64 };

typedef struct {
    long epolls, address, resident; /* epoll sets, MiB, MiB; -1 where it could not be read */
} Usage;

static struct {
    pthread_key_t key;
    _Atomic int count, stale_key, stale_local, stale_errno, loopers;
    pid_t tid[THREADS];
    int waited; /* how many of tid have been waited for */
    Usage mark;
} g_ends;
static _Thread_local int t_local = 7;

static Usage usage(void) {
    Usage u = {-1, -1, -1};
    FILE* f = fopen("/proc/self/statm", "r");
    long size = 0, resident = 0;
    if (f && fscanf(f, "%ld %ld", &size, &resident) == 2) {
        const long per_mib = (1l << 20) / sysconf(_SC_PAGESIZE);
        u.address = size / per_mib;
        u.resident = resident / per_mib;
    }
    if (f) fclose(f);
    DIR* d = opendir("/proc/self/fd");
    if (d) u.epolls = 0;
    for (struct dirent* e; d && (e = readdir(d));) {
        char path[300], to[64];
        snprintf(path, sizeof path, "/proc/self/fd/%s", e->d_name);
        ssize_t n = readlink(path, to, sizeof to - 1);
        to[n > 0 ? n : 0] = 0;
        u.epolls += strstr(to, "[eventpoll]") != NULL;
    }
    if (d) closedir(d);
    return u;
}

static void fill(JNIEnv* env) {
    if (pthread_getspecific(g_ends.key)) ++g_ends.stale_key;
    if (t_local != 7) ++g_ends.stale_local;
    t_local = 1000;
    pthread_setspecific(g_ends.key, &g_ends);
    if (ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS)) ++g_ends.loopers;
    unsigned char stack[STACK_BYTES];
    memset(stack, 1, sizeof stack);
    __asm__ volatile("" : : "r"(stack) : "memory");
    (*env)->PushLocalFrame(env, 16);
    for (int i = 0; i < 8; ++i) (*env)->NewStringUTF(env, "a local reference");
    (*env)->PopLocalFrame(env, NULL);
    (*env)->NewStringUTF(env, "one left to the thread's end");
    /* a signal that waits for a thread that will not take it */
    sigset_t usr1;
    sigemptyset(&usr1);
    sigaddset(&usr1, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &usr1, NULL);
    tgkill(getpid(), gettid(), SIGUSR1);
    int i = g_ends.count++;
    if (i < THREADS) g_ends.tid[i] = gettid();
    errno = ENOTTY;
}

static void* attached(void* arg) {
    (void)arg;
    if (errno) ++g_ends.stale_errno;
    JavaVM* vm = probe_vm();
    JNIEnv* env = NULL;
    if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK) return NULL;
    fill(env);
    (*vm)->DetachCurrentThread(vm);
    return NULL;
}

JNIEXPORT void JNICALL PROBE_FN(threadEndsStart)(JNIEnv* env, jclass cls) {
    (void)env;
    (void)cls;
    memset(&g_ends, 0, sizeof g_ends);
    pthread_key_create(&g_ends.key, NULL);
}

/* on each of Java's threads */
JNIEXPORT void JNICALL PROBE_FN(threadEndsFill)(JNIEnv* env, jclass cls) {
    (void)cls;
    fill(env);
}

/* count native threads at once, each attached, ended and joined; how many ran */
JNIEXPORT jint JNICALL PROBE_FN(threadEndsAttached)(JNIEnv* env, jclass cls, jint count) {
    (void)env;
    (void)cls;
    pthread_t t[64];
    int made = 0;
    while (made < count && made < 64 && !pthread_create(&t[made], NULL, attached, NULL)) ++made;
    for (int i = 0; i < made; ++i) pthread_join(t[i], NULL);
    return made;
}

static int listed(pid_t tid) {
    char path[64];
    snprintf(path, sizeof path, "/proc/self/task/%ld", (long)tid);
    return !access(path, F_OK);
}

/* Thread.join and pthread_join return before the kernel's thread is gone: waits for the ones made so far, 5 s at most */
JNIEXPORT void JNICALL PROBE_FN(threadEndsWait)(JNIEnv* env, jclass cls) {
    (void)env;
    (void)cls;
    const struct timespec tick = {0, 5000000};
    for (int left = 1000; g_ends.waited < g_ends.count && g_ends.waited < THREADS; ++g_ends.waited)
        while (listed(g_ends.tid[g_ends.waited]) && left-- > 0) nanosleep(&tick, NULL);
}

/* the process from here on: what the threads after this one leave is told from what the first ones set up */
JNIEXPORT void JNICALL PROBE_FN(threadEndsMark)(JNIEnv* env, jclass cls) {
    (void)env;
    (void)cls;
    g_ends.mark = usage();
}

static const char* grown(char* out, size_t size, long from, long to, long bound) {
    if (from < 0 || to < 0) snprintf(out, size, "unreadable");
    else if (to - from < bound) snprintf(out, size, "given back");
    else snprintf(out, size, "%ld more", to - from);
    return out;
}

JNIEXPORT jstring JNICALL PROBE_FN(threadEndsLeft)(JNIEnv* env, jclass cls) {
    (void)cls;
    Usage u = usage();
    int n = g_ends.count < THREADS ? g_ends.count : THREADS, in_list = 0, gone = 0;
    for (int i = 0; i < n; ++i) {
        in_list += listed(g_ends.tid[i]);
        errno = 0;
        gone += tgkill(getpid(), g_ends.tid[i], 0) == -1 && errno == ESRCH;
    }
    ReportNote("jni: of %d threads that ended, %d are no thread to a signal; since the mark the epoll sets are %ld more, "
               "the address space %ld MiB more, the resident memory %ld MiB more",
               n, gone, u.epolls - g_ends.mark.epolls, u.address - g_ends.mark.address, u.resident - g_ends.mark.resident);
    pthread_key_delete(g_ends.key);
    char epolls[32], address[32], resident[32];
    return probe_string(env, "%d threads, %d still listed|no thread to a signal %d|loopers %d|stale key %d, thread-local %d, errno %d|"
                             "epoll sets %s|address space %s|resident memory %s",
                        n, in_list, gone, g_ends.loopers, g_ends.stale_key, g_ends.stale_local, g_ends.stale_errno,
                        grown(epolls, sizeof epolls, g_ends.mark.epolls, u.epolls, EPOLLS),
                        grown(address, sizeof address, g_ends.mark.address, u.address, ADDRESS_MIB),
                        grown(resident, sizeof resident, g_ends.mark.resident, u.resident, RESIDENT_MIB));
}
