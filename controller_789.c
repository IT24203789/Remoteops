#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/time.h>

#define PORT 9410
#define BUFFER_SIZE 4096
#define MAX_FILE_SIZE (10LL * 1024 * 1024)

static int udp_socket_fd = -1;
static int udp_listener_running = 0;
static pthread_t udp_listener_tid;
static pthread_mutex_t udp_listener_mutex = PTHREAD_MUTEX_INITIALIZER;

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
int handle_get(
    int sockfd,
    const char *filename
);

void *udp_listener_thread(void *arg);
int start_udp_listener(int udp_port);
void stop_udp_listener(void);
int send_text_command(int sockfd, const char *command, char *response, int response_size);
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
        }/*
 * GET is handled specially because a successful
 * response is followed immediately by raw file bytes.
 */
if (strncmp(
        command,
        "GET ",
        4
    ) == 0) {

    const char *filename =
        command + 4;

    while (*filename == ' ') {
        filename++;
    }

    if (*filename == '\0') {

        printf(
            "Usage: GET <filename>\n"
        );

        continue;
    }

    handle_get(
        sockfd,
        filename
    );

    continue;
}

        /* MONITOR START needs a local UDP listener before TCP request. */
        if (strncmp(command, "MONITOR START ", 14) == 0) {
            char *port_text = command + 14;
            char *endptr = NULL;
            errno = 0;

            long udp_port = strtol(port_text, &endptr, 10);

            if (
                errno != 0 ||
                endptr == port_text ||
                *endptr != '\0' ||
                udp_port < 1 ||
                udp_port > 65535
            ) {
                printf("Usage: MONITOR START <udp_port>\n");
                continue;
            }

            if (start_udp_listener((int)udp_port) != 0) {
                printf("[-] Could not start UDP listener on port %ld\n", udp_port);
                continue;
            }

            int monitor_result = send_text_command(
                sockfd,
                command,
                response,
                sizeof(response)
            );

            if (monitor_result <= 0) {
                stop_udp_listener();
                printf("[-] Agent disconnected\n");
                break;
            }

            printf("%s\n", response);

            if (strncmp(response, "OK MONITOR_STARTED", 18) != 0) {
                stop_udp_listener();
            }

            continue;
        }

        if (strcmp(command, "MONITOR STOP") == 0) {
            int monitor_result = send_text_command(
                sockfd,
                command,
                response,
                sizeof(response)
            );

            if (monitor_result <= 0) {
                stop_udp_listener();
                printf("[-] Agent disconnected\n");
                break;
            }

            printf("%s\n", response);
            stop_udp_listener();
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

    stop_udp_listener();
    close(sockfd);

    return 0;
}


/* Send a normal one-line TCP command and receive one response line. */
int send_text_command(
    int sockfd,
    const char *command,
    char *response,
    int response_size
) {
    if (
        send_all(sockfd, command, strlen(command)) != 0 ||
        send_all(sockfd, "\n", 1) != 0
    ) {
        return -1;
    }

    return recv_line(sockfd, response, response_size);
}

/* UDP listener used by MONITOR START. */
void *udp_listener_thread(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        pthread_mutex_lock(&udp_listener_mutex);
        int running = udp_listener_running;
        int sockfd = udp_socket_fd;
        pthread_mutex_unlock(&udp_listener_mutex);

        if (!running || sockfd < 0) {
            break;
        }

        ssize_t received = recvfrom(
            sockfd,
            buffer,
            sizeof(buffer) - 1,
            0,
            NULL,
            NULL
        );

        if (received > 0) {
            buffer[received] = '\0';
            printf("\n[UDP MONITOR] %s\nRemoteOps> ", buffer);
            fflush(stdout);
            continue;
        }

        if (received < 0) {
            if (
                errno == EAGAIN ||
                errno == EWOULDBLOCK ||
                errno == EINTR
            ) {
                continue;
            }
            break;
        }
    }

    return NULL;
}

int start_udp_listener(int udp_port) {
    stop_udp_listener();

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("udp socket");
        return -1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons((unsigned short)udp_port);

    if (bind(
            sockfd,
            (struct sockaddr *)&udp_addr,
            sizeof(udp_addr)
        ) < 0) {

        perror("udp bind");
        close(sockfd);
        return -1;
    }

    pthread_mutex_lock(&udp_listener_mutex);
    udp_socket_fd = sockfd;
    udp_listener_running = 1;
    pthread_mutex_unlock(&udp_listener_mutex);

    if (pthread_create(
            &udp_listener_tid,
            NULL,
            udp_listener_thread,
            NULL
        ) != 0) {

        pthread_mutex_lock(&udp_listener_mutex);
        udp_listener_running = 0;
        udp_socket_fd = -1;
        pthread_mutex_unlock(&udp_listener_mutex);

        close(sockfd);
        return -1;
    }

    printf("[+] UDP listener started on port %d\n", udp_port);
    return 0;
}

void stop_udp_listener(void) {
    pthread_mutex_lock(&udp_listener_mutex);
    int was_running = udp_listener_running;
    udp_listener_running = 0;
    pthread_mutex_unlock(&udp_listener_mutex);

    if (was_running) {
        pthread_join(udp_listener_tid, NULL);
    }

    pthread_mutex_lock(&udp_listener_mutex);
    if (udp_socket_fd >= 0) {
        close(udp_socket_fd);
        udp_socket_fd = -1;
    }
    pthread_mutex_unlock(&udp_listener_mutex);
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
/* GET file from Agent */
int handle_get(
    int sockfd,
    const char *filename
) {

    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    char output_filename[BUFFER_SIZE];

    if (
        strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL
    ) {

        printf(
            "[-] GET requires a filename only\n"
        );

        return -1;
    }

    snprintf(
        request,
        sizeof(request),
        "GET %s\n",
        filename
    );

    if (send_all(
            sockfd,
            request,
            strlen(request)
        ) != 0) {

        printf(
            "[-] Failed to send GET command\n"
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
            "[-] Agent disconnected during GET\n"
        );

        return -1;
    }

    /*
     * Error response such as:
     * ERR 005 FILE_NOT_FOUND SID:9873
     */
    if (strncmp(
            response,
            "ERR ",
            4
        ) == 0) {

        printf(
            "%s\n",
            response
        );

        return -1;
    }

    char received_filename[256];
    long long filesize;
    char sid[64];

    if (sscanf(
            response,
            "OK FILE_SEND %255s %lld %63s",
            received_filename,
            &filesize,
            sid
        ) != 3) {

        printf(
            "[-] Invalid GET response: %s\n",
            response
        );

        return -1;
    }

    if (
        filesize < 0 ||
        strcmp(sid, "SID:9873") != 0
    ) {

        printf(
            "[-] Invalid GET metadata\n"
        );

        return -1;
    }

    snprintf(
        output_filename,
        sizeof(output_filename),
        "downloaded_%s",
        received_filename
    );

    FILE *file = fopen(
        output_filename,
        "wb"
    );

    if (file == NULL) {

        perror("fopen");

        return -1;
    }

    long long remaining = filesize;

    while (remaining > 0) {

        size_t amount =
            remaining > (long long)sizeof(file_buffer)
                ? sizeof(file_buffer)
                : (size_t)remaining;

        ssize_t received = recv(
            sockfd,
            file_buffer,
            amount,
            0
        );

        if (received < 0) {

            if (errno == EINTR) {
                continue;
            }

            fclose(file);
            remove(output_filename);

            printf(
                "[-] Failed while receiving file\n"
            );

            return -1;
        }

        if (received == 0) {

            fclose(file);
            remove(output_filename);

            printf(
                "[-] Agent disconnected during file transfer\n"
            );

            return -1;
        }

        size_t written = fwrite(
            file_buffer,
            1,
            (size_t)received,
            file
        );

        if (written != (size_t)received) {

            fclose(file);
            remove(output_filename);

            printf(
                "[-] Failed to write downloaded file\n"
            );

            return -1;
        }

        remaining -= received;
    }

    fclose(file);

    printf(
        "%s\n",
        response
    );

    printf(
        "[+] Downloaded as %s (%lld bytes)\n",
        output_filename,
        filesize
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
