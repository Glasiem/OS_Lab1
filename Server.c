#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <winsock2.h>
#include <ws2tcpip.h>

bool NON_BLOCKING = 1;
int BUFFER_SIZE = (1024 * 1024);
int PORT = 8080;
size_t total_bytes = 0;
size_t total_packets = 0;
struct timespec start_time, end_time;

int set_non_blocking(int socket_fd) {
    u_long mode = 1;  // 1 to enable non-blocking mode, 0 to disable
    if (NON_BLOCKING) {
        if (ioctlsocket(socket_fd, FIONBIO, &mode) != 0) {
            int err_code = WSAGetLastError();
            fprintf(stderr, "ioctlsocket() failed with error: %d\n", err_code);
            return -1;
        }
        return 0;
    }
    else return 0;
}

bool start_time_initialized = false;
void init_start_time(void) {
    if (!start_time_initialized)
    {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        start_time_initialized = true;
    }
}

double get_elapsed_time(struct timespec *start, struct timespec *end) {
    double start_sec = (double)start->tv_sec + (double)start->tv_nsec / 1e9;
    double end_sec = (double)end->tv_sec + (double)end->tv_nsec / 1e9;
    return end_sec - start_sec;
}

void print_stats(void) {
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double total_elapsed = get_elapsed_time(&start_time, &end_time);
    double bytes_per_sec = (double)total_bytes / total_elapsed;
    double packets_per_sec = (double)total_packets / total_elapsed;
    printf("Socket Type: INET\n");
    printf("Mode: %s\n", NON_BLOCKING ? "Non-blocking" : "Blocking");  // Assuming sync mode; modify as needed
    printf("Total Bytes: %zu\n", total_bytes);
    printf("Total Packets: %zu\n", total_packets);
    printf("Elapsed Time: %.6f seconds\n", total_elapsed);
    printf("Throughput: %.2f bytes/sec, %.2f packets/sec\n", bytes_per_sec, packets_per_sec);
}

unsigned long long create_server_socket(void) {
    unsigned long long server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        fprintf(stderr, "socket() failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    if (set_non_blocking(server_socket) == -1) {
        closesocket(server_socket);
        exit(EXIT_FAILURE);
    }
    return server_socket;
}

void prepare_server_address(struct sockaddr_in *addr) {
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;
    addr->sin_port = htons(PORT);
}

void bind_server_socket(unsigned long long server_socket, struct sockaddr_in *addr) {
    if (bind(server_socket, (const struct sockaddr *)addr, sizeof(*addr)) == -1) {
        perror("bind");
        closesocket(server_socket);
        exit(EXIT_FAILURE);
    }
}

void listen_server_socket(int server_socket) {
    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("listen");
        closesocket(server_socket);
        exit(EXIT_FAILURE);
    }
}

// Main function to handle server logic
int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed with error: %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }

    unsigned long long server_socket = create_server_socket();
    struct sockaddr_in server_addr;
    prepare_server_address(&server_addr);
    bind_server_socket(server_socket, &server_addr);
    listen_server_socket(server_socket);
    unsigned long long client_sockets[100] = {0};

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        unsigned long long client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket != -1) {
            printf("New client %llu connected\n", client_socket);
            init_start_time();
            if (set_non_blocking(client_socket) == -1) {
                closesocket(client_socket);
                continue;
            }
            for (int i = 0; i < 100; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = client_socket;
                    break;
                }
            }
        }
        for (int i = 0; i < 100; i++) {
            unsigned long long client_socket = client_sockets[i];
            if (client_socket > 0) {
                char* buffer = (char*)malloc(BUFFER_SIZE);
                while (1) {
                    if (NON_BLOCKING){
                        Sleep(1);
                    }
                    long n = recv(client_socket, buffer, BUFFER_SIZE, 0);
                    if (n > 0) {
                        total_bytes += n;
                        total_packets += 1;
                        if (send(client_socket, buffer, n, 0) == -1) {
                            perror("send");
                        }
                        if (NON_BLOCKING) {
                            break;
                        }
                    } else if (n == 0 || (n == -1 && errno != EWOULDBLOCK && errno != EAGAIN)) {
                        printf("Client %llu disconnected\n", client_socket);
                        closesocket(client_socket);
                        client_sockets[i] = 0;
                        print_stats();
                        break;
                    }
                }

                free(buffer);
            }
        }
    }

    return 0;
}
