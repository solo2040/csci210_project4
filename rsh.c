#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp","touch","mkdir","ls","pwd","cat","grep","chmod","diff","cd","exit","help","sendmsg"};

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
    int serverFIFO;
    struct message msgStruct;

    // Fill the message struct with source, target, and msg.
    strcpy(msgStruct.source, user);
    strcpy(msgStruct.target, target);
    strcpy(msgStruct.msg, msg);

    // Open the server's FIFO to send the message
    serverFIFO = open("serverFIFO", O_WRONLY);
    if (serverFIFO == -1) {
        perror("Failed to open serverFIFO");
        exit(1);
    }

    // Write the message struct to the server FIFO
    write(serverFIFO, &msgStruct, sizeof(msgStruct));

    // Close the server FIFO after sending the message
    close(serverFIFO);
}


void* messageListener(void *arg) {
    int userFIFO;
    struct message incomingMsg;

    // Open the user's FIFO for reading incoming messages
    userFIFO = open(uName, O_RDONLY);
    if (userFIFO == -1) {
        perror("Failed to open user FIFO");
        pthread_exit((void*)1);
    }

    // Infinite loop to listen for incoming messages
    while (1) {
        // Read the message from the FIFO
        int bytesRead = read(userFIFO, &incomingMsg, sizeof(incomingMsg));
        if (bytesRead > 0) {
            // Print the incoming message
            printf("Incoming message from %s: %s\n", incomingMsg.source, incomingMsg.msg);
        }
    }

    // Close the FIFO and exit the thread
    close(userFIFO);
    pthread_exit((void*)0);
}


int isAllowed(const char*cmd) {
	int i;
	for (i=0;i<N;i++) {
		if (strcmp(cmd,allowed[i])==0) {
			return 1;
		}
	}
	return 0;
}
// Handle the built-in "cd" command
void handle_cd(char *args[]) {
    if (args[1] == NULL || args[2] != NULL) {
        printf("-rsh: cd: too many arguments\n");
    } else if (chdir(args[1]) == -1) {
        perror("cd");
    }
}

// Handle the built-in "exit" command
void handle_exit() {
    exit(0);
}

// Handle the built-in "help" command
void handle_help() {
    printf("The allowed commands are:\n");
    for (int i = 0; i < N; i++) {
        printf("%d: %s\n", i + 1, allowed[i]);
    }
}

// Spawn an external command
void spawn_command(char *args[]) {
    pid_t pid;
    int status;
    
    // Use posix_spawnp to spawn the process
    if (posix_spawnp(&pid, args[0], NULL, NULL, args, environ) != 0) {
        perror("posix_spawnp");
        return;
    }

    // Wait for the child process to finish
    waitpid(pid, &status, 0);
}
int main(int argc, char **argv) {
    // Validate arguments
    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }

    // Set up the username
    strcpy(uName, argv[1]);
    signal(SIGINT, terminate);

    // Create the message listener thread
    pthread_t listenerThread;
    if (pthread_create(&listenerThread, NULL, messageListener, NULL) != 0) {
        perror("Error creating message listener thread");
        exit(1);
    }

    while (1) {
        fprintf(stderr, "rsh>");

        // Read the user's input
        char line[256];
        if (fgets(line, 256, stdin) == NULL) continue;

        if (strcmp(line, "\n") == 0) continue;
        line[strlen(line) - 1] = '\0'; // Remove newline

        char cmd[256];
        strcpy(cmd, strtok(line, " "));

        if (strcmp(cmd, "sendmsg") == 0) {
            char *target = strtok(NULL, " ");
            if (!target) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }

            char *msg = strtok(NULL, "");
            if (!msg) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }

            // Call sendmsg function to send the message
            sendmsg(uName, target, msg);
            continue;
        }

        // If the command is "cd"
        if (strcmp(args[0], "cd") == 0) {
            if (isAllowed(args[0])) {
                handle_cd(args);
            } else {
                printf("NOT ALLOWED!\n");
            }
        }
        // If the command is "exit"
        else if (strcmp(args[0], "exit") == 0) {
            if (isAllowed(args[0])) {
                handle_exit();
            } else {
                printf("NOT ALLOWED!\n");
            }
        }
        // If the command is "help"
        else if (strcmp(args[0], "help") == 0) {
            if (isAllowed(args[0])) {
                handle_help();
            } else {
                printf("NOT ALLOWED!\n");
            }
        }
        // For other commands, check if they are allowed and spawn processes
        else if (isAllowed(args[0])) {
            spawn_command(args);
        }
        // If the command is not allowed
        else {
            printf("NOT ALLOWED!\n");
        }
    }

    // Wait for the message listener thread to finish before exiting
    pthread_join(listenerThread, NULL);

    return 0;
}

