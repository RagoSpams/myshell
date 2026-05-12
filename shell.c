#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS  64

// Prints the prompt
void print_prompt() {
    printf("myshell> ");
    fflush(stdout);  // force it to display immediately
}

// Splits the input string into an array of argument tokens
// e.g. "ls -la /home" -> ["ls", "-la", "/home", NULL]
int tokenize(char *input, char **args) {
    int count = 0;
    char *token = strtok(input, " \t\n");
    while (token != NULL && count < MAX_ARGS - 1) {
        args[count++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[count] = NULL;  // execvp requires NULL-terminated array
    return count;
}

// Forks a child process and runs the command
void execute(char **args) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        // We are in the CHILD process
        // execvp replaces this child process with the requested program
        // It searches PATH automatically (that's what the 'p' means)
        if (execvp(args[0], args) == -1) {
            perror(args[0]);  // prints "ls: No such file or directory" etc.
            exit(1);
        }
    } else {
        // We are in the PARENT process (the shell)
        // Wait for the child to finish before showing next prompt
        int status;
        waitpid(pid, &status, 0);
    }
}

int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    while (1) {
        print_prompt();

        // Read a line of input. If Ctrl+D is pressed, fgets returns NULL → exit.
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            printf("\n");
            break;
        }

        // Skip blank lines
        if (input[0] == '\n') continue;

        int count = tokenize(input, args);
        if (count == 0) continue;

        // Handle the exit built-in
        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        execute(args);
    }

    return 0;
}