#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/shell.h"
typedef enum
{
    S_LINE,
    S_ARG,
    S_CMD,
    S_TGT,
    S_BG
} state;
int lex(char *line, tok *toks, int max_t)
{
    int t_cnt = 0;
    int i = 0;
    while (line[i] != '\0')
    {
        while (line[i] == ' ' || line[i] == '\t' || line[i] == '\n' || line[i] == '\r')
        {
            i++;
        }
        if (line[i] == '\0' || t_cnt >= max_t - 1)
        {
            break;
        }
        if (line[i] == '>' && line[i + 1] == '>')
        {
            toks[t_cnt++] = (tok){OP_GTGT, strdup(">>")};
            i += 2;
            continue;
        }
        if (line[i] == '|')
        {
            toks[t_cnt++] = (tok){OP_PIPE, strdup("|")};
            i += 1;
            continue;
        }
        if (line[i] == '&')
        {
            toks[t_cnt++] = (tok){OP_AMP, strdup("&")};
            i += 1;
            continue;
        }
        if (line[i] == ';')
        {
            toks[t_cnt++] = (tok){OP_SEMI, strdup(";")};
            i += 1;
            continue;
        }
        if (line[i] == '<')
        {
            toks[t_cnt++] = (tok){OP_LT, strdup("<")};
            i += 1;
            continue;
        }
        if (line[i] == '>')
        {
            toks[t_cnt++] = (tok){OP_GT, strdup(">")};
            i += 1;
            continue;
        }
        char *buffer = malloc(strlen(line) + 1);
        if (!buffer)
        {
            return -1;
        }
        int idx = 0;
        int quote = 0;
        while (line[i] != '\0')
        {
            if (!quote && (line[i] == ' ' || line[i] == '\t' || line[i] == '\n' || line[i] == '\r' || line[i] == '|' || line[i] == '&' || line[i] == '<' || line[i] == '>' || line[i] == ';'))
            {
                break;
            }
            if (line[i] == '\\')
            {
                if (line[i + 1] == '\0')
                {
                    free(buffer);
                    return -1;
                }
                if (quote == '\'')
                {
                    buffer[idx++] = line[i];
                }
                else if (quote == '"')
                {
                    if (line[i + 1] == '"' || line[i + 1] == '\\')
                    {
                        buffer[idx++] = line[i + 1];
                        i++;
                    }
                    else
                    {
                        buffer[idx++] = line[i];
                    }
                }
                else
                {
                    buffer[idx++] = line[++i];
                }
            }
            else if (line[i] == '"' || line[i] == '\'')
            {
                if (quote == 0)
                {
                    quote = line[i];
                }
                else if (quote == line[i])
                {
                    quote = 0;
                }
                else
                {
                    buffer[idx++] = line[i];
                }
            }
            else
            {
                buffer[idx++] = line[i];
            }
            i++;
        }
        if(quote != 0)
        {
            free(buffer);
            return -1;
        }
        buffer[idx] = '\0';
        toks[t_cnt++] = (tok){WORD, strdup(buffer)};
        free(buffer);
    }
    toks[t_cnt] = (tok){END, NULL};
    return t_cnt;
}
int parse(tok* toks)
{
    state st = S_LINE;
    int i = 0;
    while(toks[i].type != END)
    {
        tokType t = toks[i].type;
        if(st == S_LINE)
        {
            if(t == WORD)
            {
                st = S_ARG;
            }
            else
            {
                return 0;
            }
        }
        else if(st == S_ARG)
        {
            if(t == WORD)
            {
                st = S_ARG;
            }
            else if(t == OP_PIPE)
            {
                st = S_CMD;
            }
            else if(t == OP_LT || t == OP_GT || t == OP_GTGT)
            {
                st = S_TGT;
            }
            else if(t == OP_SEMI)
            {
                st = S_CMD;
            }
            else if(t == OP_AMP)
            {
                st = S_BG;
            }
            else
            {
                return 0;
            }
        }
        else if(st == S_CMD || st == S_TGT || st == S_BG)
        {
            if(t == WORD)
            {
                st = S_ARG;
            }
            else
            {
                return 0;
            }
        }
        i++;
    }
    if(st == S_CMD || st == S_TGT)
    {
        return 0;
    }
    return 1;
}