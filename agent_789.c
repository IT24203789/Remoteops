#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

#define PORT 9410
#define BUFFER_SIZE 4096

#define AUTH_TOKEN "OPS-3789"
#define SID "SID:9873"

void *handle_client(void *arg);
int recv_line(int sockfd, char *buffer, int maxlen);
void send_response(int sockfd, const char *message);

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    printf("=====================================\n");
    printf(" RemoteOps Agent - IT24103789\n");
    printf(" Listening Port : %d\n", PORT);
    printf(" Session ID     : %s\n", SID);
    printf("=====================================\n");

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;

    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0) {

        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[+] Agent started successfully\n");
    printf("[+] Waiting for Controller connections...\n");

    while (1) {

        client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len
        );

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf(
            "[+] Controller connected from %s:%d\n",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port)
        );

        int *client_socket = malloc(sizeof(int));

        if (client_socket == NULL) {
            close(client_fd);
            continue;
        }

        *client_socket = client_fd;

        pthread_t thread_id;

        if (pthread_create(
                &thread_id,
                NULL,
                handle_client,
                client_socket
            ) != 0) {

            perror("pthread_create");
            close(client_fd);
            free(client_socket);
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_fd);

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

void send_response(int sockfd, const char *message) {

    char response[BUFFER_SIZE];

    snprintf(
        response,
        sizeof(response),
        "%s\n",
        message
    );

    send(
        sockfd,
        response,
        strlen(response),
        0
    );
}

void *handle_client(void *arg) {

    int client_fd = *((int *)arg);

    free(arg);

    char buffer[BUFFER_SIZE];

    int authenticated = 0;

    while (1) {

        int result = recv_line(
            client_fd,
            buffer,
            sizeof(buffer)
        );

        if (result <= 0) {
            printf("[-] Controller disconnected\n");
            break;
        }

        printf("[COMMAND] %s\n", buffer);

        if (!authenticated) {

            if (strncmp(buffer, "AUTH ", 5) == 0) {

                char *token = buffer + 5;

                if (strcmp(token, AUTH_TOKEN) == 0) {

                    authenticated = 1;

                    send_response(
                        client_fd,
                        "OK AUTHENTICATED SID:9873"
                    );

                } else {

                    send_response(
                        client_fd,
                        "ERR 001 AUTH_FAILED SID:9873"
                    );
                }

            } else {

                send_response(
                    client_fd,
                    "ERR 001 AUTH_REQUIRED SID:9873"
                );
            }

            continue;
        }

        if (strcmp(buffer, "QUIT") == 0) {

            send_response(
                client_fd,
                "OK BYE SID:9873"
            );

            break;
        }

        send_response(
            client_fd,
            "ERR 003 UNKNOWN_COMMAND SID:9873"
        );
    }

    close(client_fd);

    printf("[-] Client session closed\n");

    return NULL;
}
