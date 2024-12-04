#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep", "chmod", "diff", "cd", "exit", "help", "sendmsg"};

struct message {
    char source[50];
    char target[50];
    char msg[200];
};

void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

void sendmsg(char *user, char *target, char *msg) {
    // Create the message structure
    struct message msg_struct;
    strcpy(msg_struct.source, uName);
    strcpy(msg_struct.target, target);
    strcpy(msg_struct.msg, msg);

    // Open the server's FIFO to send the message
    int server_fifo = open("serverFIFO", O_WRONLY);
    if (server_fifo == -1) {
        perror("Failed to open serverFIFO");
        return;
    }

    // Write the message structure to the server FIFO
    if (write(server_fifo, &msg_struct, sizeof(msg_struct)) == -1) {
        perror("Failed to send message to serverFIFO");
    }

    close(server_fifo);
}

void* messageListener(void *arg) {
    char fifo_name[256];
    snprintf(fifo_name, sizeof(fifo_name), "%s", uName); // Use the username as FIFO name

    // Open the user's FIFO in read-only mode
    int user_fifo = open(fifo_name, O_RDONLY);
    if (user_fifo == -1) {
        perror("Failed to open user's FIFO");
        pthread_exit(NULL);
    }

    struct message msg;
    while (1) {
        // Read a message from the FIFO
        ssize_t bytes_read = read(user_fifo, &msg, sizeof(msg));
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                break; // No more messages
            }
            perror("Error reading from FIFO");
            continue;
        }

        // Print the incoming message in the desired format
        printf("Incoming message from %s: %s\n", msg.source, msg.msg);
    }

    close(user_fifo);
    pthread_exit(NULL);
}

int isAllowed(const char*cmd) {
    int i;
    for (i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv;
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;

    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }
    signal(SIGINT, terminate);

    strcpy(uName, argv[1]);

    // Create the message listener thread
    pthread_t listener_thread;
    int result = pthread_create(&listener_thread, NULL, messageListener, NULL);
    if (result != 0) {
        perror("Failed to create message listener thread");
        exit(1);
    }

    while (1) {
        fprintf(stderr, "rsh>");

        if (fgets(line, 256, stdin) == NULL) continue;

        if (strcmp(line, "\n") == 0) continue;

        line[strlen(line) - 1] = '\0'; // Remove the newline character

        char cmd[256];
        char line2[256];
        strcpy(line2, line);
        strcpy(cmd, strtok(line, " "));

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            // Handle the sendmsg command
            char *target = strtok(NULL, " ");
            if (target == NULL) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }

            char *msg = strtok(NULL, " ");
            if (msg == NULL) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }

            // Join the remaining tokens to form the full message (in case there are spaces)
            char full_msg[200];
            strcpy(full_msg, msg);
            char *part;
            while ((part = strtok(NULL, " ")) != NULL) {
                strcat(full_msg, " ");
                strcat(full_msg, part);
            }

            sendmsg(uName, target, full_msg);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (strtok(NULL, " ") != NULL) {
                printf("-rsh: cd: too many arguments\n");
            } else {
                chdir(targetDir);
            }
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("%d: %s\n", i + 1, allowed[i]);
            }
            continue;
        }

        cargv = (char**)malloc(sizeof(char*));
        cargv[0] = (char *)malloc(strlen(cmd) + 1);
        path = (char *)malloc(9 + strlen(cmd) + 1);
        strcpy(path, cmd);
        strcpy(cargv[0], cmd);

        char *attrToken = strtok(line2, " ");
        attrToken = strtok(NULL, " ");
        int n = 1;
        while (attrToken != NULL) {
            n++;
            cargv = (char**)realloc(cargv, sizeof(char*) * n);
            cargv[n - 1] = (char *)malloc(strlen(attrToken) + 1);
            strcpy(cargv[n - 1], attrToken);
            attrToken = strtok(NULL, " ");
        }
        cargv = (char**)realloc(cargv, sizeof(char*) * (n + 1));
        cargv[n] = NULL;

        posix_spawnattr_init(&attr);

        if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            exit(EXIT_FAILURE);
        }

        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }

        posix_spawnattr_destroy(&attr);
    }
    return 0;
}
