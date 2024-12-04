#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

struct message {
    char source[50];
    char target[50];
    char msg[200]; // message body
};

void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server_fifo;
    int target_fifo;
    int dummyfd;
    struct message req;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    // Open the server FIFO in read-only mode
    server_fifo = open("serverFIFO", O_RDONLY);
    if (server_fifo == -1) {
        perror("Failed to open serverFIFO");
        return 1;
    }

    dummyfd = open("serverFIFO", O_WRONLY); // To keep the server FIFO open for writing

    while (1) {
        // Read the request from the server FIFO
        ssize_t bytes_read = read(server_fifo, &req, sizeof(req));
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                break; // No more messages
            }
            perror("Error reading from serverFIFO");
            continue;
        }

        printf("Received a request from %s to send the message '%s' to %s.\n", req.source, req.msg, req.target);

        // Open the target user's FIFO
        target_fifo = open(req.target, O_WRONLY);
        if (target_fifo == -1) {
            perror("Failed to open target FIFO");
            continue;
        }

        // Write the message structure to the target FIFO
        if (write(target_fifo, &req, sizeof(req)) == -1) {
            perror("Failed to send message to target FIFO");
        }

        close(target_fifo);
    }

    close(server_fifo);
    close(dummyfd);
    return 0;
}
