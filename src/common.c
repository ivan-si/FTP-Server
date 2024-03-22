#include "common.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

const char *COMMAND_USERNAME = "USER";
const char *COMMAND_PASSWORD = "PASS";
const char *COMMAND_PORT = "PORT";
const char *COMMAND_STORE = "STOR";
const char *COMMAND_RETRIEVE = "RETR";
const char *COMMAND_LIST = "LIST";
const char *COMMAND_CHANGE_DIRECTORY = "CWD";
const char *COMMAND_PRINT_DIRECTORY = "PWD";
const char *COMMAND_QUIT = "QUIT";

void create_directory_if_not_exists(char *path) {
    // Check if the directory exists
    // Credits: https://stackoverflow.com/questions/12510874/how-can-i-check-if-a-directory-exists
    struct stat sb;
    if (stat(path, &sb) != -1) {
        // It exists already
        // Checking if it is actually a directory is outside the scope of the project
        return;
    }

    // Create the directory
    // Credits: https://stackoverflow.com/questions/7430248/creating-a-new-directory-in-c
    if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
}

int list_directory(char *path, char *result, int result_size) {
    // Credits: https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
    // Open the current directory
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    // Read directory entries
    struct dirent *entry;
    int p = 0;
    while ((entry = readdir(dir)) != NULL) {
        int remaining_bytes = result_size - p - 1;
        if (remaining_bytes < (int)strlen(entry->d_name)) {
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            // Skip non-file entries
            continue;
        }

        // Add entry name to result
        p += sprintf(result + p, "%s%s", p == 0 ? "" : "\n", entry->d_name);
    }

    // Close the directory
    closedir(dir);

    return 0;
}

int check_first_token(const char *string, const char *token) {
    static char buf[COMMAND_STR_MAX];
    
    size_t string_length = strlen(string);
    size_t token_length = strlen(token);
    if (string_length < token_length) {
        return 0;
    } else if (string_length == token_length) {
        return strcmp(string, token) == 0;
    }
    
    sprintf(buf, "%s ", token);
    return strncmp(string, buf, token_length + 1) == 0;
}

void listen_port(int port, int *sockfd_result, int *port_result) {
    // Get socket file descriptor
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options to avoid bind() errors
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Specify socket parameters
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; // IPv4
    addr.sin_addr.s_addr = INADDR_ANY; // Listen to all network interfaces and IP addresses
    addr.sin_port = htons(port);
    
    // Bind socket to port
    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for connections on port
    if (listen(sockfd, 0) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Store socket and port (if asked for)
    *sockfd_result = sockfd;

    if (port_result) {
        socklen_t addr_len = sizeof(addr);
        if (getsockname(sockfd, (struct sockaddr *) &addr, &addr_len) == -1) {
            perror("getsockname");
            exit(EXIT_FAILURE);
        }
        *port_result = ntohs(addr.sin_port);        
        printf("Successfully started listening on port %d\n", *port_result);
    }
}

void connect_to_addr(struct sockaddr_in addr, int *result_sockfd) {
    // Get socket file descriptor
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options to avoid bind() errors
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Connect socket to server
    if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    *result_sockfd = sockfd;
}

void send_message(int sockfd, const char *message) {
    if (send(sockfd, message, strlen(message), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

void receive_response_then_print(int sockfd) {
    static char buf[COMMAND_STR_MAX];

    // Receive response
    int bytes_received = (int)recv(sockfd, buf, COMMAND_STR_MAX - 1, 0);
    if (bytes_received == -1) {
        perror("recv");
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        // Server closed the connection unexpectedly
        fprintf(stderr, "Connection was closed unexpectedly.\n");
        exit(EXIT_FAILURE);
    }

    // Null-terminate the response
    buf[bytes_received] = '\0';
    
    // Print the response
    printf("%s\n", buf);
}

int receive_response_then_check_first_token_then_print_response_if_ok(int sockfd, const char *expected) {
    static char buf[COMMAND_STR_MAX];

    int bytes_received = recv(sockfd, buf, COMMAND_STR_MAX - 1, 0);
    if (bytes_received == -1) {
        perror("recv");
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        // Server closed the connection unexpectedly
        fprintf(stderr, "Connection was closed unexpectedly.\n");
        exit(EXIT_FAILURE);
    } else {
        // Null-terminate the response
        buf[bytes_received] = '\0';

        // Check if response starts with expected token
        if (check_first_token(buf, expected)) {
            printf("%s\n", buf);
            return 1;
        } else {
            fprintf(stderr, "Unexpected response: %s\n", buf);
            return 0;
        }
    }
}
