#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <time.h>
#include "../include/shell.h"
extern char **environ;
typedef struct 
{
    long syscall_nr;
    char name[64];
    long calls;
    double total_time;
    int first_occurrence;
} syscall_stat;
static const char* get_syscall_name(long nr) 
{
    switch (nr) {
        case 0: return "read";
        case 1: return "write";
        case 2: return "open";
        case 3: return "close";
        case 8: return "lseek";
        case 9: return "mmap";
        case 10: return "mprotect";
        case 11: return "munmap";
        case 12: return "brk";
        case 21: return "access";
        case 35: return "nanosleep";
        case 39: return "getpid";
        case 57: return "fork";
        case 59: return "execve";
        case 60: return "exit";
        case 96: return "gettimeofday";
        case 228: return "clock_gettime";
        case 230: return "nanosleep";
        case 231: return "exit_group";
        case 247: return "wait4";
        case 257: return "openat";
        default: return NULL;
    }
}
static int compare_syscalls(const void *a, const void *b) 
{
    const syscall_stat *s1 = (const syscall_stat *)a;
    const syscall_stat *s2 = (const syscall_stat *)b;
    if (s1->calls != s2->calls) 
    {
       if(s2->calls > s1->calls)
       {
            return 1;
       }
       else
       {
            return -1;
       }
    }
    return s1->first_occurrence - s2->first_occurrence;
}
int snoop(tok *toks) 
{
    if (toks[1].type == END) 
    {
        printf("snoop: invalid syntax\n");
        return 1;
    }
    pid_t pid = -1;
    int is_attach = 0;
    int cmd_start_idx = 1;
    char resolved_path[512];
    if (strcmp(toks[1].val, "-p") == 0) 
    {
        if (toks[2].type != WORD || toks[3].type != END) 
        {
            printf("snoop: invalid syntax\n");
            return 1;
        }
        char *pid_str = toks[2].val;
        for (int i = 0; pid_str[i] != '\0'; i++) 
        {
            if (pid_str[i] < '0' || pid_str[i] > '9') 
            {
                printf("snoop: no such process\n");
                return 1;
            }
        }
        pid = atoi(pid_str);
        char proc_path[256];
        snprintf(proc_path, sizeof(proc_path), "/proc/%d", (int)pid);
        struct stat st;
        if (stat(proc_path, &st) < 0) 
        {
            printf("snoop: no such process\n");
            return 1;
        }
        is_attach = 1;
    } 
    else 
    {
        char *cmd = toks[1].val;
        int found = 0;
        if (strchr(cmd, '/')) 
        {
            if (access(cmd, X_OK) == 0) 
            {
                snprintf(resolved_path, sizeof(resolved_path), "%s", cmd);
                found = 1;
            }
        } 
        else 
        {
            if (access(cmd, X_OK) == 0) 
            {
                snprintf(resolved_path, sizeof(resolved_path), "./%s", cmd);
                found = 1;
            }
            else 
            {
                char *path_env = getenv("PATH");
                if (path_env)
                {
                    char path_copy[1024];
                    strncpy(path_copy, path_env, sizeof(path_copy) - 1);
                    char *dir = strtok(path_copy, ":");
                    while (dir) 
                    {
                        char full_path[512];
                        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
                        if (access(full_path, X_OK) == 0) 
                        {
                            snprintf(resolved_path, sizeof(resolved_path), "%s", full_path);
                            found = 1;
                            break;
                        }
                        dir = strtok(NULL, ":");
                    }
                }
            }
        }
        if (!found) 
        {
            printf("snoop: command not found\n");
            return 1;
        }
    }
    char *cmd_argv[256];
    if (!is_attach) 
    {
        int idx = 0;
        while (toks[cmd_start_idx].type == WORD && idx < 255) 
        {
            cmd_argv[idx++] = toks[cmd_start_idx++].val;
        }
        cmd_argv[idx] = NULL;
    }
    if (!is_attach){
        pid = fork();
        if (pid < 0) 
        {
            return 1;
        }
        if (pid == 0) 
        {
            if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) exit(1);
            execve(resolved_path, cmd_argv, environ);
            exit(1);
        }
    } 
    else 
    {
        if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) 
        {
            printf("snoop: no such process\n");
            return 1;
        }
    }
    int status;
    waitpid(pid, &status, 0);
    ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACESYSGOOD);
    syscall_stat stats[512];
    int stat_count = 0;
    int in_syscall = 0;
    long orig_rax = -1;
    struct timespec start_time, end_time;
    while (1) 
    {
        if (ptrace(PTRACE_SYSCALL, pid, NULL, 0) < 0) 
        {
            break;
        }
        if (waitpid(pid, &status, 0) < 0) 
        {
            break;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) 
        {
            if (in_syscall == 1 && orig_rax >= 0) 
            {
                clock_gettime(CLOCK_MONOTONIC, &end_time);
                double elapsed = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
                const char *name = get_syscall_name(orig_rax);
                char resolved_name[64];
                if (name) 
                {
                    snprintf(resolved_name, sizeof(resolved_name), "%s", name);
                } 
                else 
                {
                    snprintf(resolved_name, sizeof(resolved_name), "syscall_%ld", orig_rax);
                }
                int found_idx = -1;
                for (int i = 0; i < stat_count; i++) 
                {
                    if (strcmp(stats[i].name, resolved_name) == 0) 
                    {
                        found_idx = i;
                        break;
                    }
                }
                if (found_idx != -1) 
                {
                    stats[found_idx].calls++;
                    stats[found_idx].total_time += elapsed;
                } 
                else if (stat_count < 512) 
                {
                    stats[stat_count].syscall_nr = orig_rax;
                    snprintf(stats[stat_count].name, sizeof(stats[stat_count].name), "%s", resolved_name);
                    stats[stat_count].calls = 1;
                    stats[stat_count].total_time = elapsed;
                    stats[stat_count].first_occurrence = stat_count;
                    stat_count++;
                }
            }
            break;
        }
        if (WIFSTOPPED(status)) 
        {
            int sig = WSTOPSIG(status);
            if (!(sig & 0x80) && sig != SIGTRAP) 
            {
                continue;
            }
        }
        struct user_regs_struct regs;
        if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) 
        {
            continue;
        }
        if (in_syscall == 0) 
        {
            orig_rax = regs.orig_rax;
            if (orig_rax >= 0) 
            {
                clock_gettime(CLOCK_MONOTONIC, &start_time);
                in_syscall = 1;
            }
        } 
        else 
        {
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double elapsed = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
            const char *name = get_syscall_name(orig_rax);
            char resolved_name[64];
            if (name) 
            {
                snprintf(resolved_name, sizeof(resolved_name), "%s", name);
            } 
            else 
            {
                snprintf(resolved_name, sizeof(resolved_name), "syscall_%ld", orig_rax);
            }
            int found_idx = -1;
            for (int i = 0; i < stat_count; i++) 
            {
                if (strcmp(stats[i].name, resolved_name) == 0) 
                {
                    found_idx = i;
                    break;
                }
            }
            if (found_idx != -1) 
            {
                stats[found_idx].calls++;
                stats[found_idx].total_time += elapsed;
            } 
            else if (stat_count < 512) 
            {
                stats[stat_count].syscall_nr = orig_rax;
                snprintf(stats[stat_count].name, sizeof(stats[stat_count].name), "%s", resolved_name);
                stats[stat_count].calls = 1;
                stats[stat_count].total_time = elapsed;
                stats[stat_count].first_occurrence = stat_count;
                stat_count++;
            }
            in_syscall = 0;
        }
    }
    qsort(stats, stat_count, sizeof(syscall_stat), compare_syscalls);
    printf("syscall       calls   time\n");
    for (int i = 0; i < stat_count; i++)
    {
        printf("%-13s %-7ld %.3fs\n", stats[i].name, stats[i].calls, stats[i].total_time);
    }  
    return 1;
}