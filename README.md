# MyShell
![MyShell Demo](Unixshell_demo.gif)
A functional command-line interpreter built in C to explore the interaction between user-space applications and the Linux kernel. This project moves beyond basic command execution to handle the complexities of process synchronization and file stream manipulation.

## Technical Breakdown

### Process Lifecycle
The shell uses the `fork-exec` model. Every command is spawned as a child process using `fork()`, while the parent manages the shell's state and execution flow.

### I/O Redirection & Streams
I implemented redirection by manipulating the process file descriptor table. Using `dup2()`, the shell intercepts standard streams (stdin/stdout) and re-routes them to file pointers, allowing for features like:
* Output redirection (`>`) and appending (`>>`)
* Input redirection (`<`)

### Pipelines
The pipe (`|`) implementation handles inter-process communication by creating a shared memory buffer. The shell coordinates two child processes simultaneously, linking the output of the first to the input of the second.

### Background Execution
By parsing for a trailing `&`, the shell can run tasks asynchronously. This is achieved by intentionally skipping the `waitpid()` call in the parent process, allowing the shell to remain responsive while the child task executes.

## Built-ins
I wrote internal logic for commands that cannot run as separate processes:
* `cd`: Directly updates the shell's environment via `chdir()`.
* `history`: A custom data structure that tracks and retrieves previous inputs.

## Setup
1. Clone the repo.
2. Run `make` to compile the source.
3. Execute with `./myshell`.