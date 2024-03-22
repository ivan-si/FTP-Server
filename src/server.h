#ifndef SERVER_H_
#define SERVER_H_

#include <limits.h>
#include <netinet/in.h>
#include <sys/select.h>

#define AUTH_STR_MAX 128

#define BACKLOG 10

#define SERVER_CLIENT_STATE_NEED_USERNAME 0
#define SERVER_CLIENT_STATE_NEED_PASSWORD 1
#define SERVER_CLIENT_STATE_AUTHENTICATED 2

/**
 * @brief Linked list of usernames and passwords
 */
struct user_auth_data {
    char username[AUTH_STR_MAX];
    char password[AUTH_STR_MAX];

    struct user_auth_data *next;
};

/**
 * @brief State of a connected client
 */
struct server_client_state {
    int sockfd;
    int state;
    struct user_auth_data *auth_data;
    char current_path[PATH_MAX];
    int has_data_addr;
    struct sockaddr_in data_addr;
    struct server_client_state *next;
};

/**
 * @brief State of the server
 */
struct server_state {
    char base_path[PATH_MAX]; // The root directory for all server files
    char users_storage_path[PATH_MAX]; // The directory which will store a list of directories, one for each user
    struct user_auth_data *users_auth_data;
    int control_sockfd;
    struct server_client_state *clients;
    fd_set listen_sockfds;
};

/**
 * @brief Create (if needed) the base and users storage directories
 */
void initialize_server_directories(struct server_state *server);

/**
 * @brief Manage new incoming control connections and established connections
 */
void monitor_control_port(struct server_state *server);

void add_new_client(struct server_state *server, int client_sockfd);

void remove_client(struct server_state *server, struct server_client_state *client);

struct server_client_state* find_client_by_sockfd(struct server_state *server, int sockfd);

/**
 * @brief Read the auth data *usernames and passwords from the users.txt file and store them
 */
void read_auth_data(struct server_state *server);

void add_auth_data(struct server_state *server, char *username, char *password);

struct user_auth_data* find_auth_data_by_username(struct server_state *server, char *username);

void initialize_user_storage_directory(struct server_state *server, struct user_auth_data *auth_data);

void initialize_current_path(struct server_state *server, struct server_client_state *client);

/**
 * @brief Handle an incoming client command.
 */
void handle_command(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_username(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_password(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_port(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_store(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_retrieve(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_list(struct server_state *server, struct server_client_state *client);

void handle_command_change_directory(struct server_state *server, struct server_client_state *client, char *command);

void handle_command_print_directory(struct server_state *server, struct server_client_state *client);

void handle_command_quit(struct server_state *server, struct server_client_state *client);

#endif
