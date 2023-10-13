#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

void echo_client(int sockfd) {
    char buff[MSG_LEN];
    struct pollfd fds[2];
    int n;

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    printf("Message: ");
    fflush(stdout);

    while (1) {
        
        poll(fds, 2, -1);

        if (fds[0].revents & POLLIN) {
            // Cleaning memory
            memset(buff, 0, MSG_LEN);
            // Getting message from client
            n=0;
            while ((buff[n++] = getchar()) != '\n') {} // trailing '\n' will be sent
            // Sending message (ECHO)
            if (send(sockfd, buff, strlen(buff), 0) <= 0) {
                break;
            }

            if (strcmp(buff, "/quit\n") == 0) {
                printf("Message sent. Disconnecting...\n");
                break;
            }
            printf("Message sent!\n");
        }

        if (fds[1].revents & POLLIN) {
            // Cleaning memory
            memset(buff, 0, MSG_LEN);
            // Receiving message
            if (recv(sockfd, buff, MSG_LEN, 0) <= 0) {
                break;
            }
            printf("Received: %s", buff);
            printf("Message: ");
            fflush(stdout);
        }
    }
}

int handle_connect(char *argv[]) {
    struct addrinfo hints, *result, *rp;
    int sfd;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(argv[1], argv[2], &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
        if (sfd == -1) {
            continue;
        }
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }
        close(sfd);
    }
    if (rp == NULL) {
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result);
    return sfd;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_name> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int sfd = handle_connect(argv);
    echo_client(sfd);
    close(sfd);
    return EXIT_SUCCESS;
}