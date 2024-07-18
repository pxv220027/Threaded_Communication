#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include "msg.h"

#define BUF 256
#define DATABASE_FILE "database.txt"

void Usage(char *progname);
int StartServer(unsigned short port, int *ret_fd);
void *HandleClient(void *arg);
void HandlePut(int client_fd, struct msg *message);
void HandleGet(int client_fd, struct msg *message);

// display usage instructions 
void Usage(char *progname) {
    printf("usage: %s port \n", progname);
    exit(EXIT_FAILURE);
}

// start the server and listen for incoming connections
int StartServer(unsigned short port, int *ret_fd) {
    // create the socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 0;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind the socket 
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 0;
    }

    // listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 0;
    }

    *ret_fd = server_fd;
    return 1;
}

// handle a PUT message from the client
void HandlePut(int client_fd, struct msg *message) {
    FILE *file = fopen(DATABASE_FILE, "ab+");
    if (!file) {
        perror("fopen");
        message->type = FAIL;
        write(client_fd, message, sizeof(struct msg));
        return;
    }

    // write the record to the database file
    if (fwrite(&message->rd, sizeof(struct record), 1, file) != 1) {
        perror("fwrite");
        message->type = FAIL;
    } else {
        message->type = SUCCESS;
    }

    fclose(file);
    write(client_fd, message, sizeof(struct msg));
}

// handle a GET message from the client
void HandleGet(int client_fd, struct msg *message) {
    FILE *file = fopen(DATABASE_FILE, "rb");
    if (!file) {
        perror("fopen");
        message->type = FAIL;
        write(client_fd, message, sizeof(struct msg));
        return;
    }

    struct record rd;
    int found = 0; // Flag to indicate if at least one matching record was found

    // Read and check each record in the database
    while (fread(&rd, sizeof(struct record), 1, file) == 1) {
        if (rd.id == message->rd.id) {
            found = 1; // Set flag to indicate match found
            // Send the matching record to the client
            memcpy(&message->rd, &rd, sizeof(struct record));
            message->type = SUCCESS;
            write(client_fd, message, sizeof(struct msg));
        }
    }

    fclose(file);

    // If no matching record was found, send failure message
    if (!found) {
        message->type = FAIL;
        write(client_fd, message, sizeof(struct msg));
    }
}

// handle communication with a client
void *HandleClient(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    struct msg message;
    while (1) {
        if (read(client_fd, &message, sizeof(struct msg)) <= 0) {
            break;
        }

        // process the message based on its type
        switch (message.type) {
            case PUT:
                HandlePut(client_fd, &message);
                break;
            case GET:
                HandleGet(client_fd, &message);
                break;
            default:
                printf("Invalid message type.\n");
        }
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char **argv) {
    // check for correct command-line arguments
    if (argc != 2) {
        Usage(argv[0]);
    }

    // parse the port number from the command line
    unsigned short port = 0;
    if (sscanf(argv[1], "%hu", &port) != 1) {
        Usage(argv[0]);
    }

    int server_fd;
    if (!StartServer(port, &server_fd)) {
        Usage(argv[0]);
    }

    while (1) {
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);
        if (*client_fd < 0) {
            perror("accept");
            continue;
        }

        pthread_t thread;
        // create a thread to handle communication with the client
        if (pthread_create(&thread, NULL, HandleClient, client_fd) != 0) {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
        } else {
            pthread_detach(thread);
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}