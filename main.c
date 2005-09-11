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
#include <sys/stat.h>
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
    int i, j, len, stillruns, ksig;
    char buf[256];
    char *runsfile, *binfile, *pidsfile, *startfile;
    int pids[256];
    struct stat sb;

    atexit(ehandler);
    glog = siplog_open("sipwd", NULL, LF_REOPEN);
    siplog_write(SIPLOG_ALL, glog, "sipwd started, pid %d", getpid());

    if (argc != 5) {
        siplog_write(SIPLOG_ERR, glog, "usage: sipwd runsfile binfile pidsfile startfile");
        exit(1);
    }
    runsfile = argv[1];
    binfile = argv[2];
    pidsfile = argv[3];
    startfile = argv[4];
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
    if (i > 0 && pids[i] == 0)
        exit(0);
    ksig = SIGTERM;
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
