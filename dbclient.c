#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "msg.h"

void Usage(char *progname);
int LookupName(char *name, unsigned short port, struct sockaddr_storage *ret_addr, size_t *ret_addrlen);
int Connect(const struct sockaddr_storage *addr, const size_t addrlen, int *ret_fd);
void ClearInputBuffer();

int main(int argc, char **argv) {
    // check for correct command-line arguments
    if (argc != 3) {
        Usage(argv[0]);
    }

    // extract the port number from the command line
    unsigned short port = 0;
    if (sscanf(argv[2], "%hu", &port) != 1) {
        Usage(argv[0]);
    }

    struct sockaddr_storage addr;
    size_t addrlen;
    if (!LookupName(argv[1], port, &addr, &addrlen)) {
        Usage(argv[0]);
    }

    // connect to the server using the address information
    int socket_fd;
    if (!Connect(&addr, addrlen, &socket_fd)) {
        Usage(argv[0]);
    }

    struct msg message;
    int choice;
    char name[256];
    uint32_t id;

    // handle user interaction for sending PUT or GET requests
    while (1) {
        printf("Enter your choice (1 to put, 2 to get, 0 to quit): ");
        scanf("%d", &choice);
        ClearInputBuffer();

        switch (choice) {
            case 1: 
                printf("Enter the name: ");
                fgets(name, 256, stdin);
                name[strcspn(name, "\n")] = '\0'; 
                printf("Enter the id: ");
                scanf("%u", &id);
                ClearInputBuffer();
                message.type = PUT;
                strncpy(message.rd.name, name, 256); 
                message.rd.id = id;
                write(socket_fd, &message, sizeof(struct msg));
                read(socket_fd, &message, sizeof(struct msg));
                if (message.type == SUCCESS) {
                    printf("Put success.\n");
                } else {
                    printf("Put failed.\n");
                }
                break;

            case 2: 
                printf("Enter the id: ");
                scanf("%u", &id);
                ClearInputBuffer();
                message.type = GET;
                message.rd.id = id;
                write(socket_fd, &message, sizeof(struct msg));
                read(socket_fd, &message, sizeof(struct msg));
                if (message.type == SUCCESS) {
                    printf("name: %s\n", message.rd.name);
                    printf("id: %u\n", message.rd.id);
                } else {
                    printf("Get failed.\n");
                }
                break;

            case 0: 
                close(socket_fd);
                return EXIT_SUCCESS;

            default:
                printf("Invalid choice, please try again.\n");
                break;
        }
    }

    close(socket_fd);
    return EXIT_SUCCESS;
}

// display usage instructions for the program
void Usage(char *progname) {
    printf("usage: %s  hostname port \n", progname);
    exit(EXIT_FAILURE);
}

// look up the hostname and obtain the address information
int LookupName(char *name, unsigned short port, struct sockaddr_storage *ret_addr, size_t *ret_addrlen) {
    struct addrinfo hints, *results;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int retval;
    if ((retval = getaddrinfo(name, NULL, &hints, &results)) != 0) {
        printf("getaddrinfo failed: %s", gai_strerror(retval));
        return 0;
    }

    // set the port for the obtained address information
    if (results->ai_family == AF_INET) {
        struct sockaddr_in *v4addr = (struct sockaddr_in *) (results->ai_addr);
        v4addr->sin_port = htons(port);
    } else if (results->ai_family == AF_INET6) {
        struct sockaddr_in6 *v6addr = (struct sockaddr_in6 *)(results->ai_addr);
        v6addr->sin6_port = htons(port);
    } else {
        printf("getaddrinfo failed to provide an IPv4 or IPv6 address \n");
        freeaddrinfo(results);
        return 0;
    }

    assert(results != NULL);
    memcpy(ret_addr, results->ai_addr, results->ai_addrlen);
    *ret_addrlen = results->ai_addrlen;

    freeaddrinfo(results);
    return 1;
}

// connect to the server using the address information
int Connect(const struct sockaddr_storage *addr, const size_t addrlen, int *ret_fd) {
    // create a socket
    int socket_fd = socket(addr->ss_family, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        printf("socket() failed: %s", strerror(errno));
        return 0;
    }

    // connect to the server
    int res = connect(socket_fd, (const struct sockaddr *)(addr), addrlen);
    if (res == -1) {
        printf("connect() failed: %s", strerror(errno));
        return 0;
    }

    *ret_fd = socket_fd;
    return 1;
}

// clear the input buffer to avoid any characters
void ClearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}
