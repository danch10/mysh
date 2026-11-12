# mysh – A Simple Unix Shell in C

A minimal command-line shell for Linux, built from scratch in C to learn
how operating systems create and manage processes.

## What it does

- Displays a prompt and reads commands from the user
- Splits each command into a program name and its arguments
- Runs programs (`ls`, `echo`, `ps`, ...) as new processes
- Reports errors for unknown commands
- - Built-in commands: `cd` (with home-folder default), `help`, and `exit`; exits cleanly on Ctrl+D
## How it works

For every command, the shell uses three Linux system calls:

1. **`fork()`** – creates a child process (a copy of the shell)
2. **`execvp()`** – the child replaces itself with the requested program
3. **`waitpid()`** – the shell waits for the child to finish before showing the next prompt

## Build and run

Requires Linux (or WSL on Windows) and `gcc`.

```bash
gcc -Wall -o mysh mysh.c
./mysh
```

Example:

```
mysh> echo hello from my shell
hello from my shell
mysh> exit
```

## Roadmap

- [x] Read, parse, and run commands (fork / exec / wait)
- [x] Built-in commands: `cd`, `help`
- [ ] Output redirection (`>`)
- [ ] Pipes (`|`)
- [ ] Signal handling and background jobs

## What I learned

- How the Linux kernel starts programs using `fork` and `exec`
- Why commands like `cd` must be built into the shell itself
- Safe string handling in C (buffer limits, `strtok`, NULL-terminated argument lists)