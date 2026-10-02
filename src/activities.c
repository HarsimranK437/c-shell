#include <stdio.h>
#include <string.h>
#include "../include/shell.h"
int activities(tok* toks)
{
    for (int i = 0; i < j_cnt; i++)
    {
        if (jobs[i].active == 1)
        {
            char state_str[32] = "Running";
            char stat_path[256];
            snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", jobs[i].pid);
            FILE *f = fopen(stat_path, "r");
            if (f)
            {
                int u_pid;
                char u_cmd[256];
                char state_char;
                if (fscanf(f, "%d %s %c", &u_pid, u_cmd, &state_char) == 3)
                {
                    if (state_char == 'T') 
                    {
                        strcpy(state_str, "Stopped");
                    }
                }
                fclose(f);
            }
            if (jobs[i].pid == jobs[i].pgid) 
            {
                printf("[%d] pgid %d\n", jobs[i].jid, jobs[i].pgid);
            }
            printf("  %d %s %s\n", jobs[i].pid, jobs[i].cmd, state_str);
        }
    }
    return 1;
}