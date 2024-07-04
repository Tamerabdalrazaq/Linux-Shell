#define _GNU_SOURCE

// #include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define OP_BG 0
#define OP_PIPE 1
#define OP_REDIR_IN 2
#define OP_REDIR_OUT 3

int process_arglist_normal(int count, char** arglist);
int process_arglist_background(int count, char** arglist);
int process_arglist_pipe(int count, char** arglist, int pipe_pos);
int process_arglist_redir_input(int count, char** arglist, int redir_pos);
int process_arglist_redir_out(int count, char** arglist, int redir_pos);


struct Opeartion {
    int op_type;
    int position;
};

int prepare(void) {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN; // Ignore SIGINT
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to set SIGINT handler");
        exit(1);
    }

    return 0;
}

int finalize(void){
    return 0;
}



struct Opeartion special_command_num(int count, char** arglist) {
    struct Opeartion op;
    op.position = -1;
    op.op_type = -1;
    for (int i = 0; i < count; ++i) {
        if (strcmp(arglist[i], "|") == 0) {
            op.position = i;
            op.op_type = OP_PIPE;
            break;
        }
        if (strcmp(arglist[i], "&") == 0){
            op.position = i;
            op.op_type = OP_BG;
            break;
        }
        if (strcmp(arglist[i], "<") == 0){
            op.position = i;
            op.op_type = OP_REDIR_IN;
            break;
        }
        if (strcmp(arglist[i], ">>") == 0){
            op.position = i;
            op.op_type = OP_REDIR_OUT;
            break;
        }
    }
    return op;
}

int process_arglist(int count, char** arglist) {
    struct Opeartion op = special_command_num(count, arglist);

    if (op.op_type == OP_BG) {
        // Remove "&" from the argument list
        arglist[count - 1] = NULL;
        return process_arglist_background(count, arglist);
    } else if (op.op_type == OP_PIPE) {
        return process_arglist_pipe(count, arglist, op.position);
    } else if (op.op_type == OP_REDIR_IN){
        return process_arglist_redir_input(count, arglist, op.position);
    } else if (op.op_type == OP_REDIR_OUT){
        return process_arglist_redir_out(count, arglist, op.position);
    } else {
        return process_arglist_normal(count, arglist);
    }

    return 1;
}

int process_arglist_normal(int count, char** arglist) {
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        perror("Fork failed");
        return 1;
    } else if (pid == 0) {
        // Child process

        struct sigaction sa;
        sa.sa_handler = SIG_DFL; // Default signal handling
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("Failed to set SIGINT handler");
            exit(1);
        }

        if (execvp(arglist[0], arglist) == -1) {
            perror("Command execution failed");
        }
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        wait(NULL);
    }

    // Return 1 to continue running the shell, or 0 to exit
    return 1;
}

int process_arglist_background(int count, char** arglist) {
    printf("\nrunning in the bg\n");
    // Create a child process
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        perror("Fork failed");
        return 1;
    } else if (pid == 0) {
        // Child process
        struct sigaction sa;
        sa.sa_handler = SIG_IGN; // Ignore SIGINT
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("Failed to set SIGINT handler");
            exit(1);
        }

        // Redirect stdin to /dev/null
        int dev_null = open("/dev/null", O_RDONLY);
        if (dev_null == -1) {
            perror("Failed to open /dev/null");
            exit(1);
        }
        if (dup2(dev_null, STDIN_FILENO) == -1) {
            perror("Failed to redirect stdin to /dev/null");
            exit(1);
        }
        close(dev_null);

        // Execute the command
        if (execvp(arglist[0], arglist) == -1) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    }

    // Parent process does not wait for the child process to finish
    // Return 1 to continue running the shell
    return 1;
}


int process_arglist_pipe(int count, char** arglist, int pipe_pos) {
    // Find the pipe symbol in the arglist
    // Create a pipe
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe failed");
        return 1;
    }

    // Fork the first child process
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("Fork failed");
        return 1;
    } else if (pid1 == 0) {
        // First child process
        struct sigaction sa;
        sa.sa_handler = SIG_DFL; // Default signal handling
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("Failed to set SIGINT handler");
            exit(1);
        }

        // Redirect stdout to the write end of the pipe
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("dup2 failed");
            exit(1);
        }
        close(pipe_fd[0]); // Close unused read end of the pipe
        close(pipe_fd[1]);

        // Null-terminate the argument list for the first command
        arglist[pipe_pos] = NULL;

        // Execute the first command
        if (execvp(arglist[0], arglist) == -1) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    }

    // Fork the second child process
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("Fork failed");
        return 1;
    } else if (pid2 == 0) {
        // Second child process
        struct sigaction sa;
        sa.sa_handler = SIG_DFL; // Default signal handling
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("Failed to set SIGINT handler");
            exit(1);
        }
        
        // Redirect stdin to the read end of the pipe
        if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
            perror("dup2 failed");
            exit(1);
        }
        close(pipe_fd[0]); // Close unused write end of the pipe
        close(pipe_fd[1]);

        // Execute the second command
        if (execvp(arglist[pipe_pos + 1], &arglist[pipe_pos + 1]) == -1) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    }

    // Parent process
    close(pipe_fd[0]); // Close the read end of the pipe
    close(pipe_fd[1]); // Close the write end of the pipe

    // Wait for both child processes to finish
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 1;
}

int process_arglist_redir_input(int count, char** arglist, int redir_pos) {

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return 1;
    } else if (pid == 0) {
        int input_fd = open(arglist[redir_pos + 1], O_RDONLY);
        if (input_fd == -1) {
            perror("Failed to open input file");
            exit(1);
        }

        if (dup2(input_fd, STDIN_FILENO) == -1) {
            perror("Failed to redirect stdin");
            close(input_fd);
            exit(1);
        }
        close(input_fd);

        arglist[redir_pos] = NULL;

        if (execvp(arglist[0], arglist) == -1) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    } else {
        wait(NULL);
    }

    return 1;
}

int process_arglist_redir_out(int count, char** arglist, int redir_pos) {

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return 1;
    } else if (pid == 0) {
        int output_fd = open(arglist[redir_pos + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (output_fd == -1) {
            perror("Failed to open output file");
            exit(1);
        }

        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("Failed to redirect stdout");
            close(output_fd);
            exit(1);
        }
        close(output_fd);

        arglist[redir_pos] = NULL;

        if (execvp(arglist[0], arglist) == -1) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    } else {
        wait(NULL);
    }
    return 1;
}