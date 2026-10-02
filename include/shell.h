#ifndef SHELL_H
#define SHELL_H
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
typedef enum
{
    END = 0,
    WORD,
    OP_PIPE,
    OP_AMP,
    OP_SEMI,
    OP_LT,
    OP_GT,
    OP_GTGT
} tokType;
typedef struct
{
    tokType type;
    char* val;
} tok;
typedef struct
{
    int pid;
    int pgid;
    int jid;
    char cmd[1024];
    int is_bg;
    int active;
    int notified;
} p_info;
extern p_info jobs[1024];
extern int j_cnt;
extern int next_jid;
extern int fg_run; 
extern char def_prt[4096];
extern char prev_dir[PATH_MAX];
int lex(char* line, tok* toks, int max_t);
int parse(tok* toks);
int hop(tok* toks, const char* home_dir);
int reveal(tok* toks, const char* home_dir);
int peek(tok* toks);
int locate(tok* toks);
int activities(tok* toks);
int command(tok* toks, int is_bg);
int resume(tok *toks);
int ping(tok *toks);
int spy(tok *toks);
int snoop(tok *toks);
#endif