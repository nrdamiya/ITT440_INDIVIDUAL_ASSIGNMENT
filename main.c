#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define NUM_CHILDREN 3
#define SIZE 256

/* (GLOBAL VARIABLE: Detect Ctrl+C) */
volatile sig_atomic_t interrupted = 0;


/* (SIGNAL HANDLER: Handle Ctrl+C) */
void handle_sigint(int sig)
{
    (void)sig;
    interrupted = 1;

    const char msg[] = "\n[INTERRUPT] Ctrl+C detected!\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}


int main(void)
{
    /* (MAIN PROCESS: Declare variables) */
    int pipefd[NUM_CHILDREN][2];
    pid_t children[NUM_CHILDREN];
    char message[SIZE];
    int created = 0;

    /* (MAIN PROCESS: Register Ctrl+C handler) */
    signal(SIGINT, handle_sigint);

    /* (MAIN PROCESS: Display program information) */
    printf("====================================\n");
    printf("          FORKPIPE MESSENGER       \n");
    printf("====================================\n");
    printf("Parent PID: %ld\n", (long)getpid());

    /* (MAIN PROCESS: Read keyboard input) */
    printf("\nEnter your message: ");
    fflush(stdout);

    if (fgets(message, SIZE, stdin) == NULL) {
        if (interrupted) {
            printf("\n[PARENT] Program interrupted.\n");
            return EXIT_SUCCESS;
        }

        printf("Failed to read message.\n");
        return EXIT_FAILURE;
    }

    if (interrupted) {
        printf("\n[PARENT] Program interrupted.\n");
        return EXIT_SUCCESS;
    }

    /* (MAIN PROCESS: Remove newline) */
    message[strcspn(message, "\n")] = '\0';

    if (strlen(message) == 0) {
        printf("Message cannot be empty.\n");
        return EXIT_FAILURE;
    }

    /* (PIPE COMMUNICATION: Create pipes) */
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe");
            return EXIT_FAILURE;
        }
    }

    /* (PROCESS CREATION: Create child processes) */
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            break;
        }

        if (pid == 0) {
            /* (CHILD PROCESS: Ignore Ctrl+C) */
            signal(SIGINT, SIG_IGN);

            /* (CHILD PROCESS: Close unused pipe ends) */
            for (int j = 0; j < NUM_CHILDREN; j++) {
                close(pipefd[j][1]);

                if (j != i) {
                    close(pipefd[j][0]);
                }
            }

            /* (CHILD PROCESS: Read message from parent) */
            char received[SIZE];
            size_t total = 0;

            while (total < SIZE - 1) {
                ssize_t n = read(
                    pipefd[i][0],
                    received + total,
                    SIZE - 1 - total
                );

                if (n > 0) {
                    total += (size_t)n;
                } else if (n == -1 && errno == EINTR) {
                    continue;
                } else {
                    break;
                }
            }

            close(pipefd[i][0]);
            received[total] = '\0';

            /* (CHILD PROCESS: Display received message) */
            if (total > 0) {
                printf("\n[CHILD %d]\n", i + 1);
                printf("Child PID: %ld\n", (long)getpid());
                printf("Parent PID: %ld\n", (long)getppid());
                printf("Message received: %s\n", received);
                fflush(stdout);
            }

            _exit(EXIT_SUCCESS);
        }

        /* (MAIN PROCESS: Save child PID) */
        children[created] = pid;
        created++;

        printf("[PARENT] Child %d created. PID: %ld\n",
               i + 1, (long)pid);
    }

    /* (MAIN PROCESS: Close unused read ends) */
    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(pipefd[i][0]);
    }

    /* (PIPE COMMUNICATION: Send message to each child) */
    for (int i = 0; i < created; i++) {
        size_t length = strlen(message);
        size_t sent = 0;

        while (sent < length) {
            ssize_t n = write(
                pipefd[i][1],
                message + sent,
                length - sent
            );

            if (n > 0) {
                sent += (size_t)n;
            } else if (n == -1 && errno == EINTR) {
                continue;
            } else {
                perror("write");
                break;
            }
        }

        close(pipefd[i][1]);
    }

    /* (MAIN PROCESS: Close remaining write ends) */
    for (int i = created; i < NUM_CHILDREN; i++) {
        close(pipefd[i][1]);
    }

    /* (MAIN PROCESS: Wait for child processes) */
    for (int i = 0; i < created; i++) {
        waitpid(children[i], NULL, 0);
    }

    /* (MAIN PROCESS: Display final status) */
    if (interrupted) {
        printf("\n[PARENT] Program interrupted.\n");
    } else {
        printf("\n[PARENT] All child processes have finished.\n");
    }

    return EXIT_SUCCESS;
}
