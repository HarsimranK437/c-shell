#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "../include/shell.h"

extern p_info jobs[1024];
extern int j_cnt;
extern int fg_run;

static volatile int timeout_occurred = 0;

void sig_alarm_handler(int sig)
{
    (void)sig;
    timeout_occurred = 1;
}

int resume(tok *toks)
{
    if (toks[1].type != WORD || !toks[1].val || toks[1].val[0] != '%')
    {
        printf("resume: invalid syntax\n");
        return 1;
    }

    int target_jid = atoi(toks[1].val + 1);
    int job_idx = -1;
    for (int i = 0; i < j_cnt; i++)
    {
        if (jobs[i].jid == target_jid)
        {
            job_idx = i;
            break;
        }
    }

    if (job_idx == -1 || jobs[job_idx].active == 0 || jobs[job_idx].active == -1)
    {
        printf("resume: no such job\n");
        return 1;
    }

    if (toks[2].type != WORD || !toks[2].val)
    {
        printf("resume: invalid syntax\n");
        return 1;
    }

    int is_fg = (strcmp(toks[2].val, "fg") == 0);
    if (!is_fg && strcmp(toks[2].val, "bg") != 0)
    {
        printf("resume: invalid syntax\n");
        return 1;
    }

    int timeout_sec = -1;
    if (is_fg && toks[3].type != END)
    {
        if (toks[3].type == WORD && toks[3].val && strcmp(toks[3].val, "--timeout") == 0 &&
            toks[4].type == WORD && toks[4].val)
        {
            timeout_sec = atoi(toks[4].val);
        }
        
        if (timeout_sec < 0)
        {
            printf("resume: invalid syntax\n");
            return 1;
        }
    }

    pid_t pgid = jobs[job_idx].pgid;
    if (kill(-pgid, SIGCONT) < 0)
    {
        printf("resume: no such job\n");
        return 1;
    }

    if (!is_fg)
    {
        jobs[job_idx].active = 1;
        printf("[%d] + Running    %s\n", jobs[job_idx].jid, jobs[job_idx].cmd);
        fflush(stdout);
        return 1;
    }

    // Foreground execution path
    printf("%s\n", jobs[job_idx].cmd);
    fflush(stdout);
    tcsetpgrp(STDIN_FILENO, pgid);
    fg_run = 1;
    jobs[job_idx].active = 1;
    jobs[job_idx].is_bg = 0;
    jobs[job_idx].notified = 1;

    int status = 0;
    pid_t wpid = -1;

    if (timeout_sec > 0)
    {
        timeout_occurred = 0;
        struct sigaction sa_old, sa_new = {0};
        sa_new.sa_handler = sig_alarm_handler;
        sigaction(SIGALRM, &sa_new, &sa_old);
        alarm(timeout_sec);

        while (1)
        {
            wpid = waitpid(jobs[job_idx].pid, &status, WUNTRACED);
            if (wpid > 0 || timeout_occurred || errno != EINTR)
            {
                break;
            }
        }

        if (timeout_occurred)
        {
            kill(-pgid, SIGTERM);
            printf("resume: job timed out\n");
            jobs[job_idx].active = -1;
        }

        alarm(0);
        sigaction(SIGALRM, &sa_old, NULL);
    }
    else
    {
        do
        {
            wpid = waitpid(jobs[job_idx].pid, &status, WUNTRACED);
        } 
        while (wpid == -1 && errno == EINTR);
    }

    fg_run = 0;
    tcsetpgrp(STDIN_FILENO, getpgrp());

    if (!timeout_occurred && wpid > 0)
    {
        if (WIFSTOPPED(status))
        {
            jobs[job_idx].active = 1;
            printf("\n[%d] + Stopped    %s\n", jobs[job_idx].jid, jobs[job_idx].cmd);
            fflush(stdout);
        }
        else if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            jobs[job_idx].active = 0;
        }
    }

    return 1;
}