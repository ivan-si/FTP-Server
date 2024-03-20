#include "common.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
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

void list_directory(char *path) {
    // Credits: https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
    DIR *d = opendir(path);
    if (!d) return;

    struct dirent *dir; 
    while ((dir = readdir(d)) != NULL) {
        printf("%s\n", dir->d_name);
    }
    closedir(d);
}

int check_prefix(const char *string, const char *prefix) {
    while (*string != '\0' && *prefix != '\0') {
        if (*string != *prefix) {
            return 0;
        }

        string++;
        prefix++;
    }

    if (*prefix == '\0') {    
        // Prefix ended before (or at same time) as string
        return 1;
    } else if (*string == '\0') {
        // String ended before prefix
        return 0;
    } else {
        // This should never be reached
        return 0;
    }
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
    }
}
