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
	char channel_name[NICK_LEN];
    struct Client *nextclient;
};

struct Channel{
	char channel_name[NICK_LEN];
	int nb_clients;
	struct Channel *nextchannel;
};

struct Client *client_list = NULL; // ici on stocke les descripteurs de fichiers, adresses et ports
struct Channel *channel_list = NULL;

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

struct Channel *findChannel(char *channel_name,struct Channel *channel_list){
	if (channel_list == NULL) {
        return NULL; 
    }
    if (strcmp(channel_list->channel_name,channel_name) == 0){
        return channel_list;
    }
    return findChannel(channel_name,channel_list->nextchannel); 
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

int channelnameExists(struct Channel *channel_list, char *channel_name) {
    while (channel_list != NULL) {
        if (strcmp(channel_list->channel_name, channel_name) == 0) {
            return 1;   
        }
        channel_list = channel_list->nextchannel;
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
	strcpy(client->channel_name,"");
    *client_list = client;
}

void addChannel(struct Channel **channel_list, int nb_clients, char *channel_name) {
    struct Channel *channel = malloc(sizeof(struct Channel));
    if (channel == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    channel->nb_clients = nb_clients;
    strcpy(channel->channel_name,channel_name);
    channel->nextchannel = *channel_list;
    *channel_list = channel;
}

void freeClients(struct Client *first) {
    while (first != NULL) {
        struct Client *clean_client = first;
        first = first->nextclient;
        close(clean_client->sockfd);
        free(clean_client);           
    }
}

void freeChannels(struct Channel *first) {
    while (first != NULL) {
        struct Channel *clean_channel = first;
        first = first->nextchannel;
        free(clean_channel);           
    }
}

void handle_sigint(int sig) {
	printf("\nReceived SIGINT. Cleaning up and exiting...\n");
	freeClients(client_list);
	freeChannels(channel_list);
	close(sfd);
    quit_flag = 1;
	exit(EXIT_SUCCESS);
}

void echo_server(int sockfd, struct Client *client_list) {
	quit = 0;
    struct message msgstruct;
    char buff[MSG_LEN];
    char to_send[MSG_LEN];
	char to_send2[MSG_LEN];
    struct Client *current = NULL;
	struct Channel *currentChannel = NULL;
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

		printf("pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);
		printf("Received: %s\n", buff);

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
		else if (msgstruct.type == MULTICAST_CREATE){
			char temp[NICK_LEN];
            strcpy(temp,msgstruct.infos);
			current = findClientBy_fd(sockfd, client_list);
			if (channelnameExists(channel_list,msgstruct.infos)) {
				snprintf(to_send, MSG_LEN, "Channel \"%s\" exists already\n", msgstruct.infos);
				strcpy(msgstruct.infos,current->channel_name);
				msgstruct.pld_len = strlen(current->channel_name);
				if (!strlen(current->channel_name)){
					strcpy(msgstruct.inChannel,"0");
				}
			}
			else{
				addChannel(&channel_list,1,msgstruct.infos);
				if (strlen(current->channel_name) > 0){
					currentChannel = findChannel(current->channel_name,channel_list);
					if (currentChannel->nb_clients > 1){
						strcpy(msgstruct.infos,currentChannel->channel_name);
						currentChannel->nb_clients--;
						struct Client *list = client_list;
						while (list != NULL) {
							if (list->sockfd != sockfd && strcmp(list->channel_name,currentChannel->channel_name) == 0) {
								snprintf(to_send, 2*MSG_LEN, "INFO > %s has quit \"%s\"\n", current->nickname,currentChannel->channel_name);	
								if (send(list->sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
									break;
								}
								if (send(list->sockfd, to_send, MSG_LEN, 0) <= 0) {
									break;
								}
							}
							list = list->nextclient;
						}
						snprintf(to_send, MSG_LEN, "You have created channel \"%s\"\n[%s] > You have joined \"%s\"\n", temp, temp, temp);
					}
					else{
						snprintf(to_send, 2*MSG_LEN, "INFO > You were the last user in this channel, \"%s\" has been destroyed\n\nYou have created channel \"%s\"\n[%s] > You have joined \"%s\"\n", current->channel_name,temp, temp, temp);
						strcpy(currentChannel->channel_name,"");
						currentChannel->nb_clients = 0;
					}
				}
				else{
					snprintf(to_send, MSG_LEN, "You have created channel \"%s\"\n[%s] > You have joined \"%s\"\n", msgstruct.infos, msgstruct.infos, msgstruct.infos);
				}
				strcpy(msgstruct.infos,temp);
				strcpy(current->channel_name, msgstruct.infos);
			}
		}
		else if (msgstruct.type == MULTICAST_JOIN){
			char temp[NICK_LEN];
            strcpy(temp,msgstruct.infos);
			current = findClientBy_fd(sockfd, client_list);
			if (!channelnameExists(channel_list,msgstruct.infos)) {
				snprintf(to_send, MSG_LEN, "Channel \"%s\" does not exist !\n", msgstruct.infos);
				strcpy(msgstruct.infos,current->channel_name);
				msgstruct.pld_len = strlen(current->channel_name);
				if (!strlen(current->channel_name)){
					strcpy(msgstruct.inChannel,"0");
				}
			}
			else{
				if (strcmp(current->channel_name,msgstruct.infos)==0){
					snprintf(to_send,2*MSG_LEN,"You already are in channel \"%s\"\n",msgstruct.infos);
				}
				else{
					currentChannel = findChannel(msgstruct.infos,channel_list);
					currentChannel->nb_clients++;
					struct Client *list = client_list;
					
					while (list != NULL) {
						if (list->sockfd != sockfd && strcmp(list->channel_name,currentChannel->channel_name) == 0) {
							snprintf(to_send2, 2*MSG_LEN, "INFO > %s has joined \"%s\"\n",current->nickname,currentChannel->channel_name);	
							if (send(list->sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
								break;
							}
							if (send(list->sockfd, to_send2, MSG_LEN, 0) <= 0) {
								break;
							}
						}
						list = list->nextclient;
					}
				}

				if (strlen(current->channel_name) > 0){
					if (strcmp(current->channel_name,msgstruct.infos)==0){
						snprintf(to_send,2*MSG_LEN,"You already are in channel \"%s\"\n",msgstruct.infos);
					}
					else{
						currentChannel = findChannel(current->channel_name,channel_list);
						strcpy(msgstruct.infos,currentChannel->channel_name);
						
						if (currentChannel->nb_clients > 1){

							currentChannel->nb_clients--;
							struct Client *list = client_list;
							
							while (list != NULL) {
								if (list->sockfd != sockfd && strcmp(list->channel_name,currentChannel->channel_name) == 0) {
									snprintf(to_send2, 2*MSG_LEN, "INFO > %s has quit \"%s\"\n", current->nickname,currentChannel->channel_name);	
									if (send(list->sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
										break;
									}
									if (send(list->sockfd, to_send2, MSG_LEN, 0) <= 0) {
										break;
									}
								}
								list = list->nextclient;
							}
							snprintf(to_send, MSG_LEN, "[%s] > INFO > You have joined \"%s\"\n", temp, temp);
						}
						else{
							snprintf(to_send, 2*MSG_LEN, "INFO > You were the last user in this channel, \"%s\" has been destroyed\n\n[%s] > You have joined \"%s\"\n", current->channel_name, temp, temp);
							strcpy(currentChannel->channel_name,"");
							currentChannel->nb_clients = 0;
						}
					}
				}
				else{
					snprintf(to_send, MSG_LEN, "[%s] > INFO > You have joined \"%s\"\n", msgstruct.infos, msgstruct.infos);
				}
				strcpy(msgstruct.infos,temp);
				strcpy(current->channel_name, msgstruct.infos);
			}
		}

		else if(msgstruct.type == MULTICAST_SEND && strcmp(buff,"/quit\n")!=0){
			current = findClientBy_fd(sockfd,client_list);

			struct Client *list = client_list;						
				while (list != NULL) {
					if (list->sockfd != sockfd && strcmp(list->channel_name,current->channel_name) == 0) {
						snprintf(to_send2, 2*MSG_LEN, "%s> %s\n", current->nickname,buff);	
						if (send(list->sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
							break;
						}
						if (send(list->sockfd, to_send2, MSG_LEN, 0) <= 0) {
							break;
						}
					}
					list = list->nextclient;
				}

			if (send(sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
				break;
			}
			if (send(sockfd, "\n", MSG_LEN, 0) <= 0) {
				break;
			}
		}

		else if (msgstruct.type == MULTICAST_QUIT){
			current = findClientBy_fd(sockfd, client_list);
			if (strcmp(current->channel_name,msgstruct.infos)!=0){
				snprintf(to_send, 2*MSG_LEN, "INFO> You are not in channel \"%s\". Try again using /quit %s\n", msgstruct.infos,current->channel_name);
			}
			else{
				currentChannel = findChannel(current->channel_name,channel_list);
						
				if (currentChannel->nb_clients > 1){

					currentChannel->nb_clients--;
					struct Client *list = client_list;
					
					while (list != NULL) {
						if (list->sockfd != sockfd && strcmp(list->channel_name,currentChannel->channel_name) == 0) {
							snprintf(to_send2, 2*MSG_LEN, "INFO > %s has quit \"%s\"\n", current->nickname,currentChannel->channel_name);	
							if (send(list->sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
								break;
							}
							if (send(list->sockfd, to_send2, MSG_LEN, 0) <= 0) {
								break;
							}
						}
						list = list->nextclient;
					}
					snprintf(to_send, MSG_LEN, "[%s] > INFO > You have quit \"%s\"\n", msgstruct.infos, msgstruct.infos);
				}
				else{
					snprintf(to_send, 2*MSG_LEN, "INFO > You were the last user in this channel, \"%s\" has been destroyed\n\n", current->channel_name);
					strcpy(currentChannel->channel_name,"");
					currentChannel->nb_clients = 0;
				}
				strcpy(current->channel_name,"");
				strcpy(msgstruct.inChannel,"0");
			}
		}

		else if ( msgstruct.type == MULTICAST_LIST){
			struct Channel *channel = channel_list;
			strcpy(to_send, "Channels available are : \n");
			while (channel != NULL ) {
				if (strcmp(channel->channel_name,"")!=0){
					strcat(to_send, "			- ");
					strcat(to_send, channel->channel_name);
					strcat(to_send, "\n");
				}
				channel = channel->nextchannel;
			}
		}

		// if received message is /quit
        if (strcmp(buff, "/quit\n") == 0) {
			quit = 1;
			usr = findClientBy_fd(sockfd, client_list);

			if (strlen(usr->channel_name) > 0){
				currentChannel = findChannel(usr->channel_name,channel_list);
					if (currentChannel->nb_clients > 1){
						currentChannel->nb_clients--;
						struct Client *list = client_list;
						while (list != NULL) {
							if (list->sockfd != sockfd && strcmp(list->channel_name,currentChannel->channel_name) == 0) {
								snprintf(to_send, 2*MSG_LEN, "INFO > %s has disconnected from the server, and therefore has quit channel \"%s\"\n", usr->nickname,currentChannel->channel_name);	
								if (send(list->sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
									break;
								}
								if (send(list->sockfd, to_send, MSG_LEN, 0) <= 0) {
									break;
								}
							}
							list = list->nextclient;
						}
					}
					else{
						strcpy(currentChannel->channel_name,"");
						currentChannel->nb_clients = 0;
					}

			}
			printf("Client \"%s\" disconnected.\n",usr->nickname);
			strcpy(usr->nickname,"");
			close(sockfd);
            break; 
        }
		
		if (msgstruct.type != BROADCAST_SEND && msgstruct.type != MULTICAST_SEND){
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