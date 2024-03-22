#include "client.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

const char *COMMAND_LIST_CLIENT = "!LIST";
const char *COMMAND_CHANGE_DIRECTORY_CLIENT = "!CWD";
const char *COMMAND_PRINT_DIRECTORY_CLIENT = "!PWD";

int main() {
    struct client_state client;
    client.control_sockfd = -1;
    client.data_listen_sockfd = -1;
    client.data_sockfd = -1;
    client.data_port = -1;

    // Connect to the server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPV4
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);

    connect_to_addr(server_addr, &(client.control_sockfd));
    
    if (!receive_response_then_check_first_token_then_print_response_if_ok(client.control_sockfd, "220")) {
        exit(EXIT_FAILURE);
    }

    get_commands(&client);

    return EXIT_SUCCESS;
}

void get_commands(struct client_state *client) {
    static char command[COMMAND_STR_MAX];

    while (1) {
        printf("ftp> ");
        
        // Get a line from stdin
        fgets(command, sizeof(command), stdin);
        
        int command_length = strcspn(command, "\n");        
        if (command_length == 0) {
            // Only newline
            continue;
        }
        
        // Remove trailing newline
        command[command_length--] = '\0';
        
        if (check_first_token(command, COMMAND_LIST_CLIENT)) {
            // TODO: Implement these functions
            // Will need to initialize client->current_path in main() first and then use it here
        } else if (check_first_token(command, COMMAND_CHANGE_DIRECTORY_CLIENT)) {

        } else if (check_first_token(command, COMMAND_PRINT_DIRECTORY_CLIENT)) {
        
        } else if (check_first_token(command, COMMAND_LIST)) {
            execute_command_list(client);
        } else if (check_first_token(command, COMMAND_STORE)) {
            execute_command_store(client, command);
        } else if (check_first_token(command, COMMAND_RETRIEVE)) {
            execute_command_retrieve(client, command);
        } else {
            // This includes PORT (which effectively does nothing since it is
            // sent again before LIST, STORE, or RETR)

            // TODO: Handle "PORT" being entered with nothing else hanging the program
            
            send_message(client->control_sockfd, command);
            receive_response_then_print(client->control_sockfd);

            if (check_first_token(command, COMMAND_QUIT)) {
                break;
            }
        }
    }
}

int send_data_port(struct client_state *client) {
    static char buf[COMMAND_STR_MAX];

    // Add command to buffer
    int p = sprintf(buf, "%s ", COMMAND_PORT);
    // Add address to buffer
    in_addr_t address = inet_addr("127.0.0.1");
    for (int i = 3; i >= 0; i--) {
        int x = (address >> (8 * i)) & 0xff;
        p += sprintf(buf + p, "%d,", x);
    }
    // Add port to buffer (in host order)
    int port = client->data_port;
    for (int i = 1; i >= 0; i--) {
        int x = (port >> (8 * i)) & 0xff;
        p += sprintf(buf + p, "%d", x);
        if (i) p += sprintf(buf + p, ",");
    }

    // Send to server
    send_message(client->control_sockfd, buf);

    return receive_response_then_check_first_token_then_print_response_if_ok(client->control_sockfd, "200") ? 0 : -1;
}

void initiate_data_transfer(struct client_state *client) {
    // Wait for server to ask to establish TCP connection, and accept it
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    int data_sockfd = accept(client->data_listen_sockfd, (struct sockaddr *) &server_addr, &addr_len);
    if (data_sockfd == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    
    client->data_sockfd = data_sockfd;
}

void end_data_transfer(struct client_state *client) {
    close(client->data_sockfd);
    client->data_sockfd = -1;
    client->data_port = -1;
}

void execute_command_list(struct client_state *client) {
    // Start listening on some port for data, send the port
    listen_port(0, &(client->data_listen_sockfd), &(client->data_port));
    if (send_data_port(client) == -1) {
        return;
    }
    
    // Send the command, get server response
    send_message(client->control_sockfd, COMMAND_LIST);
    if (!receive_response_then_check_first_token_then_print_response_if_ok(client->control_sockfd, "150")) {
        return;
    }

    initiate_data_transfer(client);
    
    // Receive list of files
    receive_response_then_print(client->data_sockfd);

    end_data_transfer(client);

    // Stop listening for new connections
    close(client->data_listen_sockfd);
    client->data_listen_sockfd = -1;

    receive_response_then_print(client->control_sockfd);
}

void execute_command_store(struct client_state *client, char *command) {
    static char buf[COMMAND_STR_MAX];

    // TODO: Read file path from 'command'. Similar logic to handle_command_username() in server.c
    // The path should contain no '/'
    // Then, validate that a file exists at this path

    // Command has been validated
    // Start listening on some port for data, send the port
    listen_port(0, &(client->data_listen_sockfd), &(client->data_port));
    if (send_data_port(client) == -1) {
        return;
    }

    // Send the command, get server response
    send_message(client->control_sockfd, command);
    if (!receive_response_then_check_first_token_then_print_response_if_ok(client->control_sockfd, "150")) {
        return;
    }
    
    initiate_data_transfer(client);
    
    // TODO: Send file through the data socket
    // This should be a function probably in common.h, because it will also be used
    // by the server

    end_data_transfer(client);

    // Stop listening for new connections
    close(client->data_listen_sockfd);
    client->data_listen_sockfd = -1;
    
    receive_response_then_print(client->control_sockfd);
}

void execute_command_retrieve(struct client_state *client, char *command) {
    static char buf[COMMAND_STR_MAX];

    // Start listening on some port for data, send the port
    listen_port(0, &(client->data_listen_sockfd), &(client->data_port));
    if (send_data_port(client) == -1) {
        return;
    }

    // Send the command, get server response
    send_message(client->control_sockfd, command);
    if (!receive_response_then_check_first_token_then_print_response_if_ok(client->control_sockfd, "150")) {
        return;
    }

    initiate_data_transfer(client);

    // TODO: Receive file from data socket
    // Possibly by receiving it in buffer, COMMAND_STR_MAX bytes at a time,
    // and simultaneously writing it to disk?
    // This should be a function probably in common.h, because it will also be used
    // by the server
    
    end_data_transfer(client);

    // Stop listening for new connections
    close(client->data_listen_sockfd);
    client->data_listen_sockfd = -1;
    
    receive_response_then_print(client->control_sockfd);
}
