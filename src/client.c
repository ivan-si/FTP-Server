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
    client.state = CLIENT_STATE_CONTROL;

    connect_to_server(&client);
    get_commands(&client);

    return EXIT_SUCCESS;
}

void connect_to_server(struct client_state *client) {
    // Get socket file descriptor
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("client: socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options to avoid bind() errors
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        perror("client: setsockopt");
        exit(EXIT_FAILURE);
    }

    // Specify address and port to connect socket to
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPV4
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);

    // Connect socket to server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("client: connect");
        exit(EXIT_FAILURE);
    }

    client->control_sockfd = sockfd;
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
            send_then_print_response(client, command);
        }
    }
}

void send_then_print_response(struct client_state *client, char *command) {
    static char buf[COMMAND_STR_MAX];

    // Send to server
    printf("Sending: %s\n", command);
    if (send(client->control_sockfd, command, strlen(command), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    // Receive response
    // TODO: Program often blocks here because the server isn't sending a response for most commands.
    // Once all handle() functions in the server are implemented, it shouldn't hang.
    int bytes_received = (int)recv(client->control_sockfd, buf, COMMAND_STR_MAX - 1, 0);
    if (bytes_received == -1) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    // Null-terminate the response
    buf[bytes_received] = '\0';
    
    // Print the response
    printf("%s\n", buf);
}

void send_data_port(struct client_state *client) {
    static char buf[COMMAND_STR_MAX];

    // Add command to buffer
    int p = sprintf(buf, "%s ", COMMAND_PORT);
    // Add address to buffer
    int address = INADDR_ANY;
    for (int i = 3; i >= 0; i--) {
        p += sprintf(buf + p, "%d,", (address >> (8 * i)) & 0xff);
    }
    // Add port to buffer
    int port = client->data_port;
    for (int i = 1; i >= 0; i--) {
        p += sprintf(buf + p, "%d", (port >> (8 * i)) & 0xff);
        if (i) p += sprintf(buf + p, ",");
    }

    // Send to server
    if (send(client->control_sockfd, buf, p, 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }
    
    // TODO: Ensure server sent proper response. If not, have to stop somehow
    // Can be done by making the function return 1 (true) if the server sent an OK response
    // and 0 (false) otherwise, and checking the return value in the places where
    // this function is used
}

void initiate_data_transfer(struct client_state *client) {
    listen_port(0, &(client->data_listen_sockfd), &(client->data_port));
    send_data_port(client);
    
    // Wait for server to ask to establish TCP connection, and accept it
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    int data_sockfd = accept(client->data_listen_sockfd, (struct sockaddr *) &server_addr, &addr_len);
    if (data_sockfd == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    
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

    // Send port, then send command to server
    send_data_port(client);
    if (send(client->control_sockfd, COMMAND_LIST, strlen(COMMAND_LIST), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    initiate_data_transfer(client);
    
    // TODO: recv() files from data port
    // Could just be a string like "file1.txt\nimage2.txt\ndoc1.pdf\nfile2.txt"

    end_data_transfer(client);
}

void execute_command_store(struct client_state *client, char *command) {
    // TODO: Read file path from 'command'. Similar logic to handle_command_username() in server.c
    // The path should contain no '/'
    // Then, validate that a file exists at this path

    // Command has been validated. Send port, then send command to server
    send_data_port(client);
    if (send(client->control_sockfd, command, strlen(command), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    // TODO: Ensure server sent proper response back

    initiate_data_transfer(client);

    // TODO: Send file through the data socket
    // This should be a function probably in common.h, because it will also be used
    // by the server

    end_data_transfer(client);
}

void execute_command_retrieve(struct client_state *client, char *command) {
    static char buf[COMMAND_STR_MAX];

    // Send port, then send command to server
    send_data_port(client);
    if (send(client->control_sockfd, command, strlen(command), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    // TODO: Ensure server sent proper response back

    initiate_data_transfer(client);

    // TODO: Receive file from data socket
    // Possibly by receiving it in buffer, COMMAND_STR_MAX bytes at a time,
    // and simultaneously writing it to disk?
    // This should be a function probably in common.h, because it will also be used
    // by the server
    
    end_data_transfer(client);
}
