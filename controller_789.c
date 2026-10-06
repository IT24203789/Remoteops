#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BUFFER_SIZE 4096

int recv_line(int sockfd, char *buffer, int maxlen);

int main(int argc, char *argv[]) {

    int sockfd;

    struct sockaddr_in server_addr;

    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    char *server_ip = "127.0.0.1";

    if (argc >= 2) {
        server_ip = argv[1];
    }

    printf("=====================================\n");
    printf(" RemoteOps Controller - IT24103789\n");
    printf(" Target Agent: %s:%d\n", server_ip, PORT);
    printf("=====================================\n");

    sockfd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(
            AF_INET,
            server_ip,
            &server_addr.sin_addr
        ) <= 0) {

        printf("Invalid IP address\n");

        close(sockfd);

        return 1;
    }

    if (connect(
            sockfd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0) {

        perror("connect");

        close(sockfd);

        return 1;
    }

    printf("[+] Connected to RemoteOps Agent\n");
    printf("[+] Type commands below\n\n");

    while (1) {

        printf("RemoteOps> ");

        fflush(stdout);

        if (fgets(
                command,
                sizeof(command),
                stdin
            ) == NULL) {

            break;
        }

        send(
            sockfd,
            command,
            strlen(command),
            0
        );

        int result = recv_line(
            sockfd,
            response,
            sizeof(response)
        );

        if (result <= 0) {

            printf("[-] Agent disconnected\n");

            break;
        }

        printf("%s\n", response);

        if (strncmp(
                response,
                "OK BYE",
                6
            ) == 0) {

            break;
        }
    }

    close(sockfd);

    return 0;
}

int recv_line(int sockfd, char *buffer, int maxlen) {

    int index = 0;

    while (index < maxlen - 1) {

        char c;

        int received = recv(
            sockfd,
            &c,
            1,
            0
        );

        if (received == 0) {
            return 0;
        }

        if (received < 0) {
            return -1;
        }

        if (c == '\n') {
            break;
        }

        if (c != '\r') {
            buffer[index++] = c;
        }
    }

    buffer[index] = '\0';

    return index;
}
