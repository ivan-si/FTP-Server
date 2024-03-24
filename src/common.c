#include "common.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
    if (result_size == 0) {
        fprintf(stderr, "result_size must be positive\n");
        exit(EXIT_FAILURE);
    }

    // Open the current directory
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    // Clear the buffer
    result[0] = '\0';

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

int is_path_directory(char *path) {
    struct stat stat_result;
    if (stat(path, &stat_result) == -1) {
        return 0;
    } else if (!S_ISDIR(stat_result.st_mode)) {
        return 0;
    } else {
        return 1;
    }
}

int is_path_file(char *path) {
    struct stat stat_result;
    if (stat(path, &stat_result) == -1) {
        return 0;
    } else if (!S_ISREG(stat_result.st_mode)) {
        return 0;
    } else {
        return 1;
    }
}

int check_first_token(const char *string, const char *token) {
    static char buf[COMMAND_STR_MAX];
    
    size_t string_length = strlen(string);
    size_t token_length = strlen(token);
    if (string_length < token_length) {
        // The string cannot contain the token
        return 0;
    } else if (string_length == token_length) {
        // They are the same length, so compare them whole
        return strcmp(string, token) == 0;
    }
    
    // Compare (string) with (token with a space at the end)
    sprintf(buf, "%s ", token);
    return strncmp(string, buf, token_length + 1) == 0;
}

int try_bind(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Create an address structure for the local host
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // Try to bind to the port
    int result = bind(sockfd, (struct sockaddr *) &addr, sizeof(addr));
    
    // Close the socket
    close(sockfd);

    // If bind returned 0, it was successful and the port was free
    return result == 0;
}

int get_next_free_port(int port) {
    int start_port = port;

    // Start searching from the initial port + 1 until 65535
    port++;
    while (port < 65536) {
        if (try_bind(port)) {
            return port;
        } else {
            port++;
        }
    }

    // Wrap back down to 1024 and search until the initial port - 1
    port = 1024;
    while (port < start_port) {
        if (try_bind(port)) {
            return port;
        } else {
            port++;
        }
    }

    // No free port was found
    return -1;
}

int listen_port(int port, int *result_sockfd, int *result_port) {
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
        return -1;
    }

    // Listen for connections on port
    if (listen(sockfd, LISTEN_BACKLOG) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Store socket file descriptor and port (if asked for)
    *result_sockfd = sockfd;

    if (result_port) {
        socklen_t addr_len = sizeof(addr);
        if (getsockname(sockfd, (struct sockaddr *) &addr, &addr_len) == -1) {
            perror("getsockname");
            exit(EXIT_FAILURE);
        }
        *result_port = ntohs(addr.sin_port);
    }

    return 0;
}

void connect_to_addr(struct sockaddr_in addr, int *result_sockfd, int *result_port) {
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

    // Connect to address
    if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // Store socket file descriptor and port (if asked for)
    *result_sockfd = sockfd;

    if (result_port) {
        socklen_t addr_len = sizeof(addr);
        if (getsockname(sockfd, (struct sockaddr *) &addr, &addr_len) == -1) {
            perror("getsockname");
            exit(EXIT_FAILURE);
        }
        *result_port = ntohs(addr.sin_port);
    }
}

void send_message(int sockfd, const char *message) {
    // Send the message through the socket
    if (send(sockfd, message, strlen(message), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

void send_file(int sockfd, const char *path) {
    static char buf[FILE_TRANSFER_BUFFER_SIZE];

    // Open the file for reading in binary format (text format is covered by this)
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    size_t bytes_read;

    // Read bytes from the file into the buffer
    while ((bytes_read = fread(buf, 1, sizeof(buf), file)) > 0) {
        // Send bytes through the socket
        int bytes_sent = send(sockfd, buf, bytes_read, 0);
        if (bytes_sent == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
    }

    // Close the file
    fclose(file);
}

void save_file(int sockfd, const char *path) {
    static char buf[FILE_TRANSFER_BUFFER_SIZE];

    // Open the file for reading in binary format (text format is covered by this)
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    size_t bytes_received;

    // Receive bytes through the socket into the buffer
    while ((bytes_received = recv(sockfd, buf, FILE_TRANSFER_BUFFER_SIZE, 0)) > 0) {
        // Write the bytes into the file
        if (fwrite(buf, 1, bytes_received, file) == 0) {
            perror("fwrite");
            exit(EXIT_FAILURE);
        }
    }

    // Close the file
    fclose(file);
}

void receive_message_then_print(int sockfd) {
    static char buf[COMMAND_STR_MAX];

    // Receive response
    int bytes_received = (int)recv(sockfd, buf, COMMAND_STR_MAX - 1, 0);
    if (bytes_received == -1) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    // Null-terminate the response
    buf[bytes_received] = '\0';
    
    // Print the response
    printf("%s\n", buf);
}

int receive_message_then_print_then_check_first_token(int sockfd, const char *expected) {
    static char buf[COMMAND_STR_MAX];

    int bytes_received = recv(sockfd, buf, COMMAND_STR_MAX - 1, 0);
    if (bytes_received == -1) {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    
    // Null-terminate the response
    buf[bytes_received] = '\0';

    // Print the response
    printf("%s\n", buf);

    // Check if response starts with expected token
    return check_first_token(buf, expected);
}
