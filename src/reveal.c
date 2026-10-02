#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include "../include/shell.h"
int cmp(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}
void listDir(const char *real_path, int all, int rec, const char *prefix)
{
    DIR *d = opendir(real_path);
    if (!d)
    {
        return;
    }
    struct dirent *entry;
    int cap = 128;
    char **entries = malloc(cap * sizeof(char *));
    if (!entries) 
    {
        closedir(d);
        return;
    }
    int cnt = 0;
    while ((entry = readdir(d)) != NULL)
    {
        if (!all && entry->d_name[0] == '.')
        {
            continue;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        if (cnt >= cap)
        {
            cap *= 2;
            entries = realloc(entries, cap * sizeof(char *));
        }
        entries[cnt++] = strdup(entry->d_name);
    }
    closedir(d);
    if (cnt > 0)
    {
        qsort(entries, cnt, sizeof(char *), cmp);
    }
    typedef struct
    {
        char real[PATH_MAX];
        char disp[PATH_MAX];
    } subdir;
    int subCap = 128;
    subdir *subdirs = malloc(subCap * sizeof(subdir));
    if (!subdirs) 
    {
        free(entries);
        return;
    }
    int subCnt = 0;
    for (int i = 0; i < cnt; i++)
    {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", real_path, entries[i]);
        struct stat s;
        int isDir = (stat(path, &s) == 0 && S_ISDIR(s.st_mode));
        if (prefix && prefix[0] != '\0')
        {
            printf("%s/%s\n", prefix, entries[i]);
        }
        else
        {
            printf("%s\n", entries[i]);
        }
        if (rec && isDir)
        {
            if (subCnt >= subCap)
            {
                subCap *= 2;
                subdirs = realloc(subdirs, subCap * sizeof(subdir));
            }
            strcpy(subdirs[subCnt].real, path);
            if (prefix && prefix[0] != '\0')
            {
                snprintf(subdirs[subCnt].disp, PATH_MAX, "%s/%s", prefix, entries[i]);
            }
            else
            {
                snprintf(subdirs[subCnt].disp, PATH_MAX, "%s", entries[i]);
            }
            subCnt++;
        }
        free(entries[i]);
    }
    free(entries);
    for (int i = 0; i < subCnt; i++)
    {
        listDir(subdirs[i].real, all, rec, subdirs[i].disp);
    }
    free(subdirs);
}
int reveal(tok *toks, const char *home_dir)
{
    int all = 0;
    int rec = 0;
    char path_arg[PATH_MAX];
    int pathCnt = 0;
    int i = 1;
    while (toks[i].type == WORD)
    {
        char *arg = toks[i].val;
        if (arg[0] == '-' && strcmp(arg, "-") != 0)
        {
            for (int j = 1; arg[j] != '\0'; j++)
            {
                if (arg[j] == 'a')
                {
                    all = 1;
                }
                else if (arg[j] == 't')
                {
                    rec = 1;
                }
                else
                {
                    printf("reveal: invalid syntax\n");
                    return 1;
                }
            }
        }
        else
        {
            pathCnt++;
            if (pathCnt > 1)
            {
                printf("reveal: invalid syntax\n");
                return 1;
            }
            strcpy(path_arg, arg);
        }
        i++;
    }
    char tarPath[PATH_MAX];
    if (pathCnt == 0)
    {
        strcpy(tarPath, ".");
    }
    else if (strcmp(path_arg, "~") == 0)
    {
        strcpy(tarPath, home_dir);
    }
    else if (strcmp(path_arg, "-") == 0)
    {
        if (prev_dir[0] == '\0')
        {
            printf("reveal: no such directory\n");
            return 1;
        }
        strcpy(tarPath, prev_dir);
    }
    else
    {
        strcpy(tarPath, path_arg);
    }
    struct stat s;
    if (stat(tarPath, &s) != 0 || !S_ISDIR(s.st_mode))
    {
        printf("reveal: no such directory\n");
        return 1;
    }
    listDir(tarPath, all, rec, "");
    return 1;
}