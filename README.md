# mysh – A Simple Unix Shell in C

A minimal command-line shell for Linux, built from scratch in C to learn
how operating systems create processes, run programs, and manage input/output.

## Features

- Interactive prompt that reads and runs commands
- Runs any program on the system (`ls`, `grep`, `sort`, ...) as a child process
- Built-in commands:
  - `cd [folder]` – change the current folder (no folder = home)
  - `help` – list the built-in commands
  - `exit` – leave the shell (Ctrl+D also works)
- I/O redirection:
  - `<` – read input from a file
  - `>` – write output to a file (overwrite)
  - `>>` – append output to a file
- Pipelines with any number of commands: `ls -la | grep txt | sort -r`
- Pipes and redirection combined: `cat < in.txt | grep word | wc -l > count.txt`
- Clear error messages for unknown commands, missing files, and syntax errors

## Example

```
mysh> ls -la | grep mysh | sort -r
-rwxr-xr-x 1 user user 17216 Nov 25 14:14 mysh
-rw-r--r-- 1 user user  9263 Nov 25 14:11 mysh.c
mysh> cat < mysh.c | grep #include | wc -l
7
mysh> sleep 3 | echo done
done
mysh> ls |
mysh: syntax error near '|'
mysh> exit
```

## How it works

### Running programs: fork / exec / wait
For every external command, the shell uses three Linux system calls:

1. **`fork()`** – creates a child process (a copy of the shell)
2. **`execvp()`** – the child replaces itself with the requested program
3. **`waitpid()`** – the shell waits for the child to finish

Running programs in a child keeps the shell safe: if a program fails,
only the child is affected.

### Built-in commands
`cd` and `exit` change the shell itself, so they cannot run in a child
process. The shell handles them directly, without forking.

### I/O redirection: file descriptors
Every process has numbered file descriptors: `0` = input, `1` = output,
`2` = errors. Before running a program, the child rewires them with
`open()` and `dup2()`. The program reads and writes as usual, unaware
that its input or output is now a file.

### Pipelines
The line is split into sections at each `|`. Each section runs in its own
child, and all children run **at the same time**:

- `pipe()` creates a one-way channel with a read end and a write end
- Each child copies the pipe ends it needs onto fd 0 / fd 1 with `dup2()`
- Pipes are created one at a time: the shell only carries the read end
  of the previous pipe forward to the next child
- Every process closes the pipe ends it does not use. Otherwise the
  reader never receives end-of-file, and the pipeline hangs
- After starting all children, the shell waits for every one of them

File redirections are applied after the pipes, so `>` overrides a pipe,
as in bash.

## Code structure

| Function | Purpose |
|---|---|
| `shell_loop()` | Main loop: prompt → read → run → repeat |
| `parse_line()` | Splits the input line into words |
| `find_builtin()` | Identifies built-in commands |
| `builtin_cd()`, `builtin_help()` | Built-in command implementations |
| `split_pipeline()` | Splits the words into sections at each `\|` |
| `run_command()` | Creates the pipes and children for each section, then waits |
| `apply_redirections()` | Runs in each child: handles `<`, `>`, `>>` |

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
- [x] Pipelines with any number of commands
- [ ] Signal handling (Ctrl+C) and background jobs (`&`)
- [ ] Command history

## Known limitations

- `|`, `<`, `>`, `>>` must be separated by spaces (`ls | wc`, not `ls|wc`)
- Built-in commands do not work inside pipelines or with redirection
- No quoting support yet (`echo "hello world"` is split into two words)
- Arrow keys and command history are not supported yet

## What I learned

- How the Linux kernel starts programs using `fork` and `exec`, and why the parent must `wait`
- Why commands like `cd` must be built into the shell itself
- How file descriptors work, and how shells redirect I/O with `open` and `dup2`
- How pipes connect processes, and why every unused pipe end must be closed
- Running several processes at the same time and waiting for all of them
- Safe string handling in C: buffer limits, `strtok`, and NULL-terminated lists
- Structuring C code into small, single-purpose functions