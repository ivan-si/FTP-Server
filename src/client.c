#include "client.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
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
    client.state = CLIENT_STATE_CONTROL;

    connect_to_address_and_port(inet_addr("127.0.0.1"), SERVER_CONTROL_PORT, &(client.control_sockfd));

    get_commands(&client);

    return EXIT_SUCCESS;
}

void get_commands(struct client_state *client) {
    static char command[COMMAND_STR_MAX];

    while (1) {
        printf("ftp> ");
        
        // Get a line from stdin
        fgets(command, sizeof(command), stdin);
        
        int command_length = strlen(command);
        printf("Command length: %d\n", command_length);
        if (command_length <= 1) {
            // Only newline
            continue;
        }

        // Remove the newline at the end
        command[--command_length] = '\0';

        if (check_prefix(command, COMMAND_LIST_CLIENT)) {
            // TODO: Implement these functions
            // Will need to initialize client->current_path in main() first and then use it here
        } else if (check_prefix(command, COMMAND_CHANGE_DIRECTORY_CLIENT)) {

        } else if (check_prefix(command, COMMAND_PRINT_DIRECTORY_CLIENT)) {
        
        } else if (check_prefix(command, COMMAND_LIST)) {
            execute_command_list(client);
        } else if (check_prefix(command, COMMAND_STORE)) {
            execute_command_store(client, command);
        } else if (check_prefix(command, COMMAND_RETRIEVE)) {
            execute_command_retrieve(client, command);
        } else {
            // This includes PORT (which effectively does nothing since it is
            // sent again before LIST, STORE, or RETRIEVE)

            // TODO: Handle "PORT" being entered with nothing else hanging the program
            send_then_print_response(client->control_sockfd, command);

            if (check_prefix(command, COMMAND_QUIT)) {
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
    // Add port to buffer
    int port = client->data_port;
    for (int i = 1; i >= 0; i--) {
        int x = (port >> (8 * i)) & 0xff;
        p += sprintf(buf + p, "%d", x);
        if (i) p += sprintf(buf + p, ",");
    }

    // Send to server
    printf("Sending address %d and port %d as: %s\n", address, client->data_port, buf);
    send_message(client->control_sockfd, buf);

    // Wait for response from server
    char response[COMMAND_STR_MAX];
    int bytes_received = recv(client->control_sockfd, response, COMMAND_STR_MAX - 1, 0);

    if (bytes_received == -1) {
        perror("recv");
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        // Server closed the connection unexpectedly
        fprintf(stderr, "Server closed the connection unexpectedly.\n");
        exit(EXIT_FAILURE);
    } else {
        response[bytes_received] = '\0';
        // Check if server sent proper response
        if (check_prefix(response, "200")) {
            // Server sent OK response
            return 0;
        } else {
            // Server sent an error response
            fprintf(stderr, "Server response: %s\n", response);
            return -1;
        }
    }
}

void initiate_data_transfer(struct client_state *client) {
    // Wait for server to ask to establish TCP connection, and accept it
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    printf("Starting to wait for accept\n");
    int data_sockfd = accept(client->data_listen_sockfd, (struct sockaddr *) &server_addr, &addr_len);
    if (data_sockfd == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    
    printf("Accept success\n");
    client->data_sockfd = data_sockfd;
    client->state = CLIENT_STATE_DATA;
}

void end_data_transfer(struct client_state *client) {
    close(client->data_listen_sockfd);
    close(client->data_sockfd);
    client->data_listen_sockfd = -1;
    client->data_sockfd = -1;
    client->data_port = -1;
    client->state = CLIENT_STATE_CONTROL;
}

void execute_command_list(struct client_state *client) {
    static char buf[COMMAND_STR_MAX];

    // Start listening on some port for data, send the port
    listen_port(0, &(client->data_listen_sockfd), &(client->data_port));
    if (send_data_port(client) == -1) {
        return;
    }
    
    // Send the command
    send_then_print_response(client->control_sockfd, COMMAND_LIST);
    initiate_data_transfer(client);
    
    // TODO: recv() files from data port
    // Could just be a string like "file1.txt\nimage2.txt\ndoc1.pdf\nfile2.txt"

    end_data_transfer(client);
}

void execute_command_store(struct client_state *client, char *command) {
    // TODO: Read file path from 'command'. Similar logic to handle_command_username() in server.c
    // The path should contain no '/'
    // Then, validate that a file exists at this path

    // Command has been validated
    
    // Start listening on some port for data, send the port
    listen_port(0, &(client->data_listen_sockfd), &(client->data_port));
    if (send_data_port(client) == -1) {
        return;
    }

    // Send the command
    send_then_print_response(client->control_sockfd, command);
    initiate_data_transfer(client);
    
    // TODO: Ensure server sent proper response back

    
    // TODO: Send file through the data socket
    // This should be a function probably in common.h, because it will also be used
    // by the server

    end_data_transfer(client);
}

void execute_command_retrieve(struct client_state *client, char *command) {
    static char buf[COMMAND_STR_MAX];

    // Start listening on some port for data, send the port
    listen_port(0, &(client->data_listen_sockfd), &(client->data_port));
    if (send_data_port(client) == -1) {
        return;
    }

    // Send the command
    send_then_print_response(client->control_sockfd, command);
    initiate_data_transfer(client);
    
    // TODO: Ensure server sent proper response back

    
    // TODO: Receive file from data socket
    // Possibly by receiving it in buffer, COMMAND_STR_MAX bytes at a time,
    // and simultaneously writing it to disk?
    // This should be a function probably in common.h, because it will also be used
    // by the server
    
    end_data_transfer(client);
}
