# mysh – A Simple Unix Shell in C

A minimal command-line shell for Linux, built from scratch in C to learn
how operating systems create processes, run programs, and manage input/output.

## Features

- Interactive prompt that reads and runs commands
- Runs any program on the system (`ls`, `echo`, `sort`, `ps`, ...) as a child process
- Built-in commands:
  - `cd [folder]` – change the current folder (no folder = home)
  - `help` – list the built-in commands
  - `exit` – leave the shell (Ctrl+D also works)
- I/O redirection:
  - `<` – read input from a file
  - `>` – write output to a file (overwrite)
  - `>>` – append output to a file
  - Combinations such as `sort < in.txt > out.txt`
- Clear error messages for unknown commands, missing files, and syntax errors

## Example

```
mysh> echo zebra > animals.txt
mysh> echo apple >> animals.txt
mysh> sort < animals.txt
apple
zebra
mysh> cd /tmp
mysh> pwd
/tmp
mysh> blabla
mysh: No such file or directory
mysh> exit
```

## How it works

### Running programs: fork / exec / wait
For every external command, the shell uses three Linux system calls:

1. **`fork()`** – creates a child process (a copy of the shell)
2. **`execvp()`** – the child replaces itself with the requested program
3. **`waitpid()`** – the shell waits for the child to finish, then shows the next prompt

Running programs in a child keeps the shell safe: if a program fails or crashes,
only the child is affected.

### Built-in commands
`cd` and `exit` change the shell itself, so they cannot run in a child process
(a child cannot change its parent's current folder). The shell handles them
directly, without forking.

### I/O redirection: file descriptors
Every process has numbered file descriptors: `0` = input, `1` = output, `2` = errors.
Before running a program, the child process rewires them:

- `open()` opens the target file
- `dup2()` points descriptor 0 or 1 at that file
- `execvp()` starts the program, which reads and writes as usual,
  unaware that its input or output is now a file

The parent removes the redirection symbols and file names from the argument list,
so the program only receives its real arguments.

## Code structure

| Function | Purpose |
|---|---|
| `shell_loop()` | Main loop: prompt → read → run → repeat |
| `parse_line()` | Splits the input line into words |
| `find_builtin()` | Identifies built-in commands |
| `builtin_cd()`, `builtin_help()` | Built-in command implementations |
| `run_command()` | Handles redirection, then fork / exec / wait |

## Build and run

Requires Linux (or WSL on Windows) and `gcc`.

```bash
gcc -Wall -o mysh mysh.c
./mysh
```

## Roadmap

- [x] Read, parse, and run commands (fork / exec / wait)
- [x] Built-in commands: `cd`, `help`, `exit`
- [x] I/O redirection: `<`, `>`, `>>`
- [ ] Pipes (`|`)
- [ ] Signal handling (Ctrl+C) and background jobs (`&`)
- [ ] Command history

## Known limitations

- Redirection symbols must be separated by spaces (`ls > out.txt`, not `ls>out.txt`)
- Redirection applies to external programs, not built-in commands
- No quoting support yet (`echo "hello world"` is split into two words)
- Arrow keys and command history are not supported yet

## What I learned

- How the Linux kernel starts programs using `fork` and `exec`, and why the parent must `wait`
- Why commands like `cd` must be built into the shell itself
- How file descriptors work, and how shells redirect I/O with `open` and `dup2`
- Safe string handling in C: buffer limits, `strtok`, and NULL-terminated argument lists
- Structuring C code into small, single-purpose functions