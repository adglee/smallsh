# smallsh - a custom unix shell

This is a POSIX-style Unix shell implemented in C that uses core Linux systems programming concepts - process management, job control, signal handling, and I/O redirection. 

Features:<br/>
Interactive shell prompt with command parsing<br/>
Built-in commands: cd, status, exit<br/>
Foreground and background process execution (&)<br/>
Input/output redirection using dup2()<br/>
Background process tracking and cleanup with waitpid(..., WNOHANG)<br/>
Signal handling for SIGINT and SIGTSTP<br/>
Foreground-only mode toggled via SIGTSTP<br/>
Graceful termination of background jobs on exit<br/>

To build and run file:<br/>
Bash

gcc --std=gnu99 -Wall -Wextra -o smallsh smallsh.c
./smallsh
