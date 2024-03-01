#ifndef CLIENT_H_
#define CLIENT_H_

#include <limits.h>

extern const char
    *COMMAND_LIST_CLIENT,
    *COMMAND_CHANGE_DIRECTORY_CLIENT,
    *COMMAND_PRINT_DIRECTORY_CLIENT;

#define CLIENT_STATE_CONTROL 0
#define CLIENT_STATE_DATA 1

struct client_state {
    int state;
    int control_sockfd;
    int data_listen_sockfd;
    int data_sockfd;
    int data_port;
    char current_path[PATH_MAX];
};

void connect_to_server(struct client_state *client);

void get_commands(struct client_state *client);

void send_then_print_response(struct client_state *client, char *command);

void initiate_data_transfer(struct client_state *client);

void send_data_port(struct client_state *client);

void initiate_data_transfer(struct client_state *client);

void end_data_transfer(struct client_state *client);

void execute_command_list(struct client_state *client);

void execute_command_store(struct client_state *client, char *command);

void execute_command_retrieve(struct client_state *client, char *command);

#endif
