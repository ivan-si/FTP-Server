#ifndef _COMMON_H_
#define _COMMON_H_

#include <limits.h>
#include <netinet/in.h>

// TODO: Change to 21, 20
#define SERVER_CONTROL_PORT (2100)
#define SERVER_DATA_PORT (2000)

#define COMMAND_STR_MAX (2 * PATH_MAX)

#define FILE_TRANSFER_BUFFER_SIZE (1024)

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

int list_directory(char *path, char *result, int result_size);

int is_path_directory(char *path);

int is_path_file(char *path);

int check_first_token(const char *string, const char *token);

/**
 * @brief Create a TCP socket, bind it to the given port, and start listening for
 * incoming connections.
 * 
 * @param port The port to bind the socket to. If 0, binds to any port.
 * @param sockfd_result Location to store the new socket file descriptor.
 * @param port_result Location to store the port the socket was bound to.
 */
void listen_port(int port, int *sockfd_result, int *port_result);

void connect_to_addr(struct sockaddr_in addr, int *result_sockfd);

void send_message(int sockfd, const char *message);

void send_file(int sockfd, const char *path);

void save_file(int sockfd, const char *path);

void receive_message_then_print(int sockfd);

int receive_message_then_print_then_check_first_token(int sockfd, const char *expected);

#endif
