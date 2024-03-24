#ifndef _COMMON_H_
#define _COMMON_H_

#include <limits.h>
#include <netinet/in.h>

// NOTE: To have the ports be 21 and 20, you must run the server with 'sudo' privileges
// in order for the server to connect to these ports.
#define SERVER_CONTROL_PORT (2100)
#define SERVER_DATA_PORT (2000)

// The backlog for listen()ing for incoming TCP connections
#define LISTEN_BACKLOG (10)

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

/**
 * @brief List all entries on the filesystem at location path. Store results
 * as a newline-separated string in result. Store at most result_size letters.
 * 
 * @param path 
 * @param result 
 * @param result_size 
 * @return int 0 on success, -1 if the directory could not be found or listed.
 */
int list_directory(char *path, char *result, int result_size);

/**
 * @brief Check whether the path exists and is a directory
 * 
 * @param path 
 * @return 1 if true, 0 otherwise
 */
int is_path_directory(char *path);

/**
 * @brief Check whether the path exists and is a regular file
 * 
 * @param path 
 * @return 1 if true, 0 otherwise.
 */
int is_path_file(char *path);

/**
 * @brief Check if the first token, if interpreting the string as a space-separated
 * list of tokens, equals the given token
 * 
 * @param string 
 * @param token 
 * @return 1 if true, 0 otherwise
 */
int check_first_token(const char *string, const char *token);

/**
 * @brief Check whether it is possible to bind to the port
 * 
 * @param port 
 * @return int 
 */
int try_bind(int port);

/**
 * @brief Get the next free port after the given port, or before if all after are free
 * 
 * @param port 
 * @return The free port, or -1 if no port was found to be free
 */
int get_next_free_port(int port);

/**
 * @brief Create a TCP socket, bind it to the given port, and start listening for
 * incoming connections.
 * 
 * @param port The port to bind the socket to. If 0, binds to any port.
 * @param result_sockfd Location to store the new socket file descriptor.
 * @param result_port Location to store the port the socket was bound to.
 * @return 0 if success, -1 if fail (port was being used)
 */
int listen_port(int port, int *result_sockfd, int *result_port);

/**
 * @brief Connect to the address and port specified
 * 
 * @param addr Structure containing the information necessary to connect
 * @param result_sockfd Location to store the new socket file descriptor
 * @param port_result 
 */
void connect_to_addr(struct sockaddr_in addr, int *result_sockfd, int *result_port);

/**
 * @brief Send a message of bytes through the socket
 * 
 * @param sockfd 
 * @param message 
 */
void send_message(int sockfd, const char *message);

/**
 * @brief Send the file specified by the given path through the socket
 * 
 * @param sockfd 
 * @param path 
 */
void send_file(int sockfd, const char *path);

/**
 * @brief Receive a file through the socket and write it to the given path
 * 
 * @param sockfd 
 * @param path 
 */
void save_file(int sockfd, const char *path);

/**
 * @brief Receive a message through the socket and print it
 * 
 * @param sockfd 
 */
void receive_message_then_print(int sockfd);

/**
 * @brief Receive a message through the socket, print the message, and return whether
 * the first token of the message equals the expected string
 * 
 * @param sockfd 
 * @param expected 
 * @return 1 if the first token of the message equals the expected string, 0 otherwise
 */
int receive_message_then_print_then_check_first_token(int sockfd, const char *expected);

#endif
