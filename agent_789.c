#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>

#define PORT 9410
#define BUFFER_SIZE 4096
#define MAX_FILE_SIZE (10LL * 1024 * 1024)
#define MONITOR_INTERVAL 5

#define AUTH_TOKEN "OPS-3789"
#define SID "SID:9873"

#define STORAGE_ROOT "./agentfiles"
#define STORAGE_DIR "./agentfiles/IT24103789"
#define LOG_FILE "remoteops_IT24103789.log"

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_info_t;

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    int active;
    int thread_started;
    int udp_socket;
    struct sockaddr_in target_addr;
} monitor_state_t;

void *handle_client(void *arg);
void *monitor_thread(void *arg);

int recv_line(int sockfd, char *buffer, int maxlen);
int send_all(int sockfd, const void *buffer, size_t length);
void send_response(int sockfd, const char *message);

void handle_sysinfo(int client_fd);
void handle_listproc(int client_fd);
void handle_exec(int client_fd, const char *command_name);
void handle_put(int client_fd, const char *filename, long long filesize);
void handle_get(int client_fd, const char *filename);
int read_system_stats(double *cpu_load, long *mem_used_mb, double *uptime);
int start_monitor(monitor_state_t *monitor, const struct sockaddr_in *client_addr, int udp_port);
void stop_monitor(monitor_state_t *monitor);

int ensure_storage_directory(void);
int valid_filename(const char *filename);
void log_event(const char *format, ...);

int main() {

    int server_fd, client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    printf("=====================================\n");
    printf(" RemoteOps Agent - IT24103789\n");
    printf(" Listening Port : %d\n", PORT);
    printf(" Session ID     : %s\n", SID);
    printf(" Storage Path   : %s\n", STORAGE_DIR);
    printf("=====================================\n");

    if (ensure_storage_directory() != 0) {
        fprintf(stderr, "Failed to create storage directory\n");
        return 1;
    }

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
    log_event("AGENT_START port=%d sid=%s", PORT, SID);

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

        char accepted_ip[INET_ADDRSTRLEN] = "unknown";
        inet_ntop(
            AF_INET,
            &client_addr.sin_addr,
            accepted_ip,
            sizeof(accepted_ip)
        );
        log_event(
            "CONNECTION_OPEN ip=%s port=%d",
            accepted_ip,
            ntohs(client_addr.sin_port)
        );

        client_info_t *client_info = malloc(sizeof(client_info_t));

        if (client_info == NULL) {
            close(client_fd);
            continue;
        }

        client_info->client_fd = client_fd;
        client_info->client_addr = client_addr;

        pthread_t thread_id;

        if (pthread_create(
                &thread_id,
                NULL,
                handle_client,
                client_info
            ) != 0) {

            perror("pthread_create");

            close(client_fd);
            free(client_info);

            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_fd);

    return 0;
}


/* Append a timestamped entry to the personalised log file. */
void log_event(const char *format, ...) {

    char timestamp[64];
    time_t now = time(NULL);
    struct tm time_info;

    localtime_r(&now, &time_info);
    strftime(
        timestamp,
        sizeof(timestamp),
        "%Y-%m-%d %H:%M:%S",
        &time_info
    );

    pthread_mutex_lock(&log_mutex);

    FILE *log_file = fopen(LOG_FILE, "a");

    if (log_file != NULL) {
        fprintf(log_file, "[%s] ", timestamp);

        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);

        fputc('\n', log_file);
        fclose(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}


/* Create personalised file-storage directory */
int ensure_storage_directory(void) {

    if (mkdir(STORAGE_ROOT, 0755) < 0 && errno != EEXIST) {
        perror("mkdir agentfiles");
        return -1;
    }

    if (mkdir(STORAGE_DIR, 0755) < 0 && errno != EEXIST) {
        perror("mkdir personalised storage");
        return -1;
    }

    return 0;
}


/* Basic filename validation to prevent path traversal */
int valid_filename(const char *filename) {

    if (filename == NULL || strlen(filename) == 0) {
        return 0;
    }

    if (strcmp(filename, ".") == 0 ||
        strcmp(filename, "..") == 0) {
        return 0;
    }

    if (strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL) {
        return 0;
    }

    if (strstr(filename, "..") != NULL) {
        return 0;
    }

    return 1;
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


/* Receive one complete protocol line */
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


/* Send one line response */
void send_response(int sockfd, const char *message) {

    char response[BUFFER_SIZE];

    snprintf(
        response,
        sizeof(response),
        "%s\n",
        message
    );

    send_all(
        sockfd,
        response,
        strlen(response)
    );
}


/* Read Linux system statistics used by SYSINFO and UDP monitoring */
int read_system_stats(
    double *cpu_load,
    long *mem_used_mb,
    double *uptime
) {
    FILE *file;
    long mem_total_kb = 0;
    long mem_available_kb = 0;
    char line[256];

    file = fopen("/proc/loadavg", "r");
    if (file == NULL) {
        return -1;
    }

    if (fscanf(file, "%lf", cpu_load) != 1) {
        fclose(file);
        return -1;
    }
    fclose(file);

    file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "MemTotal: %ld kB", &mem_total_kb) == 1) {
            continue;
        }
        if (sscanf(line, "MemAvailable: %ld kB", &mem_available_kb) == 1) {
            continue;
        }
    }
    fclose(file);

    if (mem_total_kb <= 0) {
        return -1;
    }

    *mem_used_mb = (mem_total_kb - mem_available_kb) / 1024;

    file = fopen("/proc/uptime", "r");
    if (file == NULL) {
        return -1;
    }

    if (fscanf(file, "%lf", uptime) != 1) {
        fclose(file);
        return -1;
    }
    fclose(file);

    return 0;
}

/* Per-session UDP monitoring thread */
void *monitor_thread(void *arg) {
    monitor_state_t *monitor = (monitor_state_t *)arg;

    while (1) {
        pthread_mutex_lock(&monitor->mutex);
        int active = monitor->active;
        int udp_socket = monitor->udp_socket;
        struct sockaddr_in target = monitor->target_addr;
        pthread_mutex_unlock(&monitor->mutex);

        if (!active) {
            break;
        }

        double cpu_load = 0.0;
        double uptime = 0.0;
        long mem_used_mb = 0;

        if (read_system_stats(&cpu_load, &mem_used_mb, &uptime) == 0) {
            char datagram[BUFFER_SIZE];

            snprintf(
                datagram,
                sizeof(datagram),
                "SYSINFO %.2f %ld %.0f SID:9873",
                cpu_load,
                mem_used_mb,
                uptime
            );

            sendto(
                udp_socket,
                datagram,
                strlen(datagram),
                0,
                (struct sockaddr *)&target,
                sizeof(target)
            );
        }

        for (int i = 0; i < MONITOR_INTERVAL; i++) {
            sleep(1);

            pthread_mutex_lock(&monitor->mutex);
            active = monitor->active;
            pthread_mutex_unlock(&monitor->mutex);

            if (!active) {
                break;
            }
        }
    }

    return NULL;
}

int start_monitor(
    monitor_state_t *monitor,
    const struct sockaddr_in *client_addr,
    int udp_port
) {
    stop_monitor(monitor);

    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        return -1;
    }

    pthread_mutex_lock(&monitor->mutex);
    monitor->target_addr = *client_addr;
    monitor->target_addr.sin_port = htons((unsigned short)udp_port);
    monitor->udp_socket = udp_socket;
    monitor->active = 1;
    pthread_mutex_unlock(&monitor->mutex);

    if (pthread_create(
            &monitor->thread,
            NULL,
            monitor_thread,
            monitor
        ) != 0) {

        pthread_mutex_lock(&monitor->mutex);
        monitor->active = 0;
        monitor->udp_socket = -1;
        pthread_mutex_unlock(&monitor->mutex);

        close(udp_socket);
        return -1;
    }

    monitor->thread_started = 1;
    return 0;
}

void stop_monitor(monitor_state_t *monitor) {
    pthread_mutex_lock(&monitor->mutex);
    int was_started = monitor->thread_started;
    monitor->active = 0;
    pthread_mutex_unlock(&monitor->mutex);

    if (was_started) {
        pthread_join(monitor->thread, NULL);
        monitor->thread_started = 0;
    }

    pthread_mutex_lock(&monitor->mutex);
    if (monitor->udp_socket >= 0) {
        close(monitor->udp_socket);
        monitor->udp_socket = -1;
    }
    pthread_mutex_unlock(&monitor->mutex);
}

/* SYSINFO */
void handle_sysinfo(int client_fd) {
    double cpu_load = 0.0;
    double uptime = 0.0;
    long mem_used_mb = 0;
    char response[BUFFER_SIZE];

    if (read_system_stats(&cpu_load, &mem_used_mb, &uptime) != 0) {
        send_response(
            client_fd,
            "ERR 006 SYSINFO_FAILED SID:9873"
        );
        return;
    }

    snprintf(
        response,
        sizeof(response),
        "OK SYSINFO %.2f %ld %.0f SID:9873",
        cpu_load,
        mem_used_mb,
        uptime
    );

    send_response(client_fd, response);
}

/* LISTPROC */
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


/* Restricted EXEC command */
void handle_exec(
    int client_fd,
    const char *command_name
) {

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
        strcpy(output, "(no output)");
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


/* PUT file upload */
void handle_put(
    int client_fd,
    const char *filename,
    long long filesize
) {

    char filepath[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];

    long long remaining;
    FILE *file;

    if (!valid_filename(filename)) {

        send_response(
            client_fd,
            "ERR 009 INVALID_FILENAME SID:9873"
        );

        return;
    }

    if (filesize < 0) {

        send_response(
            client_fd,
            "ERR 009 INVALID_FILESIZE SID:9873"
        );

        return;
    }

    if (filesize > MAX_FILE_SIZE) {

        /*
         * Controller in this implementation checks the
         * same limit before transmitting file data.
         */
        send_response(
            client_fd,
            "ERR 004 FILE_TOO_LARGE SID:9873"
        );

        return;
    }

    snprintf(
        filepath,
        sizeof(filepath),
        "%s/%s",
        STORAGE_DIR,
        filename
    );

    file = fopen(filepath, "wb");

    if (file == NULL) {

        send_response(
            client_fd,
            "ERR 010 FILE_WRITE_FAILED SID:9873"
        );

        return;
    }

    remaining = filesize;

    while (remaining > 0) {

        size_t amount =
            remaining > (long long)sizeof(file_buffer)
                ? sizeof(file_buffer)
                : (size_t)remaining;

        ssize_t received = recv(
            client_fd,
            file_buffer,
            amount,
            0
        );

        if (received < 0) {

            if (errno == EINTR) {
                continue;
            }

            fclose(file);
            remove(filepath);

            return;
        }

        if (received == 0) {

            fclose(file);
            remove(filepath);

            return;
        }

        size_t written = fwrite(
            file_buffer,
            1,
            (size_t)received,
            file
        );

        if (written != (size_t)received) {

            fclose(file);
            remove(filepath);

            send_response(
                client_fd,
                "ERR 010 FILE_WRITE_FAILED SID:9873"
            );

            return;
        }

        remaining -= received;
    }

    fclose(file);

    snprintf(
        response,
        sizeof(response),
        "OK FILE_RECEIVED %s SID:9873",
        filename
    );

    send_response(
        client_fd,
        response
    );

    printf(
        "[FILE] Received %s (%lld bytes)\n",
        filename,
        filesize
    );

    log_event(
        "FILE_PUT filename=%s bytes=%lld path=%s/%s",
        filename,
        filesize,
        STORAGE_DIR,
        filename
    );
}

/* GET file download */
void handle_get(
    int client_fd,
    const char *filename
) {

    char filepath[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];

    FILE *file;

    if (!valid_filename(filename)) {

        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND SID:9873"
        );

        return;
    }

    snprintf(
        filepath,
        sizeof(filepath),
        "%s/%s",
        STORAGE_DIR,
        filename
    );

    file = fopen(filepath, "rb");

    if (file == NULL) {

        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND SID:9873"
        );

        return;
    }

    if (fseek(file, 0, SEEK_END) != 0) {

        fclose(file);

        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND SID:9873"
        );

        return;
    }

    long filesize = ftell(file);

    if (filesize < 0) {

        fclose(file);

        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND SID:9873"
        );

        return;
    }

    rewind(file);

    snprintf(
        response,
        sizeof(response),
        "OK FILE_SEND %s %ld SID:9873",
        filename,
        filesize
    );

    send_response(
        client_fd,
        response
    );

    long total_sent = 0;

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
                    "[-] Error reading file %s\n",
                    filename
                );
            }

            break;
        }

        if (send_all(
                client_fd,
                file_buffer,
                amount
            ) != 0) {

            printf(
                "[-] Error sending file %s\n",
                filename
            );

            break;
        }

        total_sent += (long)amount;
    }

    fclose(file);

    if (total_sent == filesize) {

        printf(
            "[FILE] Sent %s (%ld bytes)\n",
            filename,
            filesize
        );

        log_event(
            "FILE_GET filename=%s bytes=%ld path=%s/%s",
            filename,
            filesize,
            STORAGE_DIR,
            filename
        );
    }
}


/* Handle each Controller */
void *handle_client(void *arg) {

    client_info_t *client_info = (client_info_t *)arg;
    int client_fd = client_info->client_fd;
    struct sockaddr_in client_addr = client_info->client_addr;

    free(client_info);

    char client_ip[INET_ADDRSTRLEN] = "unknown";
    inet_ntop(
        AF_INET,
        &client_addr.sin_addr,
        client_ip,
        sizeof(client_ip)
    );

    char buffer[BUFFER_SIZE];
    int authenticated = 0;

    monitor_state_t monitor;
    memset(&monitor, 0, sizeof(monitor));
    monitor.udp_socket = -1;
    pthread_mutex_init(&monitor.mutex, NULL);

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

            log_event(
                "DISCONNECT_UNGRACEFUL ip=%s",
                client_ip
            );

            break;
        }

        printf(
            "[COMMAND] %s\n",
            buffer
        );

        if (strncmp(buffer, "AUTH ", 5) == 0) {
            log_event(
                "COMMAND ip=%s AUTH <redacted>",
                client_ip
            );
        } else {
            log_event(
                "COMMAND ip=%s %s",
                client_ip,
                buffer
            );
        }


        /* Authentication must happen first */
        if (!authenticated) {

            if (strncmp(
                    buffer,
                    "AUTH ",
                    5
                ) == 0) {

                char *token = buffer + 5;

                if (strcmp(
                        token,
                        AUTH_TOKEN
                    ) == 0) {

                    authenticated = 1;

                    send_response(
                        client_fd,
                        "OK AUTHENTICATED SID:9873"
                    );

                    log_event(
                        "AUTH_SUCCESS ip=%s",
                        client_ip
                    );

                } else {

                    send_response(
                        client_fd,
                        "ERR 001 AUTH_FAILED SID:9873"
                    );

                    log_event(
                        "AUTH_FAILED ip=%s",
                        client_ip
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


        if (strncmp(buffer, "PUT ", 4) == 0) {

            char *arguments = buffer + 4;

            char *last_space =
                strrchr(arguments, ' ');

            if (last_space == NULL) {

                send_response(
                    client_fd,
                    "ERR 009 INVALID_PUT_FORMAT SID:9873"
                );

                continue;
            }

            *last_space = '\0';

            char *filename = arguments;
            char *size_text = last_space + 1;

            char *endptr = NULL;

            errno = 0;

            long long filesize =
                strtoll(
                    size_text,
                    &endptr,
                    10
                );

            if (
                errno != 0 ||
                endptr == size_text ||
                *endptr != '\0' ||
                filesize < 0
            ) {

                send_response(
                    client_fd,
                    "ERR 009 INVALID_FILESIZE SID:9873"
                );

                continue;
            }

            handle_put(
                client_fd,
                filename,
                filesize
            );

            continue;
        }if (strncmp(buffer, "GET ", 4) == 0) {

    char *filename = buffer + 4;

    while (*filename == ' ') {
        filename++;
    }

    if (*filename == '\0') {

        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND SID:9873"
        );

        continue;
    }

    handle_get(
        client_fd,
        filename
    );

    continue;
}


        if (strncmp(buffer, "MONITOR START ", 14) == 0) {
            char *port_text = buffer + 14;
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
                send_response(
                    client_fd,
                    "ERR 011 INVALID_UDP_PORT SID:9873"
                );
                continue;
            }

            if (start_monitor(
                    &monitor,
                    &client_addr,
                    (int)udp_port
                ) != 0) {

                send_response(
                    client_fd,
                    "ERR 012 MONITOR_FAILED SID:9873"
                );
                continue;
            }

            send_response(
                client_fd,
                "OK MONITOR_STARTED SID:9873"
            );

            printf(
                "[MONITOR] UDP stream started to %s:%ld\n",
                inet_ntoa(client_addr.sin_addr),
                udp_port
            );

            continue;
        }

        if (strcmp(buffer, "MONITOR STOP") == 0) {
            stop_monitor(&monitor);

            send_response(
                client_fd,
                "OK MONITOR_STOPPED SID:9873"
            );

            printf("[MONITOR] UDP stream stopped\n");
            continue;
        }

        if (strcmp(buffer, "QUIT") == 0) {
            stop_monitor(&monitor);

            send_response(
                client_fd,
                "OK BYE SID:9873"
            );

            log_event(
                "DISCONNECT_GRACEFUL ip=%s",
                client_ip
            );

            break;
        }


        send_response(
            client_fd,
            "ERR 003 UNKNOWN_COMMAND SID:9873"
        );
    }

    stop_monitor(&monitor);
    pthread_mutex_destroy(&monitor.mutex);

    close(client_fd);

    log_event(
        "SESSION_CLOSED ip=%s",
        client_ip
    );

    printf(
        "[-] Client session closed\n"
    );

    return NULL;
}
