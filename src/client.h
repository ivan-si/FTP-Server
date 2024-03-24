#ifndef CLIENT_H_
#define CLIENT_H_

#include <limits.h>

extern const char
    *COMMAND_LIST_CLIENT,
    *COMMAND_CHANGE_DIRECTORY_CLIENT,
    *COMMAND_PRINT_DIRECTORY_CLIENT;

/**
 * @brief State of the client
 */
struct client_state {
    int control_sockfd;     // The socket for the control connection to the server
    int control_port;       // The port for the control connection to the server
    int data_listen_sockfd; // The socket currently being used to listen for incoming 
                            // connections to establish a data connection to the server
    int data_listen_port;   // The port associated with data_listen_sockfd
    int data_sockfd;        // The socket used for the current established data connection
};

/**
 * @brief Present a CLI. Receive commands from stdin and handle / execute them
 * 
 * @param client 
 */
void get_commands(struct client_state *client);

/**
 * @brief Listen for data connections on the next adjacent free port, initially
 * starting from one above the control port. Every subsequent call will search
 * for the closest higher-numbered port, or if all are taken, the smallest free port.
 * 
 * @param client 
 */
void listen_on_next_free_port(struct client_state *client);

/**
 * @brief Send the data listening port to the server
 * 
 * @param client 
 * @return 0 if success, -1 otherwise
 */
int send_data_listen_port(struct client_state *client);

/**
 * @brief Initiate a data transfer by waiting for the server to establish a TCP
 * connection to the data listening socket, and accepting it
 * 
 * @param client 
 */
void initiate_data_transfer(struct client_state *client);

/**
 * @brief End a data transfer by closing the data socket
 * 
 * @param client 
 */
void end_data_transfer(struct client_state *client);

void execute_command_list(struct client_state *client);

void execute_command_store(struct client_state *client, char *command);

void execute_command_retrieve(struct client_state *client, char *command);

void execute_command_change_directory_client(char *command);

void print_current_directory();

void print_files_current_directory();

#endif
