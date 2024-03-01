#ifndef _COMMON_H_
#define _COMMON_H_

// TODO: Change to 21, 20
#define SERVER_CONTROL_PORT 2100
#define SERVER_DATA_PORT 2000

#define COMMAND_STR_MAX 1024

extern const char
    *COMMAND_USERNAME,
    *COMMAND_PASSWORD,
    *COMMAND_PORT,
    *COMMAND_STORE,
    *COMMAND_RETRIEVE,
    *COMMAND_LIST,
    *COMMAND_CHANGE_DIRECTORY,
    *COMMAND_PRINT_DIRECTORY,
    *COMMAND_QUIT;

/**
 * @brief Create the directory if it does not exist yet
 */
void create_directory_if_not_exists(char *path);

void list_directory(char *path);

/**
 * @brief Check if the prefix of string is equal to prefix.
 * string_length must be positive.
 * 
 * @return int 1 if equal, 0 otherwise
 */
int check_prefix(const char *string, const char *prefix);

/**
 * @brief Create a TCP socket, bind it to the given port, and start listening for
 * incoming connections.
 * 
 * @param port The port to bind the socket to. If 0, binds to any port.
 * @param sockfd_result Location to store the new socket file descriptor.
 * @param port_result Location to store the port the socket was bound to.
 */
void listen_port(int port, int *sockfd_result, int *port_result);

#endif
