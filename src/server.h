#ifndef SERVER_H_
#define SERVER_H_

#include <limits.h>
#include <netinet/in.h>
#include <sys/select.h>

#define AUTH_STR_MAX (128)

#define SERVER_CLIENT_STATE_NEED_USERNAME (0)
#define SERVER_CLIENT_STATE_NEED_PASSWORD (1)
#define SERVER_CLIENT_STATE_AUTHENTICATED (2)

/**
 * @brief Linked list of usernames and passwords
 */
struct user_auth_data {
    char username[AUTH_STR_MAX];
    char password[AUTH_STR_MAX];

    struct user_auth_data *next;
};

/**
 * @brief Linked list of state of connected clients
 */
struct server_client_state {
    int control_sockfd;                 // The socket for the control connection to the client
    int state;                          // The state of the client (need username, need password, authenticated)
    struct user_auth_data *auth_data;   // The authentication data for this client, partially or fully entered
    char current_path[PATH_MAX];        // The current path (working directory) for the client on the server
    int has_data_addr;                  // Whether the client has given their data_addr
    struct sockaddr_in data_addr;       // The client's address for an impending data connection, received with the PORT command

    struct server_client_state *next;   // The next client_state in the linked list
};

/**
 * @brief State of the server
 */
struct server_state {
    char base_path[PATH_MAX];               // The root directory for all server files
    char users_storage_path[PATH_MAX];      // The directory which will store a list of directories, one for each user
    struct user_auth_data *users_auth_data; // All user authentication data
    int control_sockfd;                     // Socket for accepting new clients and establishing control connections
    struct server_client_state *clients;    // All connected clients
    fd_set listen_sockfds;                  // Set of sockets to asynchronously listen to for incoming data
};

/**
 * @brief Create (if needed) the base and users storage directories
 */
void initialize_server_directories(struct server_state *server);

/**
 * @brief Initialize (create if they do not exist) the storage directories
 * for all users
 * 
 * @param server 
 */
void initialize_user_storage_directories(struct server_state *server);

/**
 * @brief Initialize the current path field of the client, setting it equal to their
 * user storage directory
 * 
 * @param server 
 * @param client 
 */
void initialize_current_path(struct server_state *server, struct server_client_state *client);

/**
 * @brief Manage new incoming control connections and established connections
 */
void monitor_control_port(struct server_state *server);

/**
 * @brief Handle the receiving data from the client through its appropriate control socket
 * 
 * @param server 
 * @param client 
 */
void handle_client_sending_data(struct server_state *server, struct server_client_state *client);

/**
 * @brief Add a new client with the given control socket file descriptor
 * 
 * @param server 
 * @param client_sockfd 
 */
void add_new_client(struct server_state *server, int client_sockfd);

/**
 * @brief Remove the client from the list of clients
 * 
 * @param server 
 * @param client 
 */
void remove_client(struct server_state *server, struct server_client_state *client);

/**
 * @brief Find a client by the given control socket file descriptor
 * 
 * @param server 
 * @param control_sockfd 
 * @return The client state, or NULL if not found 
 */
struct server_client_state* find_client_by_control_sockfd(struct server_state *server, int control_sockfd);

/**
 * @brief Read the auth data usernames and passwords from the users.txt file and store them
 */
void read_auth_data(struct server_state *server);

/**
 * @brief Add authentication data with the given username and password
 * 
 * @param server 
 * @param username 
 * @param password 
 */
void add_auth_data(struct server_state *server, char *username, char *password);

/**
 * @brief Find authentication data with the given username
 * 
 * @param server 
 * @param username 
 * @return The authentication data, or NULL if not found
 */
struct user_auth_data* find_auth_data_by_username(struct server_state *server, char *username);

/**
 * @brief Handle an incoming client command
 */
void handle_command(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_username(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_password(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_port(struct server_client_state *client, char *command);

void handle_command_store(struct server_client_state *client, char *command);

void handle_command_retrieve(struct server_client_state *client, char *command);

void handle_command_list(struct server_client_state *client);

void handle_command_change_directory(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_print_directory(struct server_state *server, struct server_client_state *client);

void handle_command_quit(struct server_state *server, struct server_client_state *client);

#endif
