#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <signal.h>

#include "msg_struct.h"
#include "common.h"

#define MAX_CNX 256

int quit;
int quit_flag = 0;
int sfd; 

struct Client{
    int sockfd;
    int client_num;
    int port;
	char nickname[NICK_LEN];
    char client_addr[16];
    struct Client *nextclient;
};

struct Client *client_list = NULL; // ici on stocke les descripteurs de fichiers, adresses et ports

struct Client *findClientBy_fd(int sockfd,struct Client *client_list){
  if (client_list->sockfd==sockfd){
    return client_list;
  }
  return findClientBy_fd(sockfd,client_list->nextclient);
}

struct Client *findClientBy_nickname(char *nickname,struct Client *client_list){
	if (client_list == NULL) {
        return NULL; 
    }
    if (strcmp(client_list->nickname,nickname) == 0){
        return client_list;
    }
    return findClientBy_nickname(nickname,client_list->nextclient); 
}

int nicknameExists(struct Client *client_list, char *nickname) {
    while (client_list != NULL) {
        if (strcmp(client_list->nickname, nickname) == 0) {
            return 1;   
        }
        client_list = client_list->nextclient;
    }
    return 0; 
}


void addClient(struct Client **client_list, int client_num, int sockfd) {
    struct Client *client = malloc(sizeof(struct Client));
    if (client == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

	struct sockaddr_in infos;
    socklen_t len = sizeof(infos);

	if (getpeername(sockfd, (struct sockaddr*)&infos, &len) == -1) {
        perror("getpeername");
        exit(EXIT_FAILURE);
    }

    client->sockfd = sockfd;
    client->client_num = client_num;
    
	client->port=ntohs(infos.sin_port);
    inet_ntop(AF_INET, &(infos.sin_addr), client->client_addr, INET_ADDRSTRLEN);

    client->nextclient = *client_list;
    strcpy(client->nickname,"");
    *client_list = client;
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

void echo_server(int sockfd, struct Client *client_list) {
	quit = 0;
    struct message msgstruct;
    char buff[MSG_LEN];
    char to_send[MSG_LEN];
    struct Client *current = NULL;
	struct Client *usr=NULL;
    int sock = sockfd;
	while (1) {
		// Cleaning memory
		memset(&msgstruct, 0, sizeof(struct message));
		memset(buff, 0, MSG_LEN);
        // Receiving structure
		if (recv(sockfd, &msgstruct, sizeof(struct message), 0) <= 0) {
			break;
		}
		// Receiving message
		if (recv(sockfd, buff, MSG_LEN, 0) <= 0) {
			break;
		}

        if (msgstruct.type == NICKNAME_NEW) {
			if (nicknameExists(client_list,msgstruct.infos)) {
				snprintf(to_send, MSG_LEN, "User \"%s\" exits already\n", msgstruct.infos);
				strcpy(msgstruct.nick_sender,buff);
			}
			else{
				current = findClientBy_fd(sockfd, client_list);
				strcpy(current->nickname, msgstruct.nick_sender);
				strcpy(to_send, "Welcome to chat : ");
				strcat(to_send, current->nickname);
				strcat(to_send, "\n");
			}
		} else if (msgstruct.type == NICKNAME_LIST) {
			struct Client *list = client_list;
			strcpy(to_send, "Online users are : \n");
			while (list != NULL ) {
				if (strcmp(list->nickname,"")!=0){
					strcat(to_send, "			- ");
					strcat(to_send, list->nickname);
					strcat(to_send, "\n");
				}
				list = list->nextclient;
			}
		} else if (msgstruct.type == NICKNAME_INFOS) {
			memset(to_send, 0, MSG_LEN);
			if (!nicknameExists(client_list,msgstruct.infos)) {
				snprintf(to_send, MSG_LEN, "User \"%s\" does not exist\n", msgstruct.infos);
			} else {
				current = findClientBy_nickname(msgstruct.infos, client_list);
				char Port[6];
				sprintf(Port, "%d", current->port);

				time_t t;
				struct tm *tm_info;
				time(&t);
				tm_info = localtime(&t);
				char date_time[80];
				strftime(date_time, 80, "%Y/%m/%d@%H:%M", tm_info);
				
				strcat(to_send,"[Server] : ");
				strcat(to_send, msgstruct.infos);
				strcat(to_send, " connected since ");
        		strcat(to_send, date_time);
				strcat(to_send," with IP address ");
				strcat(to_send, current->client_addr);
				strcat(to_send, " and port number ");
				strcat(to_send, Port);
				strcat(to_send, "\n");
			}
		} else if (msgstruct.type == UNICAST_SEND) {
			if (!nicknameExists(client_list,msgstruct.infos)) {
				snprintf(to_send, 2*MSG_LEN, "User \"%s\" does not exist\n", msgstruct.infos);
			} else {
				usr = findClientBy_nickname(msgstruct.infos, client_list);
				current = findClientBy_fd(sockfd,client_list);
				sock = usr->sockfd;
				snprintf(to_send, 2*MSG_LEN, "[%s] : %s", current->nickname, buff);
				if (send(sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
					break;
				}
				char inform[MSG_LEN];
				sprintf(inform, "Message sent successfully to client \"%s\"\n", msgstruct.infos);
				if (send(sockfd, inform, MSG_LEN, 0) <= 0) {
				break;
			}
			}
		} else if (msgstruct.type == ECHO_SEND) {
			memset(to_send, 0, MSG_LEN);
			strcat(to_send, "Received : ");
			strcat(to_send,buff);
		} else if (msgstruct.type == BROADCAST_SEND) {
			struct Client *list = client_list;
			current = findClientBy_fd(sockfd,client_list);
			while (list != NULL) {
				if (list->sockfd != sockfd && strcmp(list->nickname,"")!=0) {
					snprintf(to_send, 2*MSG_LEN, "[%s] : %s", current->nickname, buff);	
					if (send(list->sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
						break;
					}
					if (send(list->sockfd, to_send, MSG_LEN, 0) <= 0) {
						break;
					}
				}
				list = list->nextclient;
			}
			if (send(sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
				break;
			}
			if (send(sockfd, "Message sent successfully to all clients\n", MSG_LEN, 0) <= 0) {
				break;
			}
		}

		// if received message is /quit
        if (strcmp(buff, "/quit\n") == 0) {
			quit = 1;
			usr = findClientBy_fd(sockfd, client_list);
			printf("Client \"%s\" disconnected.\n",usr->nickname);
			strcpy(usr->nickname,"");
			close(sockfd);
            break; 
        }
        printf("pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);
		printf("Received: %s\n", buff);
		
		if (msgstruct.type != BROADCAST_SEND){
			// Sending structure (ECHO)
			if (send(sock, &msgstruct, sizeof(msgstruct), 0) <= 0) {
				break;
			}
			// Sending message (ECHO)
			if (send(sock, to_send, MSG_LEN, 0) <= 0) {
				break;
			}
		}

		printf("Response sent\n\n");
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
			int new_fd = accept(fds[0].fd,(struct sockaddr*)&client_addr,&len);
			if (new_fd == -1){
				perror("accept\n");
				exit(EXIT_FAILURE);
			}
			printf("Client %d connected, sockfd = %d :\n",client_num,new_fd);
			if (send(new_fd,"[Server] : please login with /nick <your pseudo>\n\n",MSG_LEN,0)<=0){
				break;
			}
			addClient(&client_list, client_num, new_fd);
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
				
                echo_server(fds[i].fd,client_list);
				if (quit){
					fds[i].events = 0;
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