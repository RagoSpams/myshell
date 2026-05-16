#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>  // add this at the top of the file with your other includes


#define MAX_INPUT 1024
#define MAX_ARGS  64
#define MAX_HISTORY 100


// Prints the prompt
void print_prompt() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    // \033[1;32m = bold green, \033[1;34m = bold blue, \033[0m = reset
    printf("\033[1;32mmyshell\033[0m:\033[1;34m%s\033[0m$ ", cwd);
    fflush(stdout);
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

void handle_redirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {

        if (strcmp(args[i], ">") == 0) {
            // Open file for writing (creates it if it doesn't exist, truncates if it does)
            int fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { perror(args[i+1]); exit(1); }
            dup2(fd, STDOUT_FILENO);  // stdout now goes to the file
            close(fd);
            args[i] = NULL;  // cut off args here so execvp doesn't see ">" or filename

        } else if (strcmp(args[i], ">>") == 0) {
            // Open file for appending
            int fd = open(args[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) { perror(args[i+1]); exit(1); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;

        } else if (strcmp(args[i], "<") == 0) {
            // Open file for reading, redirect stdin
            int fd = open(args[i+1], O_RDONLY);
            if (fd < 0) { perror(args[i+1]); exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
        }
    }
}

// Forks a child process and runs the command
// Finds the index of a pipe "|" in args, or returns -1
int find_pipe(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) return i;
    }
    return -1;
}

void execute(char **args, int background) {
    int pipe_pos = find_pipe(args);

    if (pipe_pos == -1) {
        // No pipe — same as before
        pid_t pid = fork();
        if (pid == 0) {
            handle_redirection(args);
            if (execvp(args[0], args) == -1) {
                perror(args[0]);
                exit(1);
            }
        } else {
            if (background){
                printf("[background] pid %d\n", pid);
            } else {
                int status;
                waitpid(pid, &status, 0);
            }
        }
        return;
    }

    // Split args into left and right at the pipe
    args[pipe_pos] = NULL;
    char **left  = args;
    char **right = args + pipe_pos + 1;

    int fd[2];
    if (pipe(fd) < 0) { perror("pipe"); return; }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Left child: write its stdout to the pipe's write end
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        handle_redirection(left);
        if (execvp(left[0], left) == -1) { perror(left[0]); exit(1); }
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Right child: read its stdin from the pipe's read end
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        handle_redirection(right);
        if (execvp(right[0], right) == -1) { perror(right[0]); exit(1); }
    }

    // Parent closes both ends and waits for both children
    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
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

        int background = 0;
        int arg_count = 0;
        while (args[arg_count] != NULL) arg_count++;

        if (arg_count > 0 && strcmp(args[arg_count - 1], "&") == 0) {
            background = 1;
            args[arg_count - 1] = NULL;  // remove the & from args
}

        // 5. Check for "exit"
        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        // 6. Try to run as a built-in (cd, history). 
        // If it's NOT a built-in, then execute it as a normal program.
        if (!handle_builtin(args)) {
            execute(args, background);
        }
    }

    return 0;
}