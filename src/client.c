#include "client.h"
#include "common.h"

#include <libgen.h>
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
    client.data_listen_port = -1;
    client.data_sockfd = -1;
    
    // Connect to the server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPV4
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);

    connect_to_addr(server_addr, &(client.control_sockfd), &(client.control_port));
    
    if (!receive_message_then_print_then_check_first_token(client.control_sockfd, "220")) {
        // Server didn't reply with proper welcome message
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
        
        // Handle or execute the entered command
        if (check_first_token(command, COMMAND_LIST_CLIENT)) {
            print_files_current_directory();
        } else if (check_first_token(command, COMMAND_CHANGE_DIRECTORY_CLIENT)) {
            execute_command_change_directory_client(command);
        } else if (check_first_token(command, COMMAND_PRINT_DIRECTORY_CLIENT)) {
            print_current_directory();
        } else if (check_first_token(command, COMMAND_LIST)) {
            execute_command_list(client);
        } else if (check_first_token(command, COMMAND_STORE)) {
            execute_command_store(client, command);
        } else if (check_first_token(command, COMMAND_RETRIEVE)) {
            execute_command_retrieve(client, command);
        } else {
            // Whichever command it is, it is handled by sending it to the server
            // and just printing the response
            send_message(client->control_sockfd, command);
            receive_message_then_print(client->control_sockfd);

            // If the command is QUIT, exit the loop
            if (check_first_token(command, COMMAND_QUIT)) {
                break;
            }
        }
    }
}

void listen_on_next_free_port(struct client_state *client) {
    // We start searching for data listen port from the control port
    if (client->data_listen_port == -1) {
        client->data_listen_port = client->control_port;
    }

    // Find a data port to connect to
    while (1) {
        int next_free_port = get_next_free_port(client->data_listen_port);
        if (next_free_port == -1) {
            fprintf(stderr, "Error: Could not find any free port\n");
            exit(EXIT_FAILURE);
        }

        client->data_listen_port = next_free_port;
        
        // Try to listen on the port
        if (listen_port(next_free_port, &(client->data_listen_sockfd), NULL) == -1) {
            // The port was taken by the time we could use it
            continue;
        }

        // We succeeded in listening
        break;
    }
}

int send_data_listen_port(struct client_state *client) {
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
    int port = client->data_listen_port;
    for (int i = 1; i >= 0; i--) {
        int x = (port >> (8 * i)) & 0xff;
        p += sprintf(buf + p, "%d", x);
        if (i) p += sprintf(buf + p, ",");
    }

    // Send to server
    send_message(client->control_sockfd, buf);

    // If the server returns a message with code 200, return 0 (success). Otherwise return -1
    return receive_message_then_print_then_check_first_token(client->control_sockfd, "200")
        ? 0
        : -1;
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
}

void execute_command_list(struct client_state *client) {
    // Start listening on some port for data, send the port
    listen_on_next_free_port(client);
    if (send_data_listen_port(client) == -1) {
        return;
    }
    
    // Send the command, get server response
    send_message(client->control_sockfd, COMMAND_LIST);
    if (!receive_message_then_print_then_check_first_token(client->control_sockfd, "150")) {
        return;
    }

    // Initiate the data connection, wait for the server to connect,
    // receive and print the list of files, then close the connection
    initiate_data_transfer(client);
    receive_message_then_print(client->data_sockfd);
    end_data_transfer(client);

    // Stop listening for new connections
    close(client->data_listen_sockfd);
    client->data_listen_sockfd = -1;

    // Receive and print (hopefully) success message
    receive_message_then_print(client->control_sockfd);
}

void execute_command_store(struct client_state *client, char *command) {
    static char buf[COMMAND_STR_MAX];

    // Extract the path from the command and ensure it points to a file
    strtok(command, " ");
    char *path = strtok(NULL, " ");
    if (!is_path_file(path)) {
        printf("Error: Given path is not a file\n");
        return;
    }

    // Extract the filename from the path
    // Compose the message to send to the server
    char *filename = basename(path);
    sprintf(buf, "%s %s", COMMAND_STORE, filename);

    // Start listening on some port for data, send the port
    listen_on_next_free_port(client);
    if (send_data_listen_port(client) == -1) {
        return;
    }

    // Send the command message, get server response
    send_message(client->control_sockfd, buf);
    if (!receive_message_then_print_then_check_first_token(client->control_sockfd, "150")) {
        return;
    }
    
    // Initiate the data connection, wait for server to connect, then send the file
    initiate_data_transfer(client);
    send_file(client->data_sockfd, path);
    end_data_transfer(client);

    // Stop listening for new connections
    close(client->data_listen_sockfd);
    client->data_listen_sockfd = -1;
    
    // Receive and print (hopefully) success message
    receive_message_then_print(client->control_sockfd);
}

void execute_command_retrieve(struct client_state *client, char *command) {
    static char buf[COMMAND_STR_MAX];

    // Extract the filename
    strtok(command, " ");
    char *filename = strtok(NULL, " ");

    // Ensure only a file is being asked for (no directory changing or relative paths)
    // char *last_slash = strrchr(filename, '/');
    // if (last_slash != NULL) {
    //     printf("Error: Must provide only a filename\n");
    //     return;
    // }

    // Start listening on some port for data, send the port
    listen_on_next_free_port(client);
    if (send_data_listen_port(client) == -1) {
        return;
    }

    // Prepare the command message to send;
    // Send the message, get server response
    sprintf(buf, "%s %s", COMMAND_RETRIEVE, filename);
    send_message(client->control_sockfd, buf);
    if (!receive_message_then_print_then_check_first_token(client->control_sockfd, "150")) {
        return;
    }

    initiate_data_transfer(client);
    save_file(client->data_sockfd, filename);
    end_data_transfer(client);

    // Stop listening for new connections
    close(client->data_listen_sockfd);
    client->data_listen_sockfd = -1;
    
    // Receive and print (hopefully) success message
    receive_message_then_print(client->control_sockfd);
}

void execute_command_change_directory_client(char *command) {
    strtok(command, " ");
    char *path = strtok(NULL, " ");

    // Changing directories on the client-side does not require us to print server codes,
    // that is only necessary for server responses
    if (!is_path_directory(path)) {
        printf("Error: Given path is not a directory\n");
        return;
    }

    // Change the directory
    chdir(path);
    
    printf("Directory changed to ");
    print_current_directory();
}

void print_current_directory() {
    static char path[PATH_MAX];
    
    // Get the current working directory
    if (getcwd(path, sizeof(path)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    printf("%s\n", path);
}

void print_files_current_directory() {
    static char path[PATH_MAX];
    static char result[COMMAND_STR_MAX];

    // Get the current working directory
    if (getcwd(path, sizeof(path)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // List the files in the directory
    list_directory(path, result, sizeof(result));

    // Print the result
    printf("%s\n", result);
}
