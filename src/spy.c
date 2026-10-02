#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include "../include/shell.h"
static const char* get_type_str(mode_t mode)
{
    if (S_ISREG(mode))
    {
        return "REG";
    }
    if (S_ISDIR(mode))
    {
        return "DIR";
    }
    if (S_ISCHR(mode))
    {
        return "CHR";
    }
    if (S_ISBLK(mode))
    {
        return "BLK";
    }
    if (S_ISFIFO(mode))
    {
        return "FIFO";
    }
    if (S_ISLNK(mode))
    {
        return "LNK";
    }
    if (S_ISSOCK(mode))
    {
        return "SOCK";
    }
    return "REG";
}
int spy(tok *toks)
{
    pid_t target_pid;
    if (toks[1].type == END)
    {
        target_pid = getpid();
    }
    else if (toks[1].type == WORD && toks[2].type == END)
    {
        char *pid_str = toks[1].val;
        for (int i = 0; pid_str[i] != '\0'; i++)
        {
            if (pid_str[i] < '0' || pid_str[i] > '9')
            {
                printf("spy: no such process\n");
                return 1;
            }
        }
        target_pid = atoi(pid_str);
    }
    else
    {
        printf("spy: invalid syntax\n");
        return 1;
    }
    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d", (int)target_pid);
    struct stat st;
    if (stat(proc_path, &st) < 0)
    {
        printf("spy: no such process\n");
        return 1;
    }
    printf("PID    FD    TYPE   PATH\n");
    char link_path[512];
    char target_buf[512];
    ssize_t len;
    snprintf(link_path, sizeof(link_path), "/proc/%d/cwd", (int)target_pid);
    len = readlink(link_path, target_buf, sizeof(target_buf) - 1);
    if (len != -1)
    {
        target_buf[len] = '\0';
        if (stat(target_buf, &st) == 0)
        {
            printf("%d    cwd    %s    %s\n", (int)target_pid, get_type_str(st.st_mode), target_buf);
        }
        else
        {
            printf("%d    cwd    DIR    %s\n", (int)target_pid, target_buf);
        }
    }
    snprintf(link_path, sizeof(link_path), "/proc/%d/exe", (int)target_pid);
    len = readlink(link_path, target_buf, sizeof(target_buf) - 1);
    if (len != -1)
    {
        target_buf[len] = '\0';
        if (stat(target_buf, &st) == 0)
        {
            printf("%d    txt    %s    %s\n", (int)target_pid, get_type_str(st.st_mode), target_buf);
        }
        else
        {
            printf("%d    txt    REG    %s\n", (int)target_pid, target_buf);
        }
    }
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", (int)target_pid);
    FILE *fmaps = fopen(maps_path, "r");
    if (fmaps)
    {
        char unique_paths[128][512];
        int unique_count = 0;
        char line[1024];
        while (fgets(line, sizeof(line), fmaps))
        {
            char *path_start = NULL;
            int spaces = 0;
            for (int i = 0; line[i] != '\0'; i++)
            {
                if (line[i] == ' ' || line[i] == '\t')
                {
                    while (line[i+1] == ' ' || line[i+1] == '\t')
                    {
                        i++;
                    }
                    spaces++;
                    if (spaces == 5)
                    {
                        path_start = &line[i+1];
                        size_t slen = strlen(path_start);
                        if (slen > 0 && path_start[slen-1] == '\n')
                        {
                            path_start[slen-1] = '\0';
                        }
                        break;
                    }
                }
            }
            if (path_start && path_start[0] == '/')
            {
                int exists = 0;
                for (int i = 0; i < unique_count; i++)
                {
                    if (strcmp(unique_paths[i], path_start) == 0)
                    {
                        exists = 1;
                        break;
                    }
                }
                if (!exists && unique_count < 128)
                {
                    strcpy(unique_paths[unique_count], path_start);
                    unique_count++;
                    if (stat(path_start, &st) == 0)
                    {
                        printf("%d    mem    %s    %s\n", (int)target_pid, get_type_str(st.st_mode), path_start);
                    }
                    else
                    {
                        printf("%d    mem    REG    %s\n", (int)target_pid, path_start);
                    }
                }
            }
        }
        fclose(fmaps);
    }
    char fd_dir_path[256];
    snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%d/fd", (int)target_pid);
    DIR *dir = opendir(fd_dir_path);
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (entry->d_name[0] == '.')
            {
                continue;
            }
            char fd_link[512];
            snprintf(fd_link, sizeof(fd_link), "/proc/%d/fd/%s", (int)target_pid, entry->d_name);
            ssize_t rlen = readlink(fd_link, target_buf, sizeof(target_buf) - 1);
            if (rlen != -1)
            {
                target_buf[rlen] = '\0';
                if (stat(target_buf, &st) == 0)
                {
                    printf("%d    %-3s    %s    %s\n", (int)target_pid, entry->d_name, get_type_str(st.st_mode), target_buf);
                }
                else if (stat(fd_link, &st) == 0)
                {
                    printf("%d    %-3s    %s    %s\n", (int)target_pid, entry->d_name, get_type_str(st.st_mode), target_buf);
                }
                else
                {
                    printf("%d    %-3s    REG    %s\n", (int)target_pid, entry->d_name, target_buf);
                }
            }
        }
        closedir(dir);
    }
    return 1;
}