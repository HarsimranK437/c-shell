#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include "../include/shell.h"
int locate(tok *toks)
{
    int i = 1;
    int argCnt = 0;
    while (toks[i].type == WORD)
    {
        argCnt++;
        i++;
    }
    if (argCnt == 0)
    {
        printf("locate: invalid syntax\n");
        return 1;
    }
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        return 1;
    }
    const char *pathenv = getenv("PATH");
    for (int j = 1; j <= argCnt; j++)
    {
        char *cmd = toks[j].val;
        int found = 0;
        char check[PATH_MAX * 2];
        int ret1 = snprintf(check, sizeof(check), "%s/%s", cwd, cmd);
        if (ret1 > 0 && ret1 < (int)sizeof(check))
        {
            struct stat s;
            if (stat(check, &s) == 0 && S_ISREG(s.st_mode) && access(check, X_OK) == 0)
            {
                printf("%s\n", check);
                found = 1;
            }
        }
        if (pathenv != NULL)
        {
            char *copy = strdup(pathenv);
            if (copy != NULL)
            {
                char *dir = strtok(copy, ":");
                while (dir != NULL)
                {
                    int ret2 = snprintf(check, sizeof(check), "%s/%s", dir, cmd);
                    if (ret2 > 0 && ret2 < (int)sizeof(check))
                    {
                        struct stat s;
                        if (stat(check, &s) == 0 && S_ISREG(s.st_mode) && access(check, X_OK) == 0)
                        {
                            printf("%s\n", check);
                            found = 1;
                        }
                    }
                    dir = strtok(NULL, ":");
                }
                free(copy);
            }
        }
        if (!found)
        {
            printf("locate: command not found (%s)\n", cmd);
        }
    }
    return 1;
}