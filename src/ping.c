#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include "../include/shell.h"
extern p_info jobs[1024];
extern int j_cnt;
int ping(tok *toks)
{
    if (toks[1].type != WORD || !toks[1].val || toks[2].type != WORD || !toks[2].val || toks[3].type != END)
    {
        printf("ping: invalid syntax\n");
        return 1;
    }
    char *target_str = toks[1].val;
    char *sig_str = toks[2].val;
    for (int i = 0; sig_str[i] != '\0'; i++)
    {
        if (sig_str[i] < '0' || sig_str[i] > '9')
        {
            printf("ping: invalid syntax\n");
            return 1;
        }
    }
    int sig_orig = atoi(sig_str);
    int sig_actual = sig_orig % 64;
    int is_job = 0;
    int target_id = -1;
    if (target_str[0] == '%')
    {
        is_job = 1;
        target_id = atoi(target_str + 1);
    }
    else
    {
        target_id = atoi(target_str);
    }
    int found = 0;
    pid_t kill_target = -1;
    for (int i = 0; i < j_cnt; i++)
    {
        if (is_job && jobs[i].jid == target_id)
        {
            found = 1;
            kill_target = -jobs[i].pgid;
            break;
        }
        else if (!is_job && jobs[i].pid == target_id)
        {
            found = 1;
            kill_target = jobs[i].pid;
            break;
        }
    }
    if (!found)
    {
        printf("ping: no such process found\n");
        return 1;
    }
    kill(kill_target, sig_actual);
    printf("Sent signal %d to %s\n", sig_orig, target_str);
    return 1;
}