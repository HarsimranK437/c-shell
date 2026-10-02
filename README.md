# C-Shell

## Overview

**C-Shell** is a modular, feature-rich, and POSIX-compliant Unix command shell built entirely from scratch in C. Designed as an educational and high-performance terminal environment, it bridges standard user-level shell interaction with low-level process management, custom lexical analysis, and operating system diagnostics. It provides robust support for multi-stage pipelines, I/O redirection, background job scheduling, terminal foreground control, persistent directory navigation, and kernel-level process tracing.

## 1. Repository Folder Structure

```text

c-shell/
├── include/
│   └── shell.h        # Shared shell definitions, token types, and global structures
├── src/
│   ├── main.c         # Main REPL loop, signal handling, and job notification management
│   ├── lexer.c        # Command-line lexer and syntax parser
│   ├── command.c      # Execution engine for single commands and complex pipelines
│   ├── hop.c          # Directory navigation with persistent frecency tracking
│   ├── reveal.c       # Directory listing with formatting and recursive flags
│   ├── peek.c         # File content viewing with line filtering and reverse chunking
│   ├── locate.c       # Executable path lookup utility
│   ├── activities.c   # Active background and stopped jobs management
│   ├── resume.c       # Job control (fg/bg resumption and timeouts)
│   ├── ping.c         # Signal management to tasks/jobs
│   ├── spy.c          # File descriptor and resource inspection via /proc
│   └── snoop.c        # System call tracing via ptrace statistics and profiling
├── Makefile           # Build configuration for C-Shell
└── README.md          # Project documentation and instructions
```

## 2. How to Run the Code

### C-Shell

Build the shell binary:
```bash
make all
```
Run the shell:
```bash
./shell.out
```
To clean compiled artifacts:
```bash
make clean
```

## 3. Design Choices

### C-Shell Design Choices
* **Robust Tokenization & Grammar Parsing:** The lexer parses command strings handling single quotes (`'`), double quotes (`"`) and escape sequences (`\`), merging contiguous fragment tokens into clean single `WORD` tokens. A strict state-machine parser verifies syntax correctness for pipes, redirections, and separators.
* **Persistent Frecency Tracking (`hop`):** The `hop` command records directory access frequencies and recency patterns inside a persistent file (`~/.hop_track`), allowing intelligent partial string matching for directory navigation.
* **Asynchronous Job Reaping:** Background processes (`&`) are reaped asynchronously via a `SIGCHLD` signal handler, with completion notifications safely deferred and printed right before drawing the next prompt to avoid terminal corruption.
* **System Call Profiling (`snoop`):** Implements dynamic process tracing using `ptrace` (`PTRACE_TRACEME`, `PTRACE_ATTACH`, `PTRACE_SYSCALL`) combined with user-space register parsing and high-resolution timing (`clock_gettime`) to aggregate and sort executed system calls.


## 4. Assumptions & Limits

### C-Shell Assumptions & Limits
* **Maximum Input Length:** Command line buffers are allocated to safely accommodate command lines up to 4096 characters.
* **Fallback System Limits (`main.c`):** The shell assumes standard POSIX limits. If not defined by the system environment, `PATH_MAX` is strictly clamped to 4096 characters and `HOST_NAME_MAX` to 255 characters.
* **Pipeline Capacity:** Complex pipelines (`cmd1 | cmd2 | ...`) support an arbitrary number of piped commands, bounded only by system file descriptor limits and process table allocations.
* **Redirection & Fixed I/O Buffering (`command.c`):** Supports standard input (`<`), truncation output (`>`), and append output (`>>`) redirections. Intermediate data transfers utilizing `tmpfile()` are processed using fixed 4096-byte buffers. Advanced shell operators (such as file descriptor duplication like `2>&1`) are treated as standard literal arguments per project constraints.
* **Quoting and Syntax Errors (`lexer.c`):** If a user provides an unclosed single (`'`) or double (`"`) quote, the lexer will immediately fail and throw an "invalid syntax" error rather than prompting the user for secondary or continued input.
* **Ignored Directories in `reveal` (`reveal.c`):** The `.` (current) and `..` (parent) directories are strictly hard-filtered and will never be printed in the output, even when the `-a` (show hidden) flag is provided.
* **Dynamic Directory Tracking & `hop -` (`hop.c`):** The historical frecency tracking (saved in `~/.hop_track`) dynamically reallocates memory (starting at 64 entries) and is bounded only by available heap space, rather than a hard cap. Additionally, if `hop -` is invoked immediately upon starting the shell (before any directory changes occur), it will silently return without executing a change.
* **Memory Constraints for Reverse `peek` (`peek.c`):** When reading a file in reverse using the `-r` flag, the algorithm reads chunks of 4096 bytes from the end of the file. To prevent memory exhaustion, a strict hard cap of 1MB (1,048,576 bytes) is enforced for the maximum length of a single reversed line.
* **Exhaustive Path Searching (`locate.c`):** The `locate` command evaluates the current working directory first, followed by the system `PATH`. It does not break execution after the first match; instead, it prints every valid executable match it finds across all paths.
* **System Call Lookup Table (`snoop`/`spy`):** The system call name lookup table explicitly recognizes standard x86-64 system call numbers, including: `read (0)`, `write (1)`, `open (2)`, `close (3)`, `lseek (8)`, `mmap (9)`, `mprotect (10)`, `munmap (11)`, `brk (12)`, `access (21)`, `nanosleep (35/230)`, `getpid (39)`, `fork (57)`, `execve (59)`, `exit (60)`, `gettimeofday (96)`, `clock_gettime (228)`, `exit_group (231)`, `wait4 (247)`, and `openat (257)`.
