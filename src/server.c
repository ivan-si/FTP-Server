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

int main() {
    struct server_state server;
    
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working dir: %s\n", cwd);
    } else {
        perror("getcwd() error");
        return 1;
    }
    
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
    char recv_buf[COMMAND_STR_MAX];

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
                struct server_client_state *client = find_client_by_sockfd(server, sockfd);

                int bytes_received = (int)recv(sockfd, recv_buf, COMMAND_STR_MAX - 1, 0);
                if (bytes_received == -1) {
                    perror("recv");
                    exit(EXIT_FAILURE);
                } else if (bytes_received == 0) {
                    // Client closed the connection
                    // Remove all its data
                    remove_client(server, client);
                } else {
                    // Null-terminate the command
                    recv_buf[bytes_received] = '\0';
                    // Handle the command
                    handle_command(server, client, recv_buf);
                }
            }
        }
    }
}

void handle_command(struct server_state *server, struct server_client_state *client, char *command) {
    // Find which command it is
    printf("Got command: %s\n", command);

    if (check_first_token(command, COMMAND_USERNAME)) {
        handle_command_username(server, client, command);
    } else if (check_first_token(command, COMMAND_PASSWORD)) {
        handle_command_password(server, client, command);
    } else if (check_first_token(command, COMMAND_PORT)) {
        handle_command_port(client, command);
    } else if (check_first_token(command, COMMAND_STORE)) {
        handle_command_store(server, client, command);
    } else if (check_first_token(command, COMMAND_RETRIEVE)) {
        handle_command_retrieve(server, client, command);
    } else if (check_first_token(command, COMMAND_LIST)) {
        handle_command_list(server, client);
    } else if (check_first_token(command, COMMAND_CHANGE_DIRECTORY)) {
        handle_command_change_directory(server, client, command);
    } else if (check_first_token(command, COMMAND_PRINT_DIRECTORY)) {
        handle_command_print_directory(server, client);
    } else if (check_first_token(command, COMMAND_QUIT)) {
        handle_command_quit(server, client);
    } else {
        // Command is not implemented
        send_message(client->sockfd, "202 Command not implemented.");
    }
}

void handle_command_username(struct server_state *server, struct server_client_state *client, char *command) {
    if (client->state != SERVER_CLIENT_STATE_NEED_USERNAME) {
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }

    // Get the username from the command
    strtok(command, " ");
    char *username = strtok(NULL, " ");
    if (username == NULL) {
        // No username provided
        send_message(client->sockfd, "530 Not logged in.");
        return;
    }
    
    struct user_auth_data *auth_data = find_auth_data_by_username(server, username);
    if (auth_data == NULL) {
        // No user with the username was found
        send_message(client->sockfd, "530 Not logged in.");
    } else {
        // Update client state
        client->state = SERVER_CLIENT_STATE_NEED_PASSWORD;
        client->auth_data = auth_data;

        send_message(client->sockfd, "331 Username OK, need password.");
    }
}

void handle_command_password(struct server_state *server, struct server_client_state *client, char *command) {   
    if (client->state != SERVER_CLIENT_STATE_NEED_PASSWORD) {
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }

    // Get the password from the command
    strtok(command, " ");
    char *password = strtok(NULL, " ");
    if (password == NULL) {
        // No username provided
        send_message(client->sockfd, "530 Not logged in.");
        return;
    }
    
    if (strcmp(client->auth_data->password, password) == 0) {
        // Password matches
        client->state = SERVER_CLIENT_STATE_AUTHENTICATED;
        initialize_current_path(server, client);
        send_message(client->sockfd, "230 User logged in, proceed.");
    } else {
        // Password does not match
        client->state = SERVER_CLIENT_STATE_NEED_USERNAME;
        client->auth_data = NULL;
        send_message(client->sockfd, "530 Not logged in.");
    }
}

void add_new_client(struct server_state *server, int client_sockfd) {
    // Initialize structure
    struct server_client_state *client = malloc(sizeof(struct server_client_state));
    client->sockfd = client_sockfd;
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
    close(client->sockfd);
    
    // Remove socket from listening sockets
    FD_CLR(client->sockfd, &(server->listen_sockfds));

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

struct server_client_state* find_client_by_sockfd(struct server_state *server, int sockfd) {
    for (struct server_client_state *node = server->clients; node != NULL; node = node->next) {
        if (node->sockfd == sockfd) {
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
    for (struct user_auth_data *node = server->users_auth_data; node != NULL; node = node->next) {
        if (strcmp(node->username, username) == 0) {
            return node;
        }
    }

    return NULL;
}

void initialize_user_storage_directory(struct server_state *server, struct user_auth_data *auth_data) {
    static char buf[PATH_MAX + 1 + AUTH_STR_MAX];
    sprintf(buf, "%s/%s", server->users_storage_path, auth_data->username);
    create_directory_if_not_exists(buf);
}

void initialize_current_path(struct server_state *server, struct server_client_state *client) {
    static char buf[PATH_MAX + 1 + AUTH_STR_MAX];
    
    sprintf(buf, "%s/%s", server->users_storage_path, client->auth_data->username);

    size_t buf_length = strlen(buf);
    if (buf_length > PATH_MAX) {
        fprintf(stderr, "User current path too long for user %s: %s\n", client->auth_data->username, buf);
        exit(EXIT_FAILURE);
    }

    strncpy(client->current_path, buf, sizeof(client->current_path));

    printf("Initialized current path: %s\n", client->current_path);
}

void handle_command_port(struct server_client_state *client, char *command) {
    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        // Not logged in, invalid sequence of commands
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }

    // Parse the command for the address and port (in host order)
    int h1, h2, h3, h4, p1, p2;
    sscanf(command, "%*s %d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);

    in_addr_t address = (h1 << (3 * 8)) | (h2 << (2 * 8)) | (h3 << 8) | h4;
    uint16_t port = (p1 << 8) | p2;

    // Store address and port into state structure
    client->has_data_addr = 1;

    client->data_addr.sin_addr.s_addr = address;
    client->data_addr.sin_port = htons(port);

    printf("Received address %d and port %d\n", address, port);

    send_message(client->sockfd, "200 PORT command successful.");
}

void handle_command_store(struct server_state *server, struct server_client_state *client, char *command) {
    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        // Not logged in, invalid sequence of commands
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }
    if (!client->has_data_addr) {
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }


}

void handle_command_retrieve(struct server_state *server, struct server_client_state *client, char *command) {
    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        // Not logged in, invalid sequence of commands
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }
    if (!client->has_data_addr) {
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }


}

// TODO: They want us to use fork() for LIST, RETR, STOR

void handle_command_list(struct server_state *server, struct server_client_state *client) {
    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        // Not logged in, invalid sequence of commands
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }
    if (!client->has_data_addr) {
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }

    static char list_response[COMMAND_STR_MAX];
    if (list_directory(client->current_path, list_response, sizeof(list_response)) == -1) {
        send_message(client->sockfd, "550 Failed to open directory.");
        return;
    }

    // Send ready response
    send_message(client->sockfd, "150 File status okay; about to open data connection.");

    // Connect and send data
    int data_sockfd;
    connect_to_addr(client->data_addr, &data_sockfd);
    send_message(data_sockfd, list_response);

    // Disconnect
    close(data_sockfd);
    client->has_data_addr = 0;

    // Notify client that the data transfer is complete
    send_message(client->sockfd, "226 Transfer completed.");
}

void handle_command_change_directory(struct server_state *server, struct server_client_state *client, char *command) {
    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        // Not logged in, invalid sequence of commands
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }

}

void handle_command_print_directory(struct server_state *server, struct server_client_state *client) {
    static char buf1[COMMAND_STR_MAX], buf2[COMMAND_STR_MAX * 2];

    if (client->state != SERVER_CLIENT_STATE_AUTHENTICATED) {
        // Not logged in, invalid sequence of commands
        send_message(client->sockfd, "503 Bad sequence of commands.");
        return;
    }

    size_t current_path_length = strlen(client->current_path);
    size_t users_storage_path_length = strlen(server->users_storage_path);
    size_t truncated_length = current_path_length - users_storage_path_length;

    strcpy(buf1, client->current_path);
    memmove(buf1, buf1 + users_storage_path_length + 1, truncated_length + 1);
    
    sprintf(buf2, "257 /Users/%s", buf1);
    send_message(client->sockfd, buf2);
}

void handle_command_quit(struct server_state *server, struct server_client_state *client) {
    send_message(client->sockfd, "221 Service closing control connection.");
    remove_client(server, client);
}
