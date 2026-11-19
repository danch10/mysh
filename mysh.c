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
void applay_redirections(char **args);

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

void run_command(char **args)
{

    /* Ask the kernel to clone this process.
     * pid_t is the data type used for process IDs. */
    pid_t pid = fork();

    if (pid < 0)
    {
        /* fork failed (rare: e.g. the system has too many processes).
         * perror prints our message plus the reason from the OS. */
        perror("fork");
    }
    else if (pid == 0)
    {
        /* ===== CHILD PROCESS ===== (fork returned 0)
         * Replace this process's code with the requested program.
         * args[0] is the program name, args is the full argument list. */

        /* Handle <, >, >> : clean the argument list and rewire fd 0 / fd 1. */
        applay_redirections(args);

        /* Replace this process's code with the requested program. */
        execvp(args[0], args);

        /* We only get here if execvp FAILED
         * (for example, the command does not exist). */
        perror("mysh");

        /* The child must exit now. Otherwise it would continue
         * running the shell loop, and we'd have two shells! */
        exit(1);
    }
    else
    {
        /* ===== PARENT PROCESS ===== (fork returned the child's PID)
         * Wait until that specific child finishes.
         * NULL = we don't need its exit status yet.
         * 0    = no special options. */
        waitpid(pid, NULL, 0);
    }
}

//---------------------------------------------------------------//

/*
 * apply_redirections - runs in the CHILD, before execvp.
 * Finds <, >, >> in args, removes them (and their file names)
 * from the list, and rewires fd 0 / fd 1 to the files.
 * On any error, prints a message and exits the child.
 */

void applay_redirections(char **args)
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