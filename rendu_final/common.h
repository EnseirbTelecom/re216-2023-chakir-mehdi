#define MSG_LEN 1024
#define SERV_PORT "8080"
#define SERV_ADDR "127.0.0.1"
#define CLIENT_PORT "8081"
#define CLIENT_ADDR "127.0.0.1"

int handle_bind_client(const char *client_port) {
    struct addrinfo hints, *result, *rp;
    int sfd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, client_port, &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            continue;
        }

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
            perror("setsockopt()");
            exit(EXIT_FAILURE);
        }

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
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

int handle_connect_client(const char *client_address, const char *client_port) {
    struct addrinfo hints, *result, *rp;
    int sfd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(client_address, client_port, &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
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

void sendFile(int sockfd, char *filename, struct message *msgstruct) {
    char data[MSG_LEN] = {0};

    FILE *fic = fopen(filename, "rb");
    if (fic == NULL) {
        perror("Opening the File");
        exit(EXIT_FAILURE);
    }

    strcpy(msgstruct->infos, filename);
    msgstruct->type = FILE_SEND;

    // Envoi de la structure d'abord
    if (send(sockfd, msgstruct, sizeof(struct message), 0) <= 0) {
        perror("Error sending msgstruct");
        exit(EXIT_FAILURE);
    }

    // Envoi du fichier par morceaux
    size_t bytes;
    while ((bytes = fread(data, 1, MSG_LEN, fic)) > 0) {
        // Envoi de chaque morceau du fichier
        if (send(sockfd, data, bytes, 0) == -1) {
            perror("Couldn't send the file");
            exit(EXIT_FAILURE);
        }

        memset(data, 0, MSG_LEN);
    }

    fclose(fic);
}

void createDirectory(char *path) {
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        if (mkdir(path, 0700) == -1) {
            perror("Error creating directory");
            exit(EXIT_FAILURE);
        }
    } else {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "%s is not a directory\n", path);
            exit(EXIT_FAILURE);
        }
    }
}

void receiveFile(int sockfd, char *filename) {
    char data[MSG_LEN] = {0};

    FILE *fic = fopen(filename, "wb");
    if (fic == NULL) {
        perror("Opening the File");
        exit(EXIT_FAILURE);
    }

    size_t bytes;
    while ((bytes = recv(sockfd, data, MSG_LEN, 0)) > 0) {
        if (fwrite(data, 1, bytes, fic) != bytes) {
            perror("Error writing to file");
            exit(EXIT_FAILURE);
        }

        memset(data, 0, MSG_LEN);
    }

    fclose(fic);
}
