#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>

#include "msg_struct.h"
#include "common.h"
 
int clientInChannel = 0;  // Statut du client : = 1 s'il est dans un salon à priori et 0 sinon
char channel_name[MSG_LEN]; // on stocke dans cette variable le nom du salon 

int is_valid_nickname(char *nickname){
    int index = 0;

     if (strlen(nickname) > NICK_LEN) {
        printf("Error: Pseudo is too long (maximum length is %d).\n", NICK_LEN);
        return 0; 
    }

    while (nickname[index] != '\0' && index < NICK_LEN) {
        if (!isalnum(nickname[index])) {
            printf("Error: Type ONLY letters or numbers please.\n");
            return 0; 
        }
        index++;
    }

    return 1;
}

int initMsgStruct(struct message *msgstruct, char *buff){
    strcpy(msgstruct->inChannel,"");
    int isvalid = 1;
    if (buff[0]=='/'){
        char clone[MSG_LEN];
        strcpy(clone,buff);
        char *command = strtok(clone," ");
        if (strcmp(command,"/nick")==0){
            char *new_buff = strtok(NULL,"\n");
            
            if (is_valid_nickname(new_buff)){
                msgstruct->pld_len = strlen(new_buff);
                msgstruct->type = NICKNAME_NEW;           
                strcpy(msgstruct->nick_sender,new_buff);
                strcpy(msgstruct->infos,new_buff);
            }
            else{
                isvalid = 0;
            }

        }
        else if (strcmp(command,"/who\n")==0){

            msgstruct->type = NICKNAME_LIST;
            msgstruct->pld_len = 0;
            strcpy(msgstruct->infos,"");
        }
        else if (strcmp(command,"/whois")==0){
            char *new_buff = strtok(NULL,"\n");
            msgstruct->type = NICKNAME_INFOS;
            msgstruct->pld_len = strlen(new_buff);
            strcpy(msgstruct->infos,new_buff);
        }
        else if (strcmp(command,"/msg")==0){
            char *new_buff = strtok(NULL," ");

            msgstruct->pld_len = strlen(new_buff);
            msgstruct->type = UNICAST_SEND;
            strcpy(msgstruct->infos,new_buff);
        }
        else if (strcmp(command,"/msgall")==0){
            msgstruct->pld_len = 0;
            msgstruct->type = BROADCAST_SEND;
            strcpy(msgstruct->infos,"");
        }
        else if (strcmp(command,"/create")==0 && strlen(msgstruct->nick_sender)>0){
            char *new_buff = strtok(NULL,"\n");
            
            if (is_valid_nickname(new_buff)){
                msgstruct->pld_len = strlen(new_buff);
                msgstruct->type = MULTICAST_CREATE;
                strcpy(msgstruct->infos,new_buff);
                strcpy(channel_name,new_buff);
                clientInChannel = 1;
            }
            else{
                isvalid = 0;
            }
        }
        else if (strcmp(command,"/channel_list\n")==0){
            msgstruct->pld_len = 0;
            msgstruct->type = MULTICAST_LIST;
            strcpy(msgstruct->infos,"");
        }
        else if (strcmp(command,"/join")==0 && strlen(msgstruct->nick_sender)>0){
            char *new_buff = strtok(NULL,"\n");

            msgstruct->type = MULTICAST_JOIN;

            msgstruct->pld_len = strlen(new_buff);
            strcpy(msgstruct->infos,new_buff);
            strcpy(channel_name,new_buff);
            clientInChannel = 1;
        }
        else if (strcmp(command,"/quit")==0){
            if (clientInChannel){
                char *new_buff = strtok(NULL,"\n");

                msgstruct->pld_len = strlen(new_buff);
                msgstruct->type = MULTICAST_QUIT;
                strcpy(msgstruct->infos,new_buff);
            }
            else{
                msgstruct->pld_len = strlen(buff);
                msgstruct->type = ECHO_SEND;
                strcpy(msgstruct->infos,"");
            }
        }
        else {
            if (clientInChannel){
                msgstruct->type = MULTICAST_SEND;
                msgstruct->pld_len = strlen(channel_name);
                strcpy(msgstruct->infos,channel_name);
            }
            else{
                msgstruct->pld_len = strlen(buff);
                msgstruct->type = ECHO_SEND;
                strcpy(msgstruct->infos,"");
            }
        }
    }
    else {
        if (clientInChannel){
            msgstruct->type = MULTICAST_SEND;
            msgstruct->pld_len = strlen(channel_name);
            strcpy(msgstruct->infos,channel_name);
        }
        else{
            msgstruct->pld_len = strlen(buff);
            msgstruct->type = ECHO_SEND;
            strcpy(msgstruct->infos,"");
        }
    }
    return isvalid;
}


void echo_client(int sockfd) {
    struct message msgstruct;
    char buff[MSG_LEN];
    struct pollfd fds[2];
    int n;

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    if (recv(sockfd, buff, MSG_LEN, 0) <= 0) {
        return;
    }
    printf("%s",buff);
    printf(">> ");
    fflush(stdout);
    memset(&msgstruct, 0, sizeof(struct message));
    while (1) {
        
        poll(fds, 2, -1);

        if (fds[0].revents & POLLIN) {
            // Cleaning memory
            memset(buff, 0, MSG_LEN);
            // Getting message from client
            n=0;

            char old_nickname[NICK_LEN];

            if (strlen(msgstruct.nick_sender) > 0){
                strcpy(old_nickname,msgstruct.nick_sender);
            }
            
            while ((buff[n++] = getchar()) != '\n') {} // trailing '\n' will be sent
            // Filling structure
            if (!initMsgStruct(&msgstruct,buff)){
                printf("\n>> ");
                fflush(stdout);
                continue;
            }
            if (strlen(msgstruct.nick_sender) > 0){
                if (send(sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
                    break;
                }

                if (msgstruct.type == ECHO_SEND || msgstruct.type == NICKNAME_LIST || msgstruct.type == NICKNAME_INFOS || msgstruct.type == MULTICAST_LIST || msgstruct.type == MULTICAST_SEND || msgstruct.type == MULTICAST_CREATE || msgstruct.type == MULTICAST_JOIN || msgstruct.type == MULTICAST_QUIT ){
                    if (send(sockfd, buff,  strlen(buff), 0) <= 0) {
                        break;
                    }
                    if (strcmp(buff, "/quit\n") == 0) {
                        printf("Message sent. Disconnecting...\n");
                        break;
                    }
                }

                else if (msgstruct.type == NICKNAME_NEW){
                    if (send(sockfd, old_nickname,  NICK_LEN, 0) <= 0) {
                        break;
                    }
                }
    
                else if (msgstruct.type == BROADCAST_SEND){
                    strtok(buff," ");
                    char *new_buff = strtok(NULL,"");
                    if (send(sockfd, new_buff,  strlen(new_buff), 0) <= 0) {
                        break;
                    }
                }
                else if(msgstruct.type == UNICAST_SEND){
                    char clone[MSG_LEN];
                    strcpy(clone,buff);
                    strtok(clone," ");
                    strtok(NULL," ");
                    char *new_buff = strtok(NULL,"");
                    if (send(sockfd, new_buff,  strlen(new_buff), 0) <= 0) {
                        break;
                    }
                }

                printf("Message sent!\n");
            }
            else if (strlen(msgstruct.nick_sender) == 0 && strcmp(buff,"/quit\n")==0){
                if (send(sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
                    break;
                }
                if (send(sockfd, buff,  strlen(buff), 0) <= 0) {
                        break;
                    }
                printf("Message sent. Disconnecting...\n");
                break;
            }
            else{
                printf("Please enter your nickname first by using /nick <your pseudo>\n");
                printf("\n>> ");
                fflush(stdout);
                continue;
            }
        }

        if (fds[1].revents & POLLIN) {
            char client_nickname[NICK_LEN];
            strcpy(client_nickname,msgstruct.nick_sender);
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

            if(msgstruct.type == MULTICAST_CREATE || msgstruct.type == MULTICAST_JOIN || msgstruct.type == MULTICAST_QUIT){
                if (!strcmp(msgstruct.inChannel,"0")){
                    clientInChannel = 0;
                }
            }
            if(msgstruct.type == MULTICAST_CREATE || msgstruct.type == MULTICAST_JOIN){
                strcpy(channel_name,msgstruct.infos);
            }

            if (strcmp(msgstruct.nick_sender,client_nickname)== 0 &&  msgstruct.type != MULTICAST_LIST && msgstruct.type != MULTICAST_SEND && msgstruct.type != MULTICAST_CREATE && msgstruct.type != MULTICAST_JOIN && msgstruct.type != MULTICAST_QUIT ){
                printf("pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);
            }
            printf("%s\n", buff);
            if (clientInChannel){
                printf("[%s] > ",channel_name);
            }
            else{ 
                printf(">> ");
            }
            fflush(stdout);
            if (msgstruct.type != NICKNAME_NEW){
                strcpy(msgstruct.nick_sender,client_nickname);
            }
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
    printf("Connecting to server ... done!\n");
    echo_client(sfd);
    close(sfd);
    return EXIT_SUCCESS;
}