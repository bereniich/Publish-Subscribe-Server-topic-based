#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define IP_ADDRESS "127.0.0.1"
#define PORT 12346
#define DEFAULT_BUFLEN 512

// UVESTI FLAG ZA DISKONEKCIJU 
// Kad se unese /exit

// Command types
#define CMD_EXIT        "/exit\n"
#define CMD_SUBSCRIBE   "[SUBSCRIBE] "
#define CMD_UNSUBSCRIBE "[UNSUBSCRIBE] "

typedef enum {
    CMD_INVALID,
    CMD_EXIT_TYPE,
    CMD_SUBSCRIBE_TYPE,
    CMD_UNSUBSCRIBE_TYPE
} command_type_t;

// Parses the input message and returns its command type.
command_type_t parse_command(const char *msg)
{
    if (strcmp(msg, CMD_EXIT) == 0)
        return CMD_EXIT_TYPE;

    if (strncmp(msg, CMD_SUBSCRIBE, strlen(CMD_SUBSCRIBE)) == 0 &&
        strlen(msg) > strlen(CMD_SUBSCRIBE))
        return CMD_SUBSCRIBE_TYPE;

    if (strncmp(msg, CMD_UNSUBSCRIBE, strlen(CMD_UNSUBSCRIBE)) == 0 &&
        strlen(msg) > strlen(CMD_UNSUBSCRIBE))
        return CMD_UNSUBSCRIBE_TYPE;

    return CMD_INVALID;
}

void *recv_thread(void *arg)
{
    int client_socket_fd = *(int *)arg;  // get socket from argument
    char buffer[DEFAULT_BUFLEN];
    int read_size;

    /* Expected message formats include:
        [SERVER] Subscription failed.
        [SERVER] Unsubscription failed.
        [TOPIC_NAME] News
    */

    while ((read_size = recv(client_socket_fd, buffer, DEFAULT_BUFLEN - 1, 0)) > 0)
    {
        buffer[read_size] = '\0';
        printf("%s\n> ", buffer);
        fflush(stdout);
    }

    if(read_size == 0) 
    {
        printf("\nServer disconnected.\n");
        if(client_socket_fd != -1)
            close(client_socket_fd);
    } 
    else 
    {
        perror("recv failed");
    }
    
    exit(0);
}

void *send_thread(void *arg)
{
    int client_socket_fd = *(int *)arg;  // get socket from argument
    char message[DEFAULT_BUFLEN];

    while (1)
    {
        printf("> ");
        fflush(stdout);

        fgets(message, DEFAULT_BUFLEN, stdin);

        switch (parse_command(message))
        {
            case CMD_EXIT_TYPE:
                printf("Disconnecting...\n");
                if(client_socket_fd != -1) {
                    close(client_socket_fd);
                    return NULL;
                }
                break;

            case CMD_SUBSCRIBE_TYPE:
                if (send(client_socket_fd, message, strlen(message), 0) < 0) 
                    perror("subscription failed");
                break;
            
            case CMD_UNSUBSCRIBE_TYPE:
                if (send(client_socket_fd, message, strlen(message), 0) < 0) 
                    perror("unsubscription failed");
                break;
            
            case CMD_INVALID:
            default:
                printf("ERROR: Invalid command.\n");
                printf("Allowed commands:\n");
                printf("  %s", CMD_EXIT);
                printf("  %stopic1 topic2 ... topicN\n", CMD_SUBSCRIBE);
                printf("  %stopic1 topic2 ... topicN\n", CMD_UNSUBSCRIBE);
                break;
        }
    }

    return NULL;
}

int main(void)
{
    int client_socket_fd;

    // Socket creation
    client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket_fd < 0)
    {
        perror("socket creation failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address;

    // Set up the server address structure
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr(IP_ADDRESS);

    // Connect to server
    if (connect(client_socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("failed to connect");
        return EXIT_FAILURE;
    }

    printf("Connected to server [%s:%d]\n", IP_ADDRESS, PORT);
    printf("Type /exit to quit\n\n");

    // Send a message to the server to indicate whether this client is a publisher or subscriber
    const char *role_msg = "SUBSCRIBER";
    if (send(client_socket_fd, role_msg, strlen(role_msg), 0) < 0) 
    {
        perror("failed to send client role to server");
        if(client_socket_fd != -1)
            close(client_socket_fd);
        return EXIT_FAILURE;
    }

    // Two separate threads are created to enable full-duplex TCP communication.
    pthread_t t_recv, t_send;

    if (pthread_create(&t_recv, NULL, recv_thread, &client_socket_fd) != 0) 
    {
        perror("pthread_create recv failed");
        if(client_socket_fd != -1)
            close(client_socket_fd);
        return EXIT_FAILURE;
    }
    if (pthread_create(&t_send, NULL, send_thread, &client_socket_fd) != 0) 
    {
        perror("pthread_create send failed");
        if(client_socket_fd != -1)
            close(client_socket_fd);
        return EXIT_FAILURE;
    }

    pthread_join(t_send, NULL);
    pthread_join(t_recv, NULL);

    if(client_socket_fd != -1)
        close(client_socket_fd);
    
    return 0;
}
