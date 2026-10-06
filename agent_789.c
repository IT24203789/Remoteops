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

void handle_sysinfo(int client_fd);
void handle_listproc(int client_fd);
void handle_exec(int client_fd, const char *command_name);

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

    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) < 0) {

        perror("setsockopt");
        close(server_fd);
        return 1;
    }

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


void handle_sysinfo(int client_fd) {

    FILE *file;

    double cpu_load = 0.0;
    double uptime = 0.0;

    long mem_total_kb = 0;
    long mem_available_kb = 0;
    long mem_used_mb = 0;

    char line[256];
    char response[BUFFER_SIZE];

    file = fopen("/proc/loadavg", "r");

    if (file == NULL) {

        send_response(
            client_fd,
            "ERR 006 SYSINFO_FAILED SID:9873"
        );

        return;
    }

    if (fscanf(file, "%lf", &cpu_load) != 1) {

        fclose(file);

        send_response(
            client_fd,
            "ERR 006 SYSINFO_FAILED SID:9873"
        );

        return;
    }

    fclose(file);


    file = fopen("/proc/meminfo", "r");

    if (file == NULL) {

        send_response(
            client_fd,
            "ERR 006 SYSINFO_FAILED SID:9873"
        );

        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {

        if (sscanf(
                line,
                "MemTotal: %ld kB",
                &mem_total_kb
            ) == 1) {

            continue;
        }

        if (sscanf(
                line,
                "MemAvailable: %ld kB",
                &mem_available_kb
            ) == 1) {

            continue;
        }
    }

    fclose(file);

    if (mem_total_kb <= 0) {

        send_response(
            client_fd,
            "ERR 006 SYSINFO_FAILED SID:9873"
        );

        return;
    }

    mem_used_mb =
        (mem_total_kb - mem_available_kb) / 1024;


    file = fopen("/proc/uptime", "r");

    if (file == NULL) {

        send_response(
            client_fd,
            "ERR 006 SYSINFO_FAILED SID:9873"
        );

        return;
    }

    if (fscanf(file, "%lf", &uptime) != 1) {

        fclose(file);

        send_response(
            client_fd,
            "ERR 006 SYSINFO_FAILED SID:9873"
        );

        return;
    }

    fclose(file);


    snprintf(
        response,
        sizeof(response),
        "OK SYSINFO %.2f %ld %.0f SID:9873",
        cpu_load,
        mem_used_mb,
        uptime
    );

    send_response(
        client_fd,
        response
    );
}


void handle_listproc(int client_fd) {

    FILE *pipe;

    char line[256];
    char response[BUFFER_SIZE];
    char entry[256];

    size_t used;
    int first = 1;

    pipe = popen(
        "ps -eo pid=,comm= --no-headers",
        "r"
    );

    if (pipe == NULL) {

        send_response(
            client_fd,
            "ERR 007 LISTPROC_FAILED SID:9873"
        );

        return;
    }

    used = snprintf(
        response,
        sizeof(response),
        "OK PROCS "
    );

    while (fgets(
               line,
               sizeof(line),
               pipe
           ) != NULL) {

        int pid;
        char process_name[128];

        if (sscanf(
                line,
                "%d %127s",
                &pid,
                process_name
            ) != 2) {

            continue;
        }

        snprintf(
            entry,
            sizeof(entry),
            "%s%d/%s",
            first ? "" : ",",
            pid,
            process_name
        );

        size_t entry_length = strlen(entry);

        if (
            used +
            entry_length +
            strlen(" SID:9873") +
            1 >= sizeof(response)
        ) {

            break;
        }

        memcpy(
            response + used,
            entry,
            entry_length
        );

        used += entry_length;

        response[used] = '\0';

        first = 0;
    }

    pclose(pipe);

    if (first) {

        snprintf(
            response,
            sizeof(response),
            "OK PROCS none SID:9873"
        );

    } else {

        strncat(
            response,
            " SID:9873",
            sizeof(response) -
            strlen(response) -
            1
        );
    }

    send_response(
        client_fd,
        response
    );
}


void handle_exec(int client_fd, const char *command_name) {

    const char *shell_command = NULL;

    char output[2048] = "";
    char line[256];
    char response[BUFFER_SIZE];

    FILE *pipe;


    if (strcmp(command_name, "DATE") == 0) {

        shell_command = "date";

    } else if (strcmp(command_name, "UPTIME") == 0) {

        shell_command = "uptime";

    } else if (strcmp(command_name, "DISKFREE") == 0) {

        shell_command = "df -h / | tail -n 1";

    } else if (strcmp(command_name, "HOSTNAME") == 0) {

        shell_command = "hostname";

    } else if (strcmp(command_name, "WHOAMI") == 0) {

        shell_command = "whoami";

    } else {

        send_response(
            client_fd,
            "ERR 002 COMMAND_NOT_ALLOWED SID:9873"
        );

        return;
    }


    pipe = popen(shell_command, "r");

    if (pipe == NULL) {

        send_response(
            client_fd,
            "ERR 008 EXEC_FAILED SID:9873"
        );

        return;
    }


    while (fgets(
               line,
               sizeof(line),
               pipe
           ) != NULL) {

        line[strcspn(line, "\r\n")] = '\0';

        if (strlen(line) == 0) {
            continue;
        }

        if (strlen(output) > 0) {

            strncat(
                output,
                " ",
                sizeof(output) -
                strlen(output) -
                1
            );
        }

        strncat(
            output,
            line,
            sizeof(output) -
            strlen(output) -
            1
        );
    }

    pclose(pipe);


    if (strlen(output) == 0) {

        strcpy(
            output,
            "(no output)"
        );
    }


    snprintf(
        response,
        sizeof(response),
        "OK EXEC_RESULT %.3500s SID:9873",
        output
    );

    send_response(
        client_fd,
        response
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

            printf(
                "[-] Controller disconnected\n"
            );

            break;
        }

        printf(
            "[COMMAND] %s\n",
            buffer
        );


        if (!authenticated) {

            if (
                strncmp(
                    buffer,
                    "AUTH ",
                    5
                ) == 0
            ) {

                char *token = buffer + 5;

                if (
                    strcmp(
                        token,
                        AUTH_TOKEN
                    ) == 0
                ) {

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


        if (strcmp(buffer, "SYSINFO") == 0) {

            handle_sysinfo(client_fd);

            continue;
        }


        if (strcmp(buffer, "LISTPROC") == 0) {

            handle_listproc(client_fd);

            continue;
        }


        if (strncmp(buffer, "EXEC ", 5) == 0) {

            char *command_name = buffer + 5;

            handle_exec(
                client_fd,
                command_name
            );

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

    printf(
        "[-] Client session closed\n"
    );

    return NULL;
}
