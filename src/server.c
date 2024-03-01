#include "server.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

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

void listen_control_port(struct server_state *server) {
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

    // Specify address and port to connect socket to
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen to all network interfaces and IP addresses
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);
    
    // Bind socket to port
    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for connections on port
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

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

    if (check_prefix(command, COMMAND_USERNAME)) {
        handle_command_username(server, client, command);
    } else if (check_prefix(command, COMMAND_PASSWORD)) {
        handle_command_password(server, client, command);
    } else if (check_prefix(command, COMMAND_PORT)) {
        handle_command_port(server, client, command);
    } else if (check_prefix(command, COMMAND_STORE)) {
        handle_command_store(server, client, command);
    } else if (check_prefix(command, COMMAND_RETRIEVE)) {
        handle_command_retrieve(server, client, command);
    } else if (check_prefix(command, COMMAND_LIST)) {
        handle_command_list(server, client);
    } else if (check_prefix(command, COMMAND_CHANGE_DIRECTORY)) {
        handle_command_change_directory(server, client, command);
    } else if (check_prefix(command, COMMAND_PRINT_DIRECTORY)) {
        handle_command_print_directory(server, client);
    } else if (check_prefix(command, COMMAND_QUIT)) {
        handle_command_quit(server, client);
    } else {
        // Command is not implemented
        send_to_client(client, "202 Command not implemented.");
    }
}

void handle_command_username(struct server_state *server, struct server_client_state *client, char *command) {
    static char buf[AUTH_STR_MAX];
    
    if (client->state != SERVER_CLIENT_STATE_NEED_USERNAME) {
        send_to_client(client, "503 Bad sequence of commands.");
        return;
    }

    // Scan username into buf
    sscanf(command, "%*s %s", buf);
    
    struct user_auth_data *auth_data = find_auth_data_by_username(server, buf);
    if (auth_data == NULL) {
        // No user with the username was found
        send_to_client(client, "530 Not logged in.");
    } else {
        // Update client state
        client->state = SERVER_CLIENT_STATE_NEED_PASSWORD;
        client->auth_data = auth_data;

        send_to_client(client, "331 Username OK, need password.");
    }
}

void handle_command_password(struct server_state *server, struct server_client_state *client, char *command) {
    static char buf[AUTH_STR_MAX];
        
    if (client->state != SERVER_CLIENT_STATE_NEED_PASSWORD) {
        send_to_client(client, "503 Bad sequence of commands.");
        return;
    }

    // Scan password into buf
    sscanf(command, "%*s %s", buf);
    
    if (strcmp(client->auth_data->password, buf) == 0) {
        // Password matches
        client->state = SERVER_CLIENT_STATE_AUTHENTICATED;
        send_to_client(client, "230 User logged in, proceed.");
    } else {
        // Password does not match
        client->state = SERVER_CLIENT_STATE_NEED_USERNAME;
        client->auth_data = NULL;
        send_to_client(client, "530 Not logged in.");
    }
}

void handle_command_port(struct server_state *server, struct server_client_state *client, char *command) {
    
}

void handle_command_store(struct server_state *server, struct server_client_state *client, char *command) {

}

void handle_command_retrieve(struct server_state *server, struct server_client_state *client, char *command) {

}

void handle_command_list(struct server_state *server, struct server_client_state *client) {
    
}

void handle_command_change_directory(struct server_state *server, struct server_client_state *client, char *command) {

}

void handle_command_print_directory(struct server_state *server, struct server_client_state *client) {

}

void handle_command_quit(struct server_state *server, struct server_client_state *client) {
    send_to_client(client, "221 Service closing control connection.");
    remove_client(server, client);
}

void add_new_client(struct server_state *server, int client_sockfd) {
    // Initialize structure
    struct server_client_state *client = malloc(sizeof(struct server_client_state));
    client->sockfd = client_sockfd;
    client->state= SERVER_CLIENT_STATE_NEED_USERNAME;
    client->auth_data = NULL;

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

void send_to_client(struct server_client_state *client, char *message) {
    if (send(client->sockfd, message, strlen(message), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

void read_auth_data(struct server_state *server) {
    server->users_auth_data = NULL;

    // TODO: Read from users.txt, a file which should be located at server->base_path
    // Checking if the file exists is outside of the scope of this project
    // Store results in server->users_auth_data in the form of a linked list

    add_auth_data(server, "abc", "def");

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
    static char buf[PATH_MAX + 2 + AUTH_STR_MAX];
    sprintf(buf, "/%s/%s", server->users_storage_path, auth_data->username);
    create_directory_if_not_exists(buf);
}
