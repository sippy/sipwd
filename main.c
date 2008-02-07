/*
 * Copyright (c) 2004 Maxim Sobolev
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 *
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <sys/statfs.h>
#include <linux/proc_fs.h>
#endif
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <siplog.h>

siplog_t glog;

static void
ehandler(void)
{

    siplog_write(SIPLOG_ALL, glog, "sipwd ended");
    siplog_close(glog);
}

#if defined(__linux__)
static size_t
strlcpy(char *dst, const char *src, size_t size)
{

    dst[size - 1] = '\0';
    strncpy(dst, src, size - 1);
    return strlen(src);
}
#endif

static int
equal_basenames(const char *name1, const char *name2)
{
    char buf1[MAXPATHLEN], buf2[MAXPATHLEN];
    /*
     * FIXME: BSD basename() returns pointer to internal static buffer.
     * If some implementation makes in-place modification, as permitted
     * by POSIX, this must be updated to reflect that fact.
     */
    strlcpy(buf1, basename(name1), sizeof(buf1));
    strlcpy(buf2, basename(name2), sizeof(buf2));
    return 0 == strcmp(buf1, buf2);
}

int
main(int argc, char **argv)
{
    FILE *f;
    int i, j, len, stillruns;
    int ksig = SIGTERM;
    char buf[256];
    char *runsfile, *binfile, *pidsfile, *startfile;
    char *force_restart_file = NULL;
    int pids[256];
    struct stat sb;
    struct statfs fs_stat;
    int force_restart = 0;

    atexit(ehandler);
    glog = siplog_open("sipwd", NULL, LF_REOPEN);
    siplog_write(SIPLOG_ALL, glog, "sipwd started, pid %d", getpid());

    if (argc < 5 || argc > 6) {
        siplog_write(SIPLOG_ERR, glog, "usage: sipwd runsfile binfile pidsfile startfile [ force_restart_file ]");
        exit(1);
    }
    if (statfs("/proc", &fs_stat) == -1 ||
#if defined(__linux__)
        fs_stat.f_type != PROC_SUPER_MAGIC)
#else
        strcmp(fs_stat.f_fstypename, "procfs") != 0)
#endif
    {
        siplog_write(SIPLOG_ERR, glog, "the proc filesystem not mounted on /proc. Exiting...");
        exit(1);
    }
    runsfile = argv[1];
    binfile = argv[2];
    pidsfile = argv[3];
    startfile = argv[4];
    if (argc == 6)
        force_restart_file = argv[5];
    if (stat(runsfile, &sb) == -1)
        exit(0);
    f = fopen(pidsfile, "r");
    if (f != NULL) {
        for (i = 0; fgets(buf, sizeof(buf), f) != NULL; i++) {
            for (j = strlen(buf) - 1; j >= 0 && strchr("\r\n\t ", buf[j]) != NULL; j--)
                buf[j] = '\0';
            if (j < 0)
                continue;
            pids[i] = atoi(buf);
            if (pids[i] <= 1) {
                siplog_ewrite(SIPLOG_ERR, glog, "invalid pid: %s", buf);
                exit(2);
            }
        }
        fclose(f);
    } else {
        i = 0;
    }
    pids[i] = 0;
    if (force_restart_file != NULL &&
        stat(force_restart_file, &sb) != -1 &&
        S_ISREG(sb.st_mode))
    {
        force_restart = 1;
        siplog_write(SIPLOG_ALL, glog, "forcibly restarting %s", binfile);
        i = 0;
        ksig = SIGABRT;
        unlink(force_restart_file);
    }
    else {
        for (i = 0; pids[i] != 0; i++) {
            sprintf(buf, "/proc/%d/cmdline", pids[i]);
            f = fopen(buf, "r");
            if (f == NULL) {
                siplog_write(SIPLOG_ALL, glog, "%s pid %d is not running", binfile, pids[i]);
                break;
            }
            len = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            if (len == 0) {
                siplog_write(SIPLOG_ALL, glog, "%s pid %d is not running", binfile, pids[i]);
                break;
            }
            buf[len] = '\0';
            if (!equal_basenames(buf, binfile)) {
                siplog_write(SIPLOG_ALL, glog, "%s pid %d is not running", binfile, pids[i]);
                break;
            }
        }
    }
    if (i > 0 && pids[i] == 0)
        exit(0);
    for (j = 0; j < 10; j++) {
        stillruns = 0;
        for (i = 0; pids[i] != 0; i++) {
            sprintf(buf, "/proc/%d/cmdline", pids[i]);
            f = fopen(buf, "r");
            if (f == NULL)
                continue;
            len = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            if (len == 0)
                continue;
            buf[len] = '\0';
            if (!equal_basenames(buf, binfile))
                continue;
            siplog_write(SIPLOG_ALL, glog, "sending signal %d to %s pid %d", ksig, binfile, pids[i]);
            kill(pids[i], ksig);
            stillruns = 1;
        }
        if (stillruns == 0)
            break;
        if (j == 7)
            ksig = SIGKILL;
        siplog_write(SIPLOG_ALL, glog, "waiting for all %s to die", binfile);
        sleep(1);
    }
    siplog_write(SIPLOG_ALL, glog, "starting %s", binfile);
    system(startfile);
    exit(0);
}

/* vim:ts=4:sts=4:sw=4:et:
 */
