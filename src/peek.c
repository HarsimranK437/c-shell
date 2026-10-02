#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../include/shell.h"
void reverse(const char* buffer, int len, int* num, int pNum)
{
    int emp = 0;
    if(len == 0)
    {
        emp = 1;
    }
    if(!emp && pNum)
    {
        printf("%d ", *num);
        (*num)--;
    }
    for (int i = len - 1; i >= 0; i--)
    {
        putchar(buffer[i]);
    }
    putchar('\n');
}
int nonEmpty(const char* fname)
{
    FILE* f = fopen(fname, "r");
    if(!f)
    {
        return 0;
    }
    char* line = NULL;
    size_t len = 0;
    int cnt = 0;
    while (getline(&line, &len, f) != -1)
    {
        int l_len = strlen(line);
        if (l_len > 0 && line[l_len-1] == '\n')
        {
            line[l_len-1] = '\0';
        }
        if (line[0] != '\0')
        {
            cnt++;
        }
    }
    free(line);
    fclose(f);
    return cnt;
}
void fwd(const char* fname, int num, int *curr)
{
    FILE* f = fopen(fname, "r");
    if(!f)
    {
        return;
    }
    char* line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1)
    {
        int l_len = strlen(line);
        if (l_len > 0 && line[l_len-1] == '\n')
        {
            line[l_len-1] = '\0';
        }
        if (line[0] != '\0')
        {
            if(num)
            {
                printf("%d ", (*curr)++);
            }
        }
        printf("%s\n", line);
    }
    free(line);
    fclose(f);
}
void rev(const char* fname, int num, int *line_counter)
{
    int total = 0;
    if(num)
    {
        total = nonEmpty(fname);
    }
    int start_line = *line_counter;
    int end_line = start_line + total - 1;
    *line_counter += total;
    int fd = open(fname, O_RDONLY);
    if (fd < 0)
    {
        return;
    }
    off_t fsize = lseek(fd, 0, SEEK_END);
    if(fsize == 0)
    {
        close(fd);
        return;
    }
    int maxi = 1048576;
    char* lineBuffer = malloc(maxi);
    if(!lineBuffer)
    {
        close(fd);
        return;
    }
    int l_len = 0;
    char lastChar = '\0';
    lseek(fd, fsize - 1, SEEK_SET);
    read(fd, &lastChar, 1);
    int skip = 0;
    if(lastChar == '\n')
    {
        skip = 1;
    }
    off_t pos = fsize;
    int chunkSize = 4096;
    char* chunk = malloc(chunkSize);
    while (pos > 0)
    {
        off_t to_read = pos;
        if(pos > chunkSize)
        {
            to_read = chunkSize;
        }
        pos -= to_read;
        lseek(fd, pos, SEEK_SET);
        read(fd, chunk, to_read);
        for (off_t i = to_read - 1; i >= 0; i--)
        {
            if (chunk[i] == '\n')
            {
                if(skip)
                {
                    skip = 0;
                    continue;
                }
                int pNum = num;
                reverse(lineBuffer, l_len, &end_line, pNum);
                l_len = 0;
            }
            else
            {
                skip = 0;
                if (l_len < maxi - 1)
                {
                    lineBuffer[l_len++] = chunk[i];
                }
            }
        }
    }
    reverse(lineBuffer, l_len, &end_line, num);
    free(chunk);
    free(lineBuffer);
    close(fd);
}
void handle(int num, int revLines)
{
    if(!revLines)
    {
        char* line = NULL;
        size_t len = 0;
        int curr = 1;
        while (getline(&line, &len, stdin) != -1)
        {
            int l_len = strlen(line);
            if (l_len > 0 && line[l_len-1] == '\n')
            {
                line[l_len-1] = '\0';
            }
            if (line[0] != '\0')
            {
                if (num)
                {
                    printf("%d ", curr++);
                }
                printf("%s\n", line);
            }
        }
        free(line);
    }
    else
    {
        char** lines = NULL;
        int cap = 100;
        int cnt = 0;
        lines = malloc(cap * sizeof(char*));
        char* line = NULL;
        size_t len = 0;
        int curr = 0;
        while (getline(&line, &len, stdin) != -1)
        {
            if(cnt >= cap)
            {
                cap *= 2;
                lines = realloc(lines, cap * sizeof(char*));
            }
            int l_len = strlen(line);
            if (l_len > 0 && line[l_len-1] == '\n')
            {
                line[l_len-1] = '\0';
            }
            if (line[0] != '\0')
            {
                curr++;
            }
            lines[cnt++] = strdup(line);
        }
        free(line);
        for (int i = cnt - 1; i >= 0; i--)
        {
            if (strlen(lines[i]) > 0)
            {
                if(num)
                {
                    printf("%d ", curr--);
                }
            }
            printf("%s\n", lines[i]);
            free(lines[i]);
        }
        free(lines);
    }
}
int peek(tok* toks)
{
    int num = 0;
    int revLines = 0;
    int total_t = 0;
    while (toks[total_t].type != END)
    {
        total_t++;
    }
    const char** files = malloc((total_t + 1) * sizeof(const char*));
    if (!files)
    {
        return 1;
    }
    int fcnt = 0;
    int i = 1;
    while (toks[i].type == WORD)
    {
        char* arg = toks[i].val;
        if (arg[0] == '-' && arg[1] != '\0')
        {
            int valid = 1;
            for (int j = 1; arg[j] != '\0'; j++)
            {
                if (arg[j] != 'n' && arg[j] != 'r')
                {
                    valid = 0;
                    break;
                }
            }
            if(valid)
            {
                for (int j = 1; arg[j] != '\0'; j++)
                {
                    if (arg[j] == 'n')
                    {
                        num = 1;
                    }
                    if (arg[j] == 'r')
                    {
                        revLines = 1;
                    }
                }
            }
            else
            {
                printf("peek: invalid syntax\n");
                free(files);
                return 1;
            }
        }
        else
        {
            files[fcnt++] = arg;
        }
        i++;
    }
    if(fcnt == 0)
    {
        files[0] = "-";
        fcnt = 1;
    }
    int line_counter = 1;
    for (int k = 0; k < fcnt; k++)
    {
        const char* fname = files[k];
        if (strcmp(fname, "-") == 0)
        {
            handle(num, revLines);
        }
        else
        {
            struct stat s;
            if (stat(fname, &s) != 0)
            {
                printf("peek: no such file or directory\n");
                continue;
            }
            if (S_ISDIR(s.st_mode))
            {
                printf("peek: is a directory\n");
                continue;
            }
            if(revLines)
            {
                rev(fname, num, &line_counter);
            }
            else
            {
                fwd(fname, num, &line_counter);
            }
        }
    }
    free(files);
    return 1;
}