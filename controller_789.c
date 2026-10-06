#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define PORT 9410
#define BUFFER_SIZE 4096
#define MAX_FILE_SIZE (10LL * 1024 * 1024)

int recv_line(int sockfd, char *buffer, int maxlen);

int send_all(
    int sockfd,
    const void *buffer,
    size_t length
);

int handle_put(
    int sockfd,
    const char *filepath
);

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

        /*
         * Remove newline for local command parsing.
         */
        command[
            strcspn(
                command,
                "\r\n"
            )
        ] = '\0';


        /*
         * PUT is handled specially because the
         * Controller must transmit raw file bytes
         * after the text command.
         */
        if (strncmp(
                command,
                "PUT ",
                4
            ) == 0) {

            const char *filepath =
                command + 4;

            while (*filepath == ' ') {
                filepath++;
            }

            if (*filepath == '\0') {

                printf(
                    "Usage: PUT <local-file-path>\n"
                );

                continue;
            }

            handle_put(
                sockfd,
                filepath
            );

            continue;
        }

/*
 * Normal text protocol command.
 * Send the command and newline separately so that
 * no temporary buffer can be truncated.
 */
if (
    send_all(
        sockfd,
        command,
        strlen(command)
    ) != 0 ||
    send_all(
        sockfd,
        "\n",
        1
    ) != 0
) {

    printf(
        "[-] Failed to send command\n"
    );

    break;
}

        int result = recv_line(
            sockfd,
            response,
            sizeof(response)
        );

        if (result <= 0) {

            printf(
                "[-] Agent disconnected\n"
            );

            break;
        }

        printf(
            "%s\n",
            response
        );


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


/* Send exactly length bytes */
int send_all(
    int sockfd,
    const void *buffer,
    size_t length
) {

    const char *data = buffer;

    size_t total = 0;

    while (total < length) {

        ssize_t sent = send(
            sockfd,
            data + total,
            length - total,
            0
        );

        if (sent < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (sent == 0) {
            return -1;
        }

        total += (size_t)sent;
    }

    return 0;
}


/* PUT local file to Agent */
int handle_put(
    int sockfd,
    const char *filepath
) {

    struct stat file_info;

    char header[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];

    FILE *file;

    if (stat(
            filepath,
            &file_info
        ) != 0) {

        printf(
            "[-] Local file not found: %s\n",
            filepath
        );

        return -1;
    }

    if (!S_ISREG(file_info.st_mode)) {

        printf(
            "[-] PUT requires a regular file\n"
        );

        return -1;
    }

    long long filesize =
        (long long)file_info.st_size;

    if (filesize > MAX_FILE_SIZE) {

        printf(
            "[-] File too large. Maximum is 10 MB\n"
        );

        return -1;
    }


    /*
     * Send only the base filename to the Agent.
     */
    const char *filename =
        strrchr(
            filepath,
            '/'
        );

    if (filename != NULL) {
        filename++;
    } else {
        filename = filepath;
    }


    if (strlen(filename) == 0) {

        printf(
            "[-] Invalid filename\n"
        );

        return -1;
    }


    file = fopen(
        filepath,
        "rb"
    );

    if (file == NULL) {

        perror("fopen");

        return -1;
    }


    snprintf(
        header,
        sizeof(header),
        "PUT %s %lld\n",
        filename,
        filesize
    );


    if (send_all(
            sockfd,
            header,
            strlen(header)
        ) != 0) {

        printf(
            "[-] Failed to send PUT header\n"
        );

        fclose(file);

        return -1;
    }


    long long total_sent = 0;

    while (total_sent < filesize) {

        size_t amount = fread(
            file_buffer,
            1,
            sizeof(file_buffer),
            file
        );

        if (amount == 0) {

            if (ferror(file)) {

                printf(
                    "[-] Failed while reading local file\n"
                );

                fclose(file);

                return -1;
            }

            break;
        }


        if (send_all(
                sockfd,
                file_buffer,
                amount
            ) != 0) {

            printf(
                "[-] Failed while sending file\n"
            );

            fclose(file);

            return -1;
        }

        total_sent +=
            (long long)amount;
    }

    fclose(file);


    if (total_sent != filesize) {

        printf(
            "[-] File transfer incomplete\n"
        );

        return -1;
    }


    int result = recv_line(
        sockfd,
        response,
        sizeof(response)
    );

    if (result <= 0) {

        printf(
            "[-] Agent disconnected during PUT\n"
        );

        return -1;
    }


    printf(
        "%s\n",
        response
    );

    return 0;
}


/* Receive one protocol line */
int recv_line(
    int sockfd,
    char *buffer,
    int maxlen
) {

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

            if (errno == EINTR) {
                continue;
            }

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
