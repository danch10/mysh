/*
 * mysh.c - A simple Unix shell, written for learning purposes.
 */

//----------------------includes-------------------//
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

//----------------------constants-------------------//

#define MAX_LINE 1024 /* Maximum length of one input line, in characters.*/
#define MAX_ARGS 64   /* Maximum number of words in one command */

//----------------------enum------------------------//

enum builtin
{
    BI_NONE,
    BI_EXIT,
    BI_CD,
    BI_HELP
};

//----------------------functions-------------------//

int shell_loop(void);
enum builtin find_builtin(const char *name);
int parse_line(char *line, char **args);
void builtin_cd(char **args);
void builtin_help(void);
void run_command(char **args);
void apply_redirections(char **args);
int split_pipeline(char **args, char **cmds[]);

//------------------------------main-------------------------//

/* Program entry point. Every C program starts running here.
 * Returns an exit code to the operating system: 0 = success. */
int main(void)
{
    return shell_loop();
}
//-----------------------------------------------------------//

int shell_loop(void)
{

    char line[MAX_LINE];  /* Memory reserved for the line the user types (up to 1024 chars). */
    char *args[MAX_ARGS]; /* An array of pointers. Each element will point to the
                           * start of one word inside 'line'. No text is copied. */

    /* Main shell loop: prompt -> read -> respond -> repeat.
     * while (1) is an infinite loop; we leave it with 'break'. */
    while (1)
    {

        /* Show the prompt so the user knows the shell is ready. */
        printf("mysh> ");

        /* Output is buffered: it is normally sent to the screen only
         * when a newline appears. The prompt has no newline, so we
         * force it out immediately. */
        fflush(stdout);

        /* Read one line from standard input into 'line'.
         * sizeof(line) limits how much we read, so a very long line
         * can never overflow our memory (an important safety habit in C).
         * fgets returns NULL when there is no more input: that happens
         * when the user presses Ctrl+D ("end of input" in Linux). */
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        /* If the user just pressed Enter, there are no words. Show a new prompt. */
        if (parse_line(line, args) == 0)
        {
            continue; /* skip the rest of the loop and start again */
        }

        switch (find_builtin(args[0]))
        {
        case BI_EXIT:
            return 0;
        case BI_CD:
            builtin_cd(args);
            continue;
        case BI_HELP:
            builtin_help();
            continue;
        case BI_NONE:
            break;
        }

        run_command(args);
    }
    return 0;
}

//-----------------------------------------------------------//

int parse_line(char *line, char **args)
{

    int argc = 0; /* how many words we have found so far */
    /* strtok finds the first word. The separators are:
     * space ' ', tab '\t', and newline '\n'.
     * Including '\n' also gets rid of the Enter key at the end. */
    char *token = strtok(line, " \t\n");

    /* Keep collecting words until there are none left.
     * We stop at MAX_ARGS - 1 to always leave room for the NULL. */
    while (token != NULL && argc < MAX_ARGS - 1)
    {
        args[argc] = token; /* store a pointer to this word */
        argc++;
        token = strtok(NULL, " \t\n"); /* NULL = "continue the same line" */
    }
    /* Mark the end of the list. Programs that receive this list
     * (like execvp in the next stage) need it to know where it ends. */
    args[argc] = NULL;

    return argc;
}

//-----------------------------------------------------------//

enum builtin find_builtin(const char *name)
{
    if (strcmp(name, "exit") == 0)
        return BI_EXIT;
    if (strcmp(name, "cd") == 0)
        return BI_CD;
    if (strcmp(name, "help") == 0)
        return BI_HELP;
    return BI_NONE;
}

//-----------------------------------------------------------//

void builtin_cd(char **args)
{
    char *target = args[1];
    if (target == NULL)
    {
        target = getenv("HOME");
    }

    if (target == NULL)
    {
        fprintf(stderr, "mysh: cd: HOME not set\n");
    }
    else if (chdir(target) != 0)
    {
        perror("mysh: cd");
    }
}

//-----------------------------------------------------------//

void builtin_help(void)
{
    printf("mysh - a simple Unix shell\n");
    printf("Built-in commands:\n");
    printf("  cd [folder]   change folder (no folder = home)\n");
    printf("  help          show this message\n");
    printf("  exit          leave the shell (or press Ctrl+D)\n");
    printf("Any other command runs as a program, e.g. ls -la\n");
}

//-----------------------------------------------------------//

/*
 * run_command - run one external command, or a pipeline of commands.
 *
 * Examples:
 *   "ls -la"                 -> 1 section, no pipes
 *   "ls | grep txt"          -> 2 sections, 1 pipe
 *   "a < in | b | c > out"   -> 3 sections, 2 pipes, plus file redirections
 *
 * How it works:
 *   1. split_pipeline() cuts args into sections at each "|".
 *   2. Each section runs in its own child process. All children run
 *      at the same time, connected by pipes.
 *   3. Pipes are created one at a time ("rolling"): the parent only
 *      carries the read end of the previous pipe (prev_read) to the
 *      next child.
 *   4. After all children are started, the parent waits for each one.
 *
 * File descriptor rules:
 *   - A child copies the pipe ends it needs onto fd 0 / fd 1 with
 *     dup2(), then closes every original pipe descriptor.
 *   - The parent closes its copies right after each fork. If any write
 *     end stayed open, the next reader would never see end-of-file,
 *     and the shell would hang.
 */
void run_command(char **args)
{
    char **commands[MAX_ARGS];  /* start of each section's word list */
    pid_t pids[MAX_ARGS];       /* PID of each child, for waiting */

    /* Split at "|". Returns the number of sections, or -1 on a syntax
     * error (already reported), e.g. "ls |" or "| ls". */
    int num_of_commands = split_pipeline(args, commands);
    if (num_of_commands < 0)
    {
        return;
    }

    int prev_read = -1;         /* read end of the previous pipe; -1 = none */
    int started = 0;            /* CHANGE: children actually created */

    for (int i = 0; i < num_of_commands; i++)
    {
        int fds[2];             /* new pipe: fds[0] = read end, fds[1] = write end */

        /* The last section writes to the screen (or its own "> file"),
         * so it is the only one that needs no pipe after it. */
        bool is_last = (i == num_of_commands - 1);

        /* Create the pipe that connects this section to the next one.
         * Thanks to &&, pipe() is only called when this is not the last. */
        if (!is_last && pipe(fds) < 0)
        {
            perror("pipe");
            break;
        }

        pid_t pid = fork();

        if (pid < 0)
        {
            /* fork failed: release the pipe made for this round, stop. */
            perror("fork");
            if (!is_last)
            {
                close(fds[0]);
                close(fds[1]);
            }
            break;
        }
        else if (pid == 0)
        {
            /* ===== CHILD: runs section i ===== */

            /* Pipe before me: read from it instead of the keyboard. */
            if (prev_read != -1)
            {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);       /* fd 0 now covers it */
            }

            /* Pipe after me: write into it instead of the screen. */
            if (!is_last)
            {
                dup2(fds[1], STDOUT_FILENO);
                close(fds[0]);          /* belongs to the next child */
                close(fds[1]);          /* fd 1 now covers it */
            }

            /* File redirections come after the pipes, so a "> file"
             * overrides the pipe, just like in bash. */
            apply_redirections(commands[i]);

            /* Become the requested program. Only returns on failure. */
            execvp(commands[i][0], commands[i]);
            perror("mysh");
            exit(1);                    /* never fall back into the shell loop */
        }

        /* ===== PARENT ===== */

        pids[started] = pid;            /* CHANGE: was pids[i] */
        started++;

        /* The child has its own copy of the old read end: close ours. */
        if (prev_read != -1)
        {
            close(prev_read);
        }

        /* Keep only the new read end, for the next child. Closing the
         * write end here is essential, or the reader never sees EOF. */
        if (!is_last)
        {
            close(fds[1]);
            prev_read = fds[0];
        }
    }

    /* CHANGE: if we stopped early, a read end may still be open. */
    if (prev_read != -1)
    {
        close(prev_read);
    }

    /* All children run at the same time; now wait for every one. */
    for (int j = 0; j < started; j++)
    {
        waitpid(pids[j], NULL, 0);
    }
}

//---------------------------------------------------------------//

/*
 * apply_redirections - runs in the CHILD, before execvp.
 * Finds <, >, >> in args, removes them (and their file names)
 * from the list, and rewires fd 0 / fd 1 to the files.
 * On any error, prints a message and exits the child.
 */

void apply_redirections(char **args)
{

    char *infile = NULL;
    char *outfile = NULL;
    bool append = false;
    int j = 0;

    for (int i = 0; args[i] != NULL; i++)
    {

        if ((strcmp(args[i], ">") == 0) || (strcmp(args[i], ">>") == 0) || (strcmp(args[i], "<") == 0))
        {

            if (args[i + 1] == NULL)
            {
                fprintf(stderr, "mysh: syntax error: expected a file name after '%s'\n", args[i]);
                exit(1);
            }

            if (strcmp(args[i], "<") == 0)
            {
                infile = args[i + 1];
            }
            else
            {
                outfile = args[i + 1];
                append = (strcmp(args[i], ">>") == 0);
            }
            i++;
        }
        else
        {
            args[j] = args[i];
            j++;
        }
    }
    args[j] = NULL;
    if (j == 0)
    {
        fprintf(stderr, "mysh: syntax error: no command\n");
        exit(1);
    }

    if (outfile != NULL)
    {
        int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
        int ofd = open(outfile, flags, 0644);
        if (ofd < 0)
        {
            perror("mysh");
            exit(1);
        }
        dup2(ofd, STDOUT_FILENO);
        close(ofd);
    }
    if (infile != NULL)
    {
        int ifd = open(infile, O_RDONLY);
        if (ifd < 0)
        {
            perror("mysh");
            exit(1);
        }
        dup2(ifd, STDIN_FILENO);
        close(ifd);
    }
}

//----------------------------------------------------------//
/*
 * split_pipeline - split args into sections at each "|".
 * Each "|" is replaced with NULL, so every section becomes its own
 * NULL-terminated word list. cmds[k] points to the start of section k.
 * Runs in the PARENT: on a syntax error it prints a message and returns -1.
 * Returns the number of sections (1 if there are no pipes).
 */

int split_pipeline(char **args, char **cmds[])
{

    int n = 1;

    cmds[0] = &args[0];

    for (int i = 0; args[i] != NULL; i++)
    {
        if (strcmp(args[i], "|") == 0)
        {
            args[i] = NULL;
            cmds[n] = &args[i + 1];
            n++;
        }
    }

    for (int j = 0; j < n; j++)
    {
        if (cmds[j][0] == NULL)
        {
            fprintf(stderr, "mysh: syntax error near '|' \n");
            return -1;
        }
    }

    return n;
}