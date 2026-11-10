/*
 * mysh.c - A simple Unix shell, written for learning purposes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>     

#define MAX_LINE 1024   /* Maximum length of one input line, in characters.*/
#define MAX_ARGS 64     /* Maximum number of words in one command */

/* Program entry point. Every C program starts running here.
 * Returns an exit code to the operating system: 0 = success. */
int main(void) {

    char line[MAX_LINE];    /* Memory reserved for the line the user types (up to 1024 chars). */
    char *args[MAX_ARGS];   /* An array of pointers. Each element will point to the
                            * start of one word inside 'line'. No text is copied. */

    /* Main shell loop: prompt -> read -> respond -> repeat.
     * while (1) is an infinite loop; we leave it with 'break'. */
    while (1) {

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
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;         
        }
        int argc = 0;   /* how many words we have found so far */

        /* strtok finds the first word. The separators are:
         * space ' ', tab '\t', and newline '\n'.
         * Including '\n' also gets rid of the Enter key at the end. */
        char *token = strtok(line, " \t\n");

        /* Keep collecting words until there are none left.
         * We stop at MAX_ARGS - 1 to always leave room for the NULL. */
        while (token != NULL && argc < MAX_ARGS - 1) {
            args[argc] = token;          /* store a pointer to this word */
            argc++;
            token = strtok(NULL, " \t\n");  /* NULL = "continue the same line" */
        }

        /* Mark the end of the list. Programs that receive this list
         * (like execvp in the next stage) need it to know where it ends. */
        args[argc] = NULL;

        /* If the user just pressed Enter, there are no words. Show a new prompt. */
        if (argc == 0) {
            continue;   /* skip the rest of the loop and start again */
        }

        /* For now, print each word so we can see the result. */
        for (int i = 0; i < argc; i++) {
            printf("args[%d] = \"%s\"\n", i, args[i]);
        }

        /* NEW: built-in command "exit".
         * strcmp compares two strings and returns 0 if they are equal.
         * This must be handled by the shell itself: it is the shell
         * that needs to stop, not a separate program. */
        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        /* ---------- NEW: run the command as a new process ---------- */

        /* Ask the kernel to clone this process.
         * pid_t is the data type used for process IDs. */
        pid_t pid = fork();

        if (pid < 0) {
            /* fork failed (rare: e.g. the system has too many processes).
             * perror prints our message plus the reason from the OS. */
            perror("fork");

        } else if (pid == 0) {
            /* ===== CHILD PROCESS ===== (fork returned 0)
             * Replace this process's code with the requested program.
             * args[0] is the program name, args is the full argument list. */
            execvp(args[0], args);

            /* We only get here if execvp FAILED
             * (for example, the command does not exist). */
            perror("mysh");

            /* The child must exit now. Otherwise it would continue
             * running the shell loop, and we'd have two shells! */
            exit(1);

        } else {
            /* ===== PARENT PROCESS ===== (fork returned the child's PID)
             * Wait until that specific child finishes.
             * NULL = we don't need its exit status yet.
             * 0    = no special options. */
            waitpid(pid, NULL, 0);
        }
    }
    return 0;
}