#include "server.h"
#include "common.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

int main() {
    struct server_state server;
    initialize_server_directories(&server);
    read_auth_data(&server);
    listen_port(SERVER_CONTROL_PORT, &(server.control_sockfd), NULL);
    monitor_control_port(&server);
}

void initialize_server_directories(struct server_state *server) {
    // Get the current working directory
    static char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // Create the base directory if it does not exist
    strcat(buf, "/server");
    create_directory_if_not_exists(buf);
    strncpy(server->base_path, buf, sizeof(buf));

    // Create the users storage directory if it does not exist
    strcat(buf, "/users");
    create_directory_if_not_exists(buf);
    strncpy(server->users_storage_path, buf, sizeof(buf));
}

void monitor_control_port(struct server_state *server) {
    // No clients connected initially
    server->clients = NULL;

    // Initialize listening sockets
    // Initially, only control socket is present
    // Add it to the set of sockets to monitor
    FD_ZERO(&(server->listen_sockfds));
    FD_SET(server->control_sockfd, &(server->listen_sockfds));

    int max_sockfd_so_far = server->control_sockfd;

    while (1) {
        fd_set ready_sockfds = server->listen_sockfds;

        // Find out which sockets have incoming data
        if (select(max_sockfd_so_far + 1, &ready_sockfds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (int sockfd = 0; sockfd <= max_sockfd_so_far; sockfd++) {
            // Ensure this socket has incoming data
            if (!FD_ISSET(sockfd, &ready_sockfds)) continue;

            if (sockfd == server->control_sockfd) {
                // A new client wants to connect

                // Accept connection and add its socket to the set
                int client_sockfd = accept(server->control_sockfd, 0, 0);
                FD_SET(client_sockfd, &(server->listen_sockfds));
                if (client_sockfd > max_sockfd_so_far) {
                    max_sockfd_so_far = client_sockfd;
                }

                // Add to list of clients
                add_new_client(server, client_sockfd);

                // Send ready message
                send_message(client_sockfd, "220 Service ready for new user.");
            } else {
                // A client is sending data
                struct server_client_state *client = find_client_by_control_sockfd(server, sockfd);
                handle_client_sending_data(server, client);
            }
        }
    }
}

void handle_client_sending_data(struct server_state *server, struct server_client_state *client) {
    static char command[COMMAND_STR_MAX];
    
    int bytes_received = (int)recv(client->control_sockfd, command, COMMAND_STR_MAX - 1, 0);
    if (bytes_received == -1) {
        perror("recv");
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        // Client closed the connection
        // Remove all its data
        remove_client(server, client);
    } else {
        // Null-terminate the command
        command[bytes_received] = '\0';
        // Handle the command
        handle_command(server, client, command);
    }
}

void handle_command(struct server_state *server, struct server_client_state *client, char *command) {
    // Handle command based on which one it is
    if (check_first_token(command, COMMAND_USERNAME)) {
        handle_command_username(server, client, command);
    } else if (check_first_token(command, COMMAND_PASSWORD)) {
        handle_command_password(server, client, command);
    } else if (check_first_token(command, COMMAND_PORT)) {
        handle_command_port(client, command);
    } else if (check_first_token(command, COMMAND_STORE)) {
        handle_command_store(client, command);
    } else if (check_first_token(command, COMMAND_RETRIEVE)) {
        handle_command_retrieve(client, command);
    } else if (check_first_token(command, COMMAND_LIST)) {
        handle_command_list(client);
    } else if (check_first_token(command, COMMAND_CHANGE_DIRECTORY)) {
        handle_command_change_directory(server, client, command);
    } else if (check_first_token(command, COMMAND_PRINT_DIRECTORY)) {
        handle_command_print_directory(server, client);
    } else if (check_first_token(command, COMMAND_QUIT)) {
        handle_command_quit(server, client);
    } else {
        // Command is not implemented
        send_message(client->control_sockfd, "202 Command not implemented.");
    }
}

// TODO: Handle same user logged in twice
// TODO: Handle user logging in, disconnecting, logging in again

void handle_command_username(struct server_state *server, struct server_client_state *client, char *command) {
    if (client->state != SERVER_CLIENT_STATE_NEED_USERNAME) {
        send_message(client->control_sockfd, "503 Bad sequence of commands.");
        return;
    }

    // Get the username from the command
    strtok(command, " ");
    char *username = strtok(NULL, " ");
    if (username == NULL) {
        // No username provided
        send_message(client->control_sockfd, "530 Not logged in.");
        return;
    }
    
    struct user_auth_data *auth_data = find_auth_data_by_username(server, username);
    if (auth_data == NULL) {
        // No user with the username was found
        send_message(client->control_sockfd, "530 Not logged in.");
    } else {
        // Update client state
        client->state = SERVER_CLIENT_STATE_NEED_PASSWORD;
        client->auth_data = auth_data;

        send_message(client->control_sockfd, "331 Username OK, need password.");
    }
}

void handle_command_password(struct server_state *server, struct server_client_state *client, char *command) {   
    if (client->state != SERVER_CLIENT_STATE_NEED_PASSWORD) {
        send_message(client->control_sockfd, "503 Bad sequence of commands.");
        return;
    }

    // Get the password from the command
    strtok(command, " ");
    char *password = strtok(NULL, " ");
    if (password == NULL) {
        // No username provided
        send_message(client->control_sockfd, "530 Not logged in.");
        return;
    }
    
    if (strcmp(client->auth_data->password, password) == 0) {
        // Password matches
        client->state = SERVER_CLIENT_STATE_AUTHENTICATED;
        initialize_current_path(server, client);
        send_message(client->control_sockfd, "230 User logged in, proceed.");
    } else {
        // Password does not match
        client->state = SERVER_CLIENT_STATE_NEED_USERNAME;
        client->auth_data = NULL;
        send_message(client->control_sockfd, "530 Not logged in.");
    }
}

void add_new_client(struct server_state *server, int client_sockfd) {
    // Initialize structure
    struct server_client_state *client = malloc(sizeof(struct server_client_state));
    client->control_sockfd = client_sockfd;
    client->state = SERVER_CLIENT_STATE_NEED_USERNAME;
    client->auth_data = NULL;
    client->has_data_addr = 0;
    client->data_addr.sin_family = AF_INET; // IPV4
    
    // Make structure head of linked list
    client->next = server->clients;
    server->clients = client;
}

void remove_client(struct server_state *server, struct server_client_state *client) {
    // Close the socket
    close(client->control_sockfd);
    
    // Remove socket from listening sockets
    FD_CLR(client->control_sockfd, &(server->listen_sockfds));

    // Remove client from list
    if (server->clients == client) {
        // It is the head node of the list
        server->clients = client->next;
        free(client);
    } else {
        // It is not the head node of the list
        for (struct server_client_state *node = server->clients; node != NULL; node = node->next) {
            if (node->next == client) {
                node->next = client->next;
                free(client);
                break;
            }
        }
    }
}

struct server_client_state* find_client_by_control_sockfd(struct server_state *server, int control_sockfd) {
    // Go through the linked list until the client with the given control_sockfd is found
    for (struct server_client_state *node = server->clients; node != NULL; node = node->next) {
        if (node->control_sockfd == control_sockfd) {
            return node;
        }
    }

    return NULL;
}

void read_auth_data(struct server_state *server) {
    server->users_auth_data = NULL;

    // Open the users.txt file for reading
    static char users_txt_path[PATH_MAX * 2];
    sprintf(users_txt_path, "%s/users.txt", server->base_path);

    // If path is too long, error since system will error later anyway
    size_t users_txt_path_length = strlen(users_txt_path);
    if (users_txt_path_length > PATH_MAX) {
        fprintf(stderr, "users.txt path too long\n");
        exit(EXIT_FAILURE);
    }

    // Open the file
    FILE *file = fopen(users_txt_path, "r");
    if (file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Read each line from the file and parse username and password
    static char line[AUTH_STR_MAX + 1 + AUTH_STR_MAX];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // Tokenize the line to extract username and password
        char *username = strtok(line, " ");
        char *password = strtok(NULL, " ");

        // Skip empty lines
        if (username == NULL || password == NULL) {
            continue;
        }

        // Add the authentication data to the linked list
        add_auth_data(server, username, password);
    }

    // Close the file
    fclose(file);

    // For each user, create their storage directory if it is missing
    for (struct user_auth_data *node = server->users_auth_data; node != NULL; node = node->next) {
        initialize_user_storage_directory(server, node);
    }
}

void add_auth_data(struct server_state *server, char *username, char *password) {
    // Create structure
    struct user_auth_data *t = malloc(sizeof(struct user_auth_data));
    strcpy(t->username, username);
    strcpy(t->password, password);

    // Make structure head of linked list
    t->next = server->users_auth_data;
    server->users_auth_data = t;
}

struct user_auth_data* find_auth_data_by_username(struct server_state *server, char *username) {
    // Go through the list until the user_auth_data with the given username is found
    for (struct user_auth_data *node = server->users_auth_data; node != NULL; node = node->next) {
        if (strcmp(node->username, username) == 0) {
            return node;
        }
    }

    return NULL;
}

void initialize_user_storage_directory(struct server_state *server, struct user_auth_data *auth_data) {
    static char buf[PATH_MAX + 1 + AUTH_STR_MAX];

    // Construct the user's base and directory, then create it if it does not exist
    sprintf(buf, "%s/%s", server->users_storage_path, auth_data->username);
    create_directory_if_not_exists(buf);
}

void initialize_current_path(struct server_state *server, struct server_client_state *client) {
    static char buf[PATH_MAX + 1 + AUTH_STR_MAX];
    
    // Initialize the current path of the user to the user's base directory
    sprintf(buf, "%s/%s", server->users_storage_path, client->auth_data->username);
    strcpy(client->current_path, buf);
}

void handle_command_port(struct server_client_state *client, char *command) {
    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        send_message(client->control_sockfd, "532 Need account for storing files.");
        return;
    }

    // Parse the command for the address and port (in host order)
    int h1, h2, h3, h4, p1, p2;
    
    int scanned_count = sscanf(command, "%*s %d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
    if (scanned_count < 6) {
        // Message not in expected format
        send_message(client->control_sockfd, "501 Syntax error in parameters or arguments.");
        return;
    }

    in_addr_t address = (h1 << (3 * 8)) | (h2 << (2 * 8)) | (h3 << 8) | h4;
    uint16_t port = (p1 << 8) | p2;

    // Store address and port into state structure
    client->has_data_addr = 1;

    client->data_addr.sin_addr.s_addr = address;
    client->data_addr.sin_port = htons(port);

    send_message(client->control_sockfd, "200 PORT command successful.");
}

void handle_command_store(struct server_client_state *client, char *command) {
    static char buf[PATH_MAX + 1 + COMMAND_STR_MAX];

    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        send_message(client->control_sockfd, "532 Need account for storing files.");
        return;
    }
    if (!client->has_data_addr) {
        send_message(client->control_sockfd, "503 Bad sequence of commands.");
        return;
    }

    // Fork a child process to handle the data transfer
    pid_t child_pid = fork();
    if (child_pid > 0) {
        // This is the parent process, just exit
        return;
    }

    // This is the child process
    // Extract the filename from the command
    strtok(command, " ");
    char *filename = strtok(NULL, " ");
    if (filename == NULL) {
        send_message(client->control_sockfd, "501 Syntax error in parameters or arguments.");
        return;
    }

    // Ensure the filename has no slashes
    char *last_slash = strrchr(filename, '/');
    if (last_slash != NULL) {
        send_message(client->control_sockfd, "550 Requested action not taken. File name not allowed.");
        return;
    }

    // Send ready response
    send_message(client->control_sockfd, "150 File status okay; about to open data connection.");

    // Connect
    int data_sockfd;
    connect_to_addr(client->data_addr, &data_sockfd);

    // Receive the file and save it at the client directory
    sprintf(buf, "%s/%s", client->current_path, filename);
    save_file(data_sockfd, buf);

    // Disconnect
    close(data_sockfd);
    client->has_data_addr = 0;

    // Notify client that the data transfer is complete
    send_message(client->control_sockfd, "226 Transfer completed.");

    // Since this is a child process of the server, exit successfully
    exit(EXIT_SUCCESS);
}

void handle_command_retrieve(struct server_client_state *client, char *command) {
    static char buf[PATH_MAX + 1 + COMMAND_STR_MAX];

    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        send_message(client->control_sockfd, "532 Need account for storing files.");
        return;
    }
    if (!client->has_data_addr) {
        send_message(client->control_sockfd, "503 Bad sequence of commands.");
        return;
    }

    // Fork a child process to handle the data transfer
    pid_t child_pid = fork();
    if (child_pid > 0) {
        // This is the parent process, just exit
        return;
    }

    // This is the child process
    // Extract the filename from the command
    strtok(command, " ");
    char *filename = strtok(NULL, " ");
    if (filename == NULL) {
        send_message(client->control_sockfd, "501 Syntax error in parameters or arguments.");
        return;
    }
    // Ensure the filename has no slashes
    char *last_slash = strrchr(filename, '/');
    if (last_slash != NULL) {
        send_message(client->control_sockfd, "550 Requested action not taken. File name not allowed.");
        return;
    }

    // Write the file path into the buffer
    sprintf(buf, "%s/%s", client->current_path, filename);

    // Ensure the file exists
    if (is_path_directory(buf)) {
        send_message(client->control_sockfd, "504 Command not implemented for that parameter.");
        return;    
    } else if (!is_path_file(buf)) {
        send_message(client->control_sockfd, "550 No such file or directory.");
        return;
    }

    // Send ready response
    send_message(client->control_sockfd, "150 File status okay; about to open data connection.");

    // Connect
    int data_sockfd;
    connect_to_addr(client->data_addr, &data_sockfd);

    // Send the file
    send_file(data_sockfd, buf);
    
    // Disconnect
    close(data_sockfd);
    client->has_data_addr = 0;

    // Notify client that the data transfer is complete
    send_message(client->control_sockfd, "226 Transfer completed.");

    // Since this is a child process of the server, exit successfully
    exit(EXIT_SUCCESS);
}

void handle_command_list(struct server_client_state *client) {
    static char buf[COMMAND_STR_MAX];
    
    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        send_message(client->control_sockfd, "530 Not logged in.");
        return;
    }
    if (!client->has_data_addr) {
        send_message(client->control_sockfd, "503 Bad sequence of commands.");
        return;
    }

    // Fork a child process to handle the data transfer
    pid_t child_pid = fork();
    if (child_pid > 0) {
        // This is the parent process, nothing else to do for this client
        return;
    }

    // This is the child process
    // List the files
    if (list_directory(client->current_path, buf, sizeof(buf)) == -1) {
        send_message(client->control_sockfd, "550 Failed to open directory.");
        return;
    }

    // Send ready response
    send_message(client->control_sockfd, "150 File status okay; about to open data connection.");

    // Connect
    int data_sockfd;
    connect_to_addr(client->data_addr, &data_sockfd);

    // Send the data
    send_message(data_sockfd, buf);

    // Disconnect
    close(data_sockfd);
    client->has_data_addr = 0;

    // Notify client that the data transfer is complete
    send_message(client->control_sockfd, "226 Transfer completed.");

    // Since this is a child process of the server, exit successfully
    exit(EXIT_SUCCESS);
}

void handle_command_change_directory(struct server_state *server, struct server_client_state *client, char *command) {
    static char buf[PATH_MAX];
    static char working_dir_resolved[PATH_MAX];
    static char user_dir_resolved[PATH_MAX];
    static char response[COMMAND_STR_MAX];

    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        send_message(client->control_sockfd, "530 Not logged in.");
        return;
    }

    // Extract the path from the command
    strtok(command, " ");
    char *path = strtok(NULL, " ");

    // Construct the expected new working directory for the user
    {
        int p = sprintf(buf, "%s", client->current_path);
        p += sprintf(buf + p, "/%s", path);
    }

    // Ensure the new working directory is a directory
    if (!is_path_directory(buf)) {
        send_message(client->control_sockfd, "550 No such file or directory.");
        return;
    }

    // Resolve the new working directory
    if (realpath(buf, working_dir_resolved) == NULL) {
        perror("realpath");
        exit(EXIT_FAILURE);
    }

    // Get the user's base directory and resolve it
    {
        int p = sprintf(buf, "%s", server->users_storage_path);
        p += sprintf(buf + p, "/%s", client->auth_data->username);
    }
    if (realpath(buf, user_dir_resolved) == NULL) {
        perror("realpath");
        exit(EXIT_FAILURE);    
    }

    // Ensure the new path (resolved) starts with the user's base directory
    size_t user_dir_resolved_length = strlen(user_dir_resolved);
    if (strncmp(working_dir_resolved, user_dir_resolved, user_dir_resolved_length) != 0) {
        // New path does not start with the user's base directory
        send_message(client->control_sockfd, "550 No such file or directory.");
        return;
    }

    // Update the user's working directory
    strcpy(client->current_path, working_dir_resolved);

    // Truncate the user's working directory from the user's base path
    size_t current_path_length = strlen(client->current_path);
    size_t users_storage_path_length = strlen(server->users_storage_path);
    if (current_path_length < users_storage_path_length) {
        fprintf(stderr, "User %s current path (%s) is shorter than users storage path (%s)\n",
            client->auth_data->username, client->current_path, server->users_storage_path);
        exit(EXIT_FAILURE);
    }

    size_t truncated_length = current_path_length - users_storage_path_length;
    strcpy(buf, client->current_path);
    memmove(buf, buf + users_storage_path_length + 1, truncated_length + 1);
    
    // Prepare the response, then send it
    {
        int p = sprintf(response, "200 directory changed to /Users/");
        p += sprintf(response + p, "%s", buf);
    }
    send_message(client->control_sockfd, response);
}

void handle_command_print_directory(struct server_state *server, struct server_client_state *client) {
    static char buf1[COMMAND_STR_MAX], buf2[COMMAND_STR_MAX * 2];

    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        send_message(client->control_sockfd, "530 Not logged in.");
        return;
    }

    // Truncate the user's working directory from the user's base path
    size_t current_path_length = strlen(client->current_path);
    size_t users_storage_path_length = strlen(server->users_storage_path);
    if (current_path_length < users_storage_path_length) {
        fprintf(stderr, "User %s current path (%s) is shorter than users storage path (%s)\n",
            client->auth_data->username, client->current_path, server->users_storage_path);
        exit(EXIT_FAILURE);
    }

    size_t truncated_length = current_path_length - users_storage_path_length;
    strcpy(buf1, client->current_path);
    memmove(buf1, buf1 + users_storage_path_length + 1, truncated_length + 1);
    
    sprintf(buf2, "257 /Users/%s", buf1);
    send_message(client->control_sockfd, buf2);
}

void handle_command_quit(struct server_state *server, struct server_client_state *client) {
    send_message(client->control_sockfd, "221 Service closing control connection.");
    remove_client(server, client);
}
