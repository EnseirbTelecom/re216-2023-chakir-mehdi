#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include "common.h"

#define MAX_CNX 256

int quit;
int quit_flag = 0;
int sfd;

struct Client{
    int sockfd;
    int client_num;
    struct sockaddr_in client_addr;
    struct Client *nextclient;
};

struct Client *client_list = NULL; // ici on stocke les descripteurs de fichiers, adresses et ports


void addClient(struct Client **client_list, int client_num, int sockfd, struct sockaddr_in client_addr) {
    struct Client *client = malloc(sizeof(struct Client));
    if (client == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    client->sockfd = sockfd;
    client->client_addr = client_addr;
    client->client_num = client_num;
    client->nextclient = *client_list;
    *client_list = client;
}

void removeClient(struct Client **client_list, int sockfd) {
    struct Client *current = *client_list;
    struct Client *prev = NULL;

    while (current != NULL) {
        if (current->sockfd == sockfd) {
            if (prev == NULL) {
                *client_list = current->nextclient;
            } else {
                prev->nextclient = current->nextclient;
            }
            close(current->sockfd);
            free(current);
            return;
        }
        prev = current;
        current = current->nextclient;
    }
}

void freeClients(struct Client *first) {
    while (first != NULL) {
        struct Client *clean_client = first;
        first = first->nextclient;
        close(clean_client->sockfd);
        free(clean_client);           
    }
}

void handle_sigint(int sig) {
	printf("\nReceived SIGINT. Cleaning up and exiting...\n");
	freeClients(client_list);
	close(sfd);
    quit_flag = 1;
	exit(EXIT_SUCCESS);
}

void echo_server(int sockfd, struct Client *client) {
	quit = 0;
	char buff[MSG_LEN];
	while (1) {
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		// Receiving message
		if (recv(sockfd, buff, MSG_LEN, 0) <= 0) {
			break;
		}
		// if received message is /quit
        if (strcmp(buff, "/quit\n") == 0) {
			quit = 1;
			printf("Client %d disconnected.\n",client->client_num);
			removeClient(&client_list, sockfd);
            break; 
        }
		printf("Message received from client %d: %s",client->client_num, buff);
		// Sending message (ECHO)
		if (send(sockfd, buff, strlen(buff), 0) <= 0) {
			break;
		}
		printf("Message sent to client %d!\n",client->client_num);
		break;
	}
}

int handle_bind(char *argv[]) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(NULL, argv[1], &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

void handle_multipleclients(int sockfd){
	struct sockaddr_in client_addr;
	socklen_t len;

	struct pollfd fds[MAX_CNX];
	memset(fds,0,sizeof(struct pollfd)*MAX_CNX); 
    
	fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
	
	int i;
    int client_num = 1;
	
	while(!quit_flag){
		if (poll(fds, MAX_CNX,-1) == -1){
			perror("Poll\n");
			exit(EXIT_FAILURE);
		}
		if ((fds[0].revents & POLLIN) == POLLIN){
			int new_fd = accept(fds[0].fd,(struct sockaddr *)&client_addr,&len);
			if (new_fd == -1){
				perror("accept\n");
				exit(EXIT_FAILURE);
			}
			printf("Client %d connected, sockfd = %d :\n",client_num,new_fd);
			addClient(&client_list, client_num, new_fd, client_addr);
			client_num++;
			
			int cpt;
			for (cpt=1;cpt<MAX_CNX;cpt++){
				if (fds[cpt].fd == 0){
					
					fds[cpt].fd = new_fd;
					fds[cpt].events = POLLIN;
					fds[cpt].revents = 0;
					break;
				}
			}
		}
		for (i=0;i<MAX_CNX;i++){
			if ((fds[i].fd != fds[0].fd) && ((fds[i].revents & POLLIN) == POLLIN)){
				
                struct Client *current = client_list;
                while (current != NULL) {
                    if (current->sockfd == fds[i].fd) {
                        echo_server(fds[i].fd,current);
						if (quit){
							fds[i].events = 0;
						}
                        break; 
                    }
                    current = current->nextclient;
                }

				fds[i].revents = 0;
			}
		}
	}
}


int main(int argc, char*argv[]) {
	if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
	sfd = handle_bind(argv);
	if ((listen(sfd, SOMAXCONN)) != 0) {
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}
	signal(SIGINT, handle_sigint);

	handle_multipleclients(sfd);
	return EXIT_SUCCESS;
}