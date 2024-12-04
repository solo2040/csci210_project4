int main() {
    int serverFIFO, targetFIFO;
    struct message req;

    // Open server FIFO for reading and dummy file descriptor for writing
    serverFIFO = open("serverFIFO", O_RDONLY);
    if (serverFIFO == -1) {
        perror("Failed to open serverFIFO");
        exit(1);
    }

    while (1) {
        // Read the message request from server FIFO
        int bytesRead = read(serverFIFO, &req, sizeof(req));
        if (bytesRead > 0) {
            // Print the message details
            printf("Received a request from %s to send the message %s to %s.\n", req.source, req.msg, req.target);

            // Open the target user's FIFO for writing the message
            targetFIFO = open(req.target, O_WRONLY);
            if (targetFIFO == -1) {
                perror("Failed to open target user's FIFO");
                continue;
            }

            // Write the message to the target FIFO
            write(targetFIFO, &req, sizeof(req));

            // Close the target FIFO after writing the message
            close(targetFIFO);
        }
    }

    // Close the server FIFO when done
    close(serverFIFO);
    return 0;
}
