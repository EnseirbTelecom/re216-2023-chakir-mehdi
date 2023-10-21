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

int is_valid_nickname(struct message msgstruct){
    int index = 0;

     if (strlen(msgstruct.nick_sender) > NICK_LEN) {
        printf("Error: Pseudo is too long (maximum length is %d).\n", NICK_LEN);
        return 0; 
    }

    while (msgstruct.nick_sender[index] != '\0' && index < NICK_LEN) {
        if (!isalnum(msgstruct.nick_sender[index])) {
            printf("Error: Type ONLY letters or numbers please.\n");
            return 0; 
        }
        index++;
    }

    return 1;
}

void initMsgStruct(struct message *msgstruct, char *buff){
    if (buff[0]=='/'){
        char clone[MSG_LEN];
        strcpy(clone,buff);
        char *command = strtok(clone," ");
        if (strcmp(command,"/nick")==0){
            char *new_buff = strtok(NULL,"\n");
            
            msgstruct->pld_len = strlen(new_buff);
            msgstruct->type = NICKNAME_NEW;           
            strcpy(msgstruct->nick_sender,new_buff);
            strcpy(msgstruct->infos,new_buff);

        }
        else if (strcmp(command,"/who\n")==0){

            msgstruct->type = NICKNAME_LIST;
            msgstruct->pld_len = strlen(buff);
            strcpy(msgstruct->infos,"");
        }
        else if (strcmp(command,"/whois")==0){
            char *new_buff = strtok(NULL,"\n");
            msgstruct->type = NICKNAME_INFOS;
            msgstruct->pld_len = strlen(buff);
            strcpy(msgstruct->infos,new_buff);
        }
        else if (strcmp(command,"/msg")==0){
            char *new_buff = strtok(NULL," ");
            char *msg = strtok(NULL,"");

            msgstruct->pld_len = strlen(msg);
            msgstruct->type = UNICAST_SEND;
            strcpy(msgstruct->infos,new_buff);
        }
        else if (strcmp(command,"/msgall")==0){
            char *new_buff = strtok(NULL,"");

            msgstruct->pld_len = strlen(new_buff);
            msgstruct->type = BROADCAST_SEND;
            strcpy(msgstruct->infos,"");
        }
        else {

            msgstruct->pld_len = strlen(buff);
            msgstruct->type = ECHO_SEND;
            strcpy(msgstruct->infos,"");
        }
    }
    else {
        msgstruct->pld_len = strlen(buff);
        msgstruct->type = ECHO_SEND;
        strcpy(msgstruct->infos,"");
    }
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

    printf("[Server] : please login with /nick <your pseudo>\n");
    printf("Message: ");
    fflush(stdout);
    memset(&msgstruct, 0, sizeof(struct message));
    while (1) {
        
        poll(fds, 2, -1);

        if (fds[0].revents & POLLIN) {
            // Cleaning memory
            memset(buff, 0, MSG_LEN);
            // Getting message from client
            n=0;
            while ((buff[n++] = getchar()) != '\n') {} // trailing '\n' will be sent
            // Filling structure
            initMsgStruct(&msgstruct,buff);
            if (!is_valid_nickname(msgstruct)){
                break;
            }

            if (send(sockfd, &msgstruct, sizeof(msgstruct), 0) <= 0) {
                break;
            }

            if (msgstruct.type == ECHO_SEND){
                if (send(sockfd, buff,  msgstruct.pld_len, 0) <= 0) {
                    break;
                }
                if (strcmp(buff, "/quit\n") == 0) {
                    printf("Message sent. Disconnecting...\n");
                    break;
                }
            }
            else if (msgstruct.type == NICKNAME_NEW){
                if (send(sockfd, msgstruct.infos,  msgstruct.pld_len, 0) <= 0) {
                    break;
                }
            }
            else if (msgstruct.type == NICKNAME_LIST || msgstruct.type == NICKNAME_INFOS ){
                if (send(sockfd, buff,  msgstruct.pld_len, 0) <= 0) {
                    break;
                }
            }
            else if (msgstruct.type == BROADCAST_SEND ){
                strtok(buff," ");
                char *new_buff = strtok(NULL,"");
                if (send(sockfd, new_buff,  msgstruct.pld_len, 0) <= 0) {
                    break;
                }
            }
            else if(msgstruct.type == UNICAST_SEND){
                char clone[MSG_LEN];
                strcpy(clone,buff);
                strtok(clone," ");
                strtok(NULL," ");
                char *new_buff = strtok(NULL,"");
                if (send(sockfd, new_buff,  msgstruct.pld_len, 0) <= 0) {
                    break;
                }
            }
            printf("Message sent!\n");
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
            if (msgstruct.type != NICKNAME_NEW){
                strcpy(msgstruct.nick_sender,client_nickname);
            }
            if (msgstruct.type == NICKNAME_NEW || msgstruct.type == NICKNAME_INFOS || msgstruct.type == NICKNAME_LIST || msgstruct.type == ECHO_SEND ){
                printf("pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);
            }
            printf("Received: %s\n\n", buff);
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
    printf("Connecting to server ... done!\n");
    echo_client(sfd);
    close(sfd);
    return EXIT_SUCCESS;
}