#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS  64
#define MAX_HISTORY 100


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
char history[MAX_HISTORY][MAX_INPUT];
int history_count = 0;

void add_to_history(char *input) {
    if (history_count < MAX_HISTORY) {
        strncpy(history[history_count++], input, MAX_INPUT - 1);
    }
}

void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

// Expands ~ to the home directory path
// e.g. "~/Documents" becomes "/home/yourname/Documents"
void expand_tilde(char *path, char *result, size_t size) {
    if (path[0] == '~') {
        char *home = getenv("HOME");
        snprintf(result, size, "%s%s", home, path + 1);
    } else {
        strncpy(result, path, size);
    }
}

// Returns 1 if a built-in was handled, 0 if not
int handle_builtin(char **args) {
    if (strcmp(args[0], "cd") == 0) {
        char path[MAX_INPUT];
        if (args[1] == NULL) {
            // cd with no args goes home
            chdir(getenv("HOME"));
        } else {
            expand_tilde(args[1], path, sizeof(path));
            if (chdir(path) != 0) {
                perror(args[1]);
            }
        }
        return 1;
    }

    if (strcmp(args[0], "history") == 0) {
        print_history();
        return 1;
    }

    return 0;  // not a built-in
}

int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    while (1) {
        print_prompt();

        // 1. Read input
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            printf("\n");
            break;
        }

        // 2. Skip empty lines
        if (input[0] == '\n') continue;

        // 3. Save to history BEFORE tokenizing (so we save the full string)
        add_to_history(input);

        // 4. Break string into arguments
        int count = tokenize(input, args);
        if (count == 0) continue;

        // 5. Check for "exit"
        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        // 6. Try to run as a built-in (cd, history). 
        // If it's NOT a built-in, then execute it as a normal program.
        if (!handle_builtin(args)) {
            execute(args);
        }
    }

    return 0;
}