#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include "../include/shell.h"
extern char **environ;
int single_command(tok *toks, int is_bg)
{
    if (toks[0].type != WORD)
    {
        return 1;
    }
    int total_t = 0;
    while (toks[total_t].type != END)
    {
        total_t++;
    }
    tok *cleanToks = malloc((total_t + 1) * sizeof(tok));
    char **inputFiles = malloc(total_t * sizeof(char *));
    char **outFiles = malloc(total_t * sizeof(char *));
    int *outModes = malloc(total_t * sizeof(int));
    int *input_fds = malloc(total_t * sizeof(int));
    int *out_fds = malloc(total_t * sizeof(int));
    if (!cleanToks || !inputFiles || !outFiles || !outModes || !input_fds || !out_fds)
    {
        return 1;
    }
    int cleanArgc = 0;
    int inputCnt = 0;
    int outCnt = 0;
    int i = 0;
    while (toks[i].type != END && toks[i].type != OP_SEMI && toks[i].type != OP_AMP)
    {
        if (toks[i].type == OP_LT && toks[i + 1].type == WORD)
        {
            inputFiles[inputCnt++] = toks[i + 1].val;
            i += 2;
            continue;
        }
        else if (toks[i].type == OP_GT && toks[i + 1].type == WORD)
        {
            outFiles[outCnt] = toks[i + 1].val;
            outModes[outCnt] = 0;
            outCnt++;
            i += 2;
            continue;
        }
        else if (toks[i].type == OP_GTGT && toks[i + 1].type == WORD)
        {
            outFiles[outCnt] = toks[i + 1].val;
            outModes[outCnt] = 1;
            outCnt++;
            i += 2;
            continue;
        }
        cleanToks[cleanArgc++] = toks[i];
        i++;
    }
    cleanToks[cleanArgc].type = END;
    cleanToks[cleanArgc].val = NULL;
    if (cleanArgc == 0)
    {
        return 1;
    }
    char full_cmd[1024] = "";
    for (int k = 0; k < cleanArgc; k++) 
    {
        strcat(full_cmd, cleanToks[k].val);
        if (k < cleanArgc - 1) 
        {
            strcat(full_cmd, " ");
        }
    }
    for (int j = 0; j < inputCnt; j++)
    {
        input_fds[j] = open(inputFiles[j], O_RDONLY);
        if (input_fds[j] < 0)
        {
            printf("cshell: no such file or directory\n");
            return 0;
        }
    }
    for (int j = 0; j < outCnt; j++)
    {
        int flags = O_WRONLY | O_CREAT;
        if (outModes[j] == 0)
        {
            flags |= O_TRUNC;
        }
        else
        {
            flags |= O_APPEND;
        }
        out_fds[j] = open(outFiles[j], flags, 0644);
        if (out_fds[j] < 0)
        {
            printf("cshell: unable to create file for writing\n");
            return 0;
        }
    }
    char *raw = cleanToks[0].val;
    char resolvedPath[PATH_MAX * 2];
    int found = 0;
    int force = 0;
    char *cmd = raw;
    if (raw[0] == '%')
    {
        force = 1;
        cmd = raw + 1;
    }
    struct stat s;
    if (!force && strchr(cmd, '/') != NULL)
    {
        if (stat(cmd, &s) == 0 && S_ISREG(s.st_mode) && access(cmd, X_OK) == 0)
        {
            snprintf(resolvedPath, sizeof(resolvedPath), "%s", cmd);
            found = 1;
        }
    }
    else if (!force)
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            snprintf(resolvedPath, sizeof(resolvedPath), "%s/%s", cwd, cmd);
            if (stat(resolvedPath, &s) == 0 && S_ISREG(s.st_mode) && access(resolvedPath, X_OK) == 0)
            {
                found = 1;
            }
        }
    }
    if (!found && (force || strchr(cmd, '/') == NULL))
    {
        const char *pathenv = getenv("PATH");
        if (pathenv != NULL)
        {
            char *copy = strdup(pathenv);
            char *dir = strtok(copy, ":");
            while (dir != NULL)
            {
                snprintf(resolvedPath, sizeof(resolvedPath), "%s/%s", dir, cmd);
                if (stat(resolvedPath, &s) == 0 && S_ISREG(s.st_mode) && access(resolvedPath, X_OK) == 0)
                {
                    found = 1;
                    break;
                }
                dir = strtok(NULL, ":");
            }
            free(copy);
        }
    }
    if (!found)
    {
        printf("cshell: command not found (%s)\n", cmd);
        return 0;
    }
    char **argv = malloc((cleanArgc + 1) * sizeof(char *));
    argv[0] = cmd;
    for (int k = 1; k < cleanArgc; k++)
    {
        argv[k] = cleanToks[k].val;
    }
    argv[cleanArgc] = NULL;
    FILE *outStream = NULL;
    if (outCnt > 0)
    {
        outStream = tmpfile();
    }
    pid_t pid = fork();
    if (pid < 0)
    {
        return 1;
    }
    else if (pid == 0)
    {
        setpgid(0, 0);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        if (inputCnt > 0)
        {
            FILE *tempStream = tmpfile();
            if (tempStream)
            {
                char buffer[4096];
                for (int j = 0; j < inputCnt; j++)
                {
                    ssize_t bytes_read;
                    while ((bytes_read = read(input_fds[j], buffer, sizeof(buffer))) > 0)
                    {
                        fwrite(buffer, 1, bytes_read, tempStream);
                    }
                    close(input_fds[j]);
                }
                rewind(tempStream);
                dup2(fileno(tempStream), STDIN_FILENO);
                fclose(tempStream);
            }
        }
        if (outCnt > 0 && outStream)
        {
            dup2(fileno(outStream), STDOUT_FILENO);
        }
        execve(resolvedPath, argv, environ);
        exit(1);
    }
    else
    {
        setpgid(pid, pid);
        if (is_bg)
        {
            jobs[j_cnt].pid = pid;
            jobs[j_cnt].pgid = pid;
            jobs[j_cnt].jid = next_jid;
            strncpy(jobs[j_cnt].cmd, full_cmd, sizeof(jobs[j_cnt].cmd) - 1);
            jobs[j_cnt].cmd[sizeof(jobs[j_cnt].cmd) - 1] = '\0';
            jobs[j_cnt].is_bg = is_bg;
            jobs[j_cnt].active = 1;
            jobs[j_cnt].notified = 0;
            j_cnt++;
            printf("[%d] %d\n", next_jid, pid);
            fflush(stdout);
            next_jid++;
        }
        else
        {
            tcsetpgrp(STDIN_FILENO, pid);
            fg_run = 1;
            int status;
            pid_t wpid;
            do 
            {
                wpid = waitpid(pid, &status, WUNTRACED);
            } 
            while (wpid == -1 && errno == EINTR);
            fg_run = 0;
            tcsetpgrp(STDIN_FILENO, getpgrp());
            if (WIFSIGNALED(status))
            {
                printf("\n");
                fflush(stdout);
            }
            if (WIFSTOPPED(status))
            {
                jobs[j_cnt].pid = pid;
                jobs[j_cnt].pgid = pid;
                jobs[j_cnt].jid = next_jid;
                strncpy(jobs[j_cnt].cmd, full_cmd, sizeof(jobs[j_cnt].cmd) - 1);
                jobs[j_cnt].cmd[sizeof(jobs[j_cnt].cmd) - 1] = '\0';
                jobs[j_cnt].is_bg = 1;
                jobs[j_cnt].active = 1;
                jobs[j_cnt].notified = 0;
                printf("\n[%d] + Stopped    %s\n", next_jid, jobs[j_cnt].cmd);
                fflush(stdout);
                j_cnt++;
                next_jid++;
            }
            if (outCnt > 0 && outStream)
            {
                rewind(outStream);
                char buffer[4096];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), outStream)) > 0)
                {
                    for (int j = 0; j < outCnt; j++)
                    {
                        write(out_fds[j], buffer, bytes_read);
                    }
                }
            }
        }
        if (outStream)
        {
            fclose(outStream);
        }
    }
    for (int j = 0; j < inputCnt; j++)
    {
        close(input_fds[j]);
    }
    for (int j = 0; j < outCnt; j++)
    {
        close(out_fds[j]);
    }
    free(argv);
    free(cleanToks);
    free(inputFiles);
    free(outFiles);
    free(outModes);
    free(input_fds);
    free(out_fds);
    return 1;
}
int command(tok *toks, int is_bg)
{
    int pipes = 0;
    int i = 0;
    while (toks[i].type != END)
    {
        if (toks[i].type == OP_PIPE)
        {
            pipes++;
        }
        i++;
    }
    if (pipes == 0)
    {
        return single_command(toks, is_bg);
    }
    int cmds = pipes + 1;
    int (*pi)[2] = malloc(pipes * sizeof(int[2]));
    for (int j = 0; j < pipes; j++)
    {
        pipe(pi[j]);
    }
    pid_t *pids = malloc(cmds * sizeof(pid_t));
    int idx = 0;
    int pgid = 0;
    for (int cm = 0; cm < cmds; cm++)
    {
        int total_t = 0;
        while (toks[idx + total_t].type != END && toks[idx + total_t].type != OP_PIPE)
        {
            total_t++;
        }
        tok *cmdToks = malloc((total_t + 1) * sizeof(tok));
        int c = 0;
        while (toks[idx].type != END && toks[idx].type != OP_PIPE)
        {
            cmdToks[c++] = toks[idx++];
        }
        cmdToks[c].type = END;
        cmdToks[c].val = NULL;
        if (toks[idx].type == OP_PIPE)
        {
            idx++;
        }
        tok *cleanToks = malloc((c + 1) * sizeof(tok));
        char **inputFiles = malloc(c * sizeof(char *));
        char **outFiles = malloc(c * sizeof(char *));
        int *outModes = malloc(c * sizeof(int));
        int cleanArgc = 0;
        int inputCnt = 0;
        int outCnt = 0;
        int k = 0;
        while (k < c)
        {
            if (cmdToks[k].type == OP_LT && k + 1 < c && cmdToks[k + 1].type == WORD)
            {
                inputFiles[inputCnt++] = cmdToks[k + 1].val;
                k += 2;
                continue;
            }
            else if (cmdToks[k].type == OP_GT && k + 1 < c && cmdToks[k + 1].type == WORD)
            {
                outFiles[outCnt] = cmdToks[k + 1].val;
                outModes[outCnt] = 0;
                outCnt++;
                k += 2;
                continue;
            }
            else if (cmdToks[k].type == OP_GTGT && k + 1 < c && cmdToks[k + 1].type == WORD)
            {
                outFiles[outCnt] = cmdToks[k + 1].val;
                outModes[outCnt] = 1;
                outCnt++;
                k += 2;
                continue;
            }
            cleanToks[cleanArgc++] = cmdToks[k];
            k++;
        }
        cleanToks[cleanArgc].type = END;
        cleanToks[cleanArgc].val = NULL;

        pids[cm] = fork();
        if (pids[cm] == 0)
        {
            if (cm == 0)
            {
                setpgid(0, 0);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
            }
            else
            {
                setpgid(0, pgid);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_DFL);
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
            }
            if (cm > 0)
            {
                dup2(pi[cm - 1][0], STDIN_FILENO);
            }
            else if (inputCnt > 0)
            {
                FILE *tempStream = tmpfile();
                if (tempStream)
                {
                    char buffer[4096];
                    for (int j = 0; j < inputCnt; j++)
                    {
                        int infd = open(inputFiles[j], O_RDONLY);
                        if (infd < 0)
                        {
                            printf("cshell: no such file or directory\n");
                            exit(1);
                        }
                        ssize_t bytes_read;
                        while ((bytes_read = read(infd, buffer, sizeof(buffer))) > 0)
                        {
                            fwrite(buffer, 1, bytes_read, tempStream);
                        }
                        close(infd);
                    }
                    rewind(tempStream);
                    dup2(fileno(tempStream), STDIN_FILENO);
                    fclose(tempStream);
                }
            }
            if (cm < pipes)
            {
                dup2(pi[cm][1], STDOUT_FILENO);
            }
            else if (outCnt > 0)
            {
                int flags = O_WRONLY | O_CREAT;
                if (outModes[0] == 0) flags |= O_TRUNC;
                else flags |= O_APPEND;
                int outfd = open(outFiles[0], flags, 0644);
                if (outfd < 0)
                {
                    printf("cshell: unable to create file for writing\n");
                    exit(1);
                }
                dup2(outfd, STDOUT_FILENO);
                close(outfd);
            }
            for (int j = 0; j < pipes; j++)
            {
                close(pi[j][0]);
                close(pi[j][1]);
            }
            if (cleanArgc == 0) 
            {
                exit(1);
            }
            char *raw = cleanToks[0].val;
            char resolvedPath[PATH_MAX * 2];
            int found = 0;
            int force = 0;
            char *cmd = raw;
            if (raw[0] == '%') 
            { 
                force = 1; 
                cmd = raw + 1; 
            }
            struct stat s;
            if (!force && strchr(cmd, '/') != NULL)
            {
                if (stat(cmd, &s) == 0 && S_ISREG(s.st_mode) && access(cmd, X_OK) == 0)
                {
                    snprintf(resolvedPath, sizeof(resolvedPath), "%s", cmd);
                    found = 1;
                }
            }
            else if (!force)
            {
                char cwd[PATH_MAX];
                if (getcwd(cwd, sizeof(cwd)) != NULL)
                {
                    snprintf(resolvedPath, sizeof(resolvedPath), "%s/%s", cwd, cmd);
                    if (stat(resolvedPath, &s) == 0 && S_ISREG(s.st_mode) && access(resolvedPath, X_OK) == 0)
                    {
                        found = 1;
                    }
                }
            }
            if (!found && (force || strchr(cmd, '/') == NULL))
            {
                const char *pathenv = getenv("PATH");
                if (pathenv != NULL)
                {
                    char *copy = strdup(pathenv);
                    char *dir = strtok(copy, ":");
                    while (dir != NULL)
                    {
                        snprintf(resolvedPath, sizeof(resolvedPath), "%s/%s", dir, cmd);
                        if (stat(resolvedPath, &s) == 0 && S_ISREG(s.st_mode) && access(resolvedPath, X_OK) == 0)
                        {
                            found = 1;
                            break;
                        }
                        dir = strtok(NULL, ":");
                    }
                    free(copy);
                }
            }
            if (!found)
            {
                printf("cshell: command not found (%s)\n", cmd);
                exit(1);
            }

            char **argv = malloc((cleanArgc + 1) * sizeof(char *));
            argv[0] = cmd;
            for (int k = 1; k < cleanArgc; k++) argv[k] = cleanToks[k].val;
            argv[cleanArgc] = NULL;
            execve(resolvedPath, argv, environ);
            exit(1);
        }
        else 
        {
            if (cm == 0)
            {
                pgid = pids[cm];
            }
            setpgid(pids[cm], pgid);
            if (is_bg)
            {
                char pipe_cmd[1024] = "";
                for (int k = 0; k < c; k++) {
                    strcat(pipe_cmd, cmdToks[k].val);
                    if (k < c - 1) strcat(pipe_cmd, " ");
                }
                jobs[j_cnt].pid = pids[cm];
                jobs[j_cnt].pgid = pgid;
                jobs[j_cnt].jid = next_jid;
                strncpy(jobs[j_cnt].cmd, pipe_cmd, sizeof(jobs[j_cnt].cmd) - 1);
                jobs[j_cnt].cmd[sizeof(jobs[j_cnt].cmd) - 1] = '\0';
                jobs[j_cnt].is_bg = is_bg;
                jobs[j_cnt].active = 1;
                jobs[j_cnt].notified = 0;
                j_cnt++;
            }
        }
        free(cmdToks);
        free(cleanToks);
        free(inputFiles);
        free(outFiles);
        free(outModes);
    }
    for (int j = 0; j < pipes; j++)
    {
        close(pi[j][0]);
        close(pi[j][1]);
    }
    if (is_bg)
    {
        printf("[%d] %d\n", next_jid, pids[0]);
        fflush(stdout);
        next_jid++;
    }
    else
    {
        tcsetpgrp(STDIN_FILENO, pgid);
        fg_run = 1;
        int status = 0;
        int stopped = 0;
        int signaled = 0;
        for (int cm = 0; cm < cmds; cm++)
        {
            pid_t wpid;
            do 
            {
                wpid = waitpid(pids[cm], &status, WUNTRACED);
            } 
            while (wpid == -1 && errno == EINTR);
            if (WIFSIGNALED(status))
            {
                signaled = 1;
            }
            if (WIFSTOPPED(status)) 
            {
                stopped = 1;
            }
        }
        fg_run = 0;
        tcsetpgrp(STDIN_FILENO, getpgrp());
        if (signaled)
        {
            printf("\n");
            fflush(stdout);
        }
        if (stopped)
        {
            char full_pipeline[1024] = "";
            int t_idx = 0;
            while (toks[t_idx].type != END) 
            {
                if (t_idx > 0) strcat(full_pipeline, " ");
                strcat(full_pipeline, toks[t_idx].val);
                t_idx++;
            }
            jobs[j_cnt].pid = pids[0];
            jobs[j_cnt].pgid = pgid;
            jobs[j_cnt].jid = next_jid;
            strncpy(jobs[j_cnt].cmd, full_pipeline, sizeof(jobs[j_cnt].cmd) - 1);
            jobs[j_cnt].cmd[sizeof(jobs[j_cnt].cmd) - 1] = '\0';
            jobs[j_cnt].is_bg = is_bg;
            jobs[j_cnt].active = 1;
            jobs[j_cnt].notified = 0;
            printf("\n[%d] + Stopped    %s\n", next_jid, jobs[j_cnt].cmd);
            fflush(stdout);
            j_cnt++;
            next_jid++;
        }
    }
    free(pi);
    free(pids);
    return 1;
}