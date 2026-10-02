#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include "../include/shell.h"
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
p_info jobs[1024];
int j_cnt = 0;
int next_jid = 1;
int fg_run = 0;
int job_exited = 0;
void flush_notifications()
{
    if (!fg_run)
    {
        for (int i = 0; i < j_cnt; i++)
        {
            if (jobs[i].is_bg && jobs[i].active <= 0 && !jobs[i].notified)
            {
                if (jobs[i].active == 0)
                {
                    printf("%s with pid %d exited normally\n", jobs[i].cmd, jobs[i].pid);
                }
                else
                {
                    printf("%s with pid %d exited abnormally\n", jobs[i].cmd, jobs[i].pid);
                }
                jobs[i].notified = 1;
            }
        }
    }
}
void sig_chld(int sig)
{
    (void)sig;
    int st;
    pid_t p;
    while ((p = waitpid(-1, &st, WNOHANG | WUNTRACED)) > 0)
    {
        for (int i = 0; i < j_cnt; i++)
        {
            if (jobs[i].pid == p)
            {
                if (WIFEXITED(st) || WIFSIGNALED(st))
                {
                    if (WIFEXITED(st))
                    {
                        jobs[i].active = 0;
                    }
                    else
                    {
                        jobs[i].active = -1;
                    }
                    if (!fg_run && jobs[i].is_bg && !jobs[i].notified)
                    {
                        char msg[1200];
                        int l;
                        if (jobs[i].active == 0)
                        {
                            l = snprintf(msg, sizeof(msg), "%s with pid %d exited normally\n", jobs[i].cmd, p);
                        }
                        else
                        {
                            l = snprintf(msg, sizeof(msg), "%s with pid %d exited abnormally\n", jobs[i].cmd, p);
                        }
                        write(STDOUT_FILENO, msg, l);
                        jobs[i].notified = 1;
                        job_exited = 1;
                    }
                }
            }
        }
    }
}
int main()
{
    static int ctrl_d_warned = 0;
    struct sigaction sa;
    sa.sa_handler = sig_chld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa, NULL) < 0)
    {
        perror("sigaction");
        return 1;
    }
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    char home_path[PATH_MAX];
    if (getcwd(home_path, sizeof(home_path)) == NULL)
    {
        return 1;
    }
    size_t len = strlen(home_path);
    while (1)
    {
        flush_notifications();
        char cwd[PATH_MAX];
        char hostname[HOST_NAME_MAX];
        struct passwd *pw = getpwuid(getuid());
        const char *username = "unknown";
        if (pw != NULL)
        {
            username = pw->pw_name;
        }
        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            strcpy(hostname, "unknown");
        }
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            continue;
        }
        if (strncmp(cwd, home_path, len) == 0 && (cwd[len] == '\0' || cwd[len] == '/'))
        {
            printf("<%s@%s:~%s> ", username, hostname, cwd + len);
        }
        else
        {
            printf("<%s@%s:%s> ", username, hostname, cwd);
        }
        fflush(stdout);
        char *input = NULL;
        size_t input_len = 0;
        ssize_t read_res;
        do
        {
            job_exited = 0;
            clearerr(stdin);
            errno = 0;
            read_res = getline(&input, &input_len, stdin);
        } while (read_res == -1 && errno == EINTR && job_exited == 0);
        if (read_res == -1)
        {
            if (feof(stdin))
            {
                int has_stopped = 0;
                for (int k = 0; k < j_cnt; k++)
                {
                    if (jobs[k].active == 1)
                    {
                        char stat_path[256];
                        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", jobs[k].pid);
                        FILE *f = fopen(stat_path, "r");
                        if (f)
                        {
                            int p;
                            char c_name[256];
                            char state;
                            if (fscanf(f, "%d %s %c", &p, c_name, &state) == 3 && state == 'T')
                            {
                                has_stopped = 1;
                            }
                            fclose(f);
                        }
                    }
                }
                if (has_stopped && !ctrl_d_warned)
                {
                    printf("cshell: there are stopped jobs\n");
                    ctrl_d_warned = 1;
                    clearerr(stdin);
                    free(input);
                    input = NULL;
                    continue;
                }
                for (int k = 0; k < j_cnt; k++)
                {
                    if (jobs[k].active == 1)
                    {
                        kill(-jobs[k].pgid, SIGHUP);
                    }
                }
                printf("\n");
                free(input);
                break;
            }
            clearerr(stdin);
            free(input);
            continue;
        }
        ctrl_d_warned = 0;
        input[strcspn(input, "\n")] = '\0';
        int max_toks = strlen(input) + 2;
        tok *toks = malloc(max_toks * sizeof(tok));
        if (!toks)
        {
            free(input);
            continue;
        }
        int n_toks = lex(input, toks, max_toks);
        if (n_toks == -1)
        {
            free(toks);
            free(input);
            printf("cshell: invalid syntax\n");
            continue;
        }
        if (!parse(toks))
        {
            printf("cshell: invalid syntax\n");
        }
        else
        {
            int st = 0;
            while (toks[st].type != END)
            {
                int en = st;
                while (toks[en].type != END && toks[en].type != OP_SEMI && toks[en].type != OP_AMP)
                {
                    en++;
                }
                int is_bg = 0;
                if (toks[en].type == OP_AMP)
                {
                    is_bg = 1;
                }
                int c_len = en - st;
                if (c_len > 0)
                {
                    tok *c_toks = malloc((c_len + 1) * sizeof(tok));
                    for (int j = 0; j < c_len; j++)
                    {
                        c_toks[j] = toks[st + j];
                    }
                    c_toks[c_len].type = END;
                    c_toks[c_len].val = NULL;
                    int res = 1;
                    if (c_toks[0].type == WORD)
                    {
                        if (strcmp(c_toks[0].val, "hop") == 0)
                        {
                            res = hop(c_toks, home_path);
                        }
                        else if (strcmp(c_toks[0].val, "reveal") == 0)
                        {
                            res = reveal(c_toks, home_path);
                        }
                        else if (strcmp(c_toks[0].val, "peek") == 0)
                        {
                            res = peek(c_toks);
                        }
                        else if (strcmp(c_toks[0].val, "locate") == 0)
                        {
                            res = locate(c_toks);
                        }
                        else if (strcmp(c_toks[0].val, "activities") == 0)
                        {
                            res = activities(c_toks);
                        }
                        else if (strcmp(c_toks[0].val, "resume") == 0)
                        {
                            res = resume(c_toks);
                        }
                        else if (strcmp(c_toks[0].val, "ping") == 0)
                        {
                            ping(c_toks);
                        }
                        else if (strcmp(c_toks[0].val, "spy") == 0)
                        {
                            spy(c_toks);
                        }
                        else if (strcmp(c_toks[0].val, "snoop") == 0)
                        {
                            snoop(c_toks);
                        }
                        else
                        {
                            res = command(c_toks, is_bg);
                        }
                    }
                    free(c_toks);
                    if (res == 0)
                    {
                        break;
                    }
                }
                if (toks[en].type != END)
                {
                    st = en + 1;
                }
                else
                {
                    st = en;
                }
            }
        }
        flush_notifications();
        for (int i = 0; toks[i].type != END; i++)
        {
            if (toks[i].val != NULL)
            {
                free(toks[i].val);
            }
        }
        free(toks);
        free(input);
    }
    return 0;
}