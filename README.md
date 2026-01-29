# smallsh - a custom unix shell

This is a POSIX-style Unix shell implemented in C that uses core Linux systems programming concepts - process management, job control, signal handling, and I/O redirection. 

Features:<br/>
Interactive shell prompt with command parsing
Built-in commands: cd, status, exit
Foreground and background process execution (&)
Input/output redirection using dup2()
Background process tracking and cleanup with waitpid(..., WNOHANG)
Signal handling for SIGINT and SIGTSTP
Foreground-only mode toggled via SIGTSTP
Graceful termination of background jobs on exit

To build and run file:<br/>
Bash

gcc --std=gnu99 -Wall -Wextra -o smallsh smallsh.c
./smallsh
