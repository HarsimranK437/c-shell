#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include "../include/shell.h"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
char prev_dir[PATH_MAX] = "";
void frec(const char* tar, const char* home_dir)
{
    char db_path[PATH_MAX];
    snprintf(db_path, sizeof(db_path), "%s/.hop_track", home_dir);
    FILE* f = fopen(db_path, "r");
    int cap = 64;
    char** paths = malloc(cap * sizeof(char*));
    int* scores = malloc(cap * sizeof(int));
    int cnt = 0;
    int found = 0;
    if(f)
    {
        char temp_path[PATH_MAX];
        int tmp_score;
        while(fscanf(f, "%d %4095[^\n]", &tmp_score, temp_path) != EOF)
        {
            if (cnt >= cap)
            {
                cap *= 2;
                paths = realloc(paths, cap * sizeof(char*));
                scores = realloc(scores, cap * sizeof(int));
            }
            scores[cnt] = tmp_score;
            paths[cnt] = strdup(temp_path);
            if(strcmp(paths[cnt], tar) == 0)
            {
                scores[cnt]++;
                found = 1;
            }
            cnt++;
        }
        fclose(f);
    }
    if(!found)
    {
        if (cnt >= cap)
        {
            cap *= 2;
            paths = realloc(paths, cap * sizeof(char*));
            scores = realloc(scores, cap * sizeof(int));
        }
        paths[cnt] = strdup(tar); 
        scores[cnt] = 1;
        cnt++;
    }
    f = fopen(db_path, "w");
    if(f)
    {
        for(int i = 0; i < cnt; i++)
        {
            fprintf(f, "%d %s\n", scores[i], paths[i]);
            free(paths[i]);
        }
        fclose(f);
    }
    else
    {
        for(int i = 0; i < cnt; i++)
        {
            free(paths[i]);
        }
    }
    free(paths);
    free(scores);
}
int findfrec(const char* name, char* res, const char* home_dir)
{
    char db_path[PATH_MAX];
    snprintf(db_path, sizeof(db_path), "%s/.hop_track", home_dir);
    FILE* f = fopen(db_path, "r");
    if(!f)
    {
        return 0;
    }
    int maxi = -1;
    char temp_path[PATH_MAX];
    int tmp;
    int matched = 0;
    while (fscanf(f, "%d %4095[^\n]", &tmp, temp_path) != EOF)
    {
        if(strstr(temp_path, name) != NULL)
        {
            struct stat s;
            if(stat(temp_path, &s) == 0 && S_ISDIR(s.st_mode))
            {
                if(tmp > maxi)
                {
                    maxi = tmp;
                    strcpy(res, temp_path);
                    matched = 1;
                }
            }
        }
    }
    fclose(f);
    return matched;
}
void doHop(const char* arg, const char* home_dir)
{
    if (strcmp(arg, ".") == 0)
    {
        return;
    }
    char curr[PATH_MAX];
    if(getcwd(curr, sizeof(curr)) == NULL)
    {
        return;
    }
    char tar[PATH_MAX];
    if(strcmp(arg, "~") == 0)
    {
        strcpy(tar, home_dir);
    }
    else if (strcmp(arg, "-") == 0)
    {
        if (prev_dir[0] == '\0')
        {
            return;
        }
        strcpy(tar, prev_dir);
    }
    else
    {
        strcpy(tar, arg);
    }
    if(chdir(tar) == 0)
    {
        strcpy(prev_dir, curr);
        char newd[PATH_MAX];
        if(getcwd(newd, sizeof(newd)) != NULL)
        {
            frec(newd, home_dir);
        }
        return;
    }
    char match[PATH_MAX];
    if (findfrec(arg, match, home_dir))
    {
        if(chdir(match) == 0)
        {
            strcpy(prev_dir, curr);
            char newd[PATH_MAX];
            if(getcwd(newd, sizeof(newd)) != NULL)
            {
                frec(newd, home_dir);
            }
            return;
        }
    }
    printf("hop: no such directory\n");
}
int hop(tok* toks, const char* home_dir)
{
    int i = 1;
    int args = 0;
    while (toks[i].type == WORD)
    {
        doHop(toks[i].val, home_dir);
        args = 1;
        i++;
    }
    if(!args)
    {
        doHop("~", home_dir);
    }
    return 1;
}