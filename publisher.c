#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>

#define IP_ADDRESS "127.0.0.1"
#define PORT 12345
#define DEFAULT_BUFLEN 512

// Command types
#define CMD_EXIT        "/exit\n"

bool server_disconnected = false;
pthread_mutex_t server_disconnected_mutex = PTHREAD_MUTEX_INITIALIZER;

void set_exit_flag(void)
{
    pthread_mutex_lock(&server_disconnected_mutex);
    server_disconnected = true;
    pthread_mutex_unlock(&server_disconnected_mutex);
}

bool should_exit(void)
{
    bool val;
    pthread_mutex_lock(&server_disconnected_mutex);
    val = server_disconnected;
    pthread_mutex_unlock(&server_disconnected_mutex);
    return val;
}

void *monitor_server_disconnect(void *arg)
{
    int sock = *(int *)arg;
    char buf[DEFAULT_BUFLEN];

    while (!should_exit())
    {
        int n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0)
        {
            set_exit_flag();
            fprintf(stderr, "Server disconnected. Press Enter to exit, any other input will be ignored.\n");
            fflush(stderr);
        }
    }

    return NULL;
}

/*
Expected message format:
[topic] "text"
*/

int valid_message_format(const char *msg)
{
    char tmp[DEFAULT_BUFLEN];
    strncpy(tmp, msg, DEFAULT_BUFLEN - 1);
    tmp[DEFAULT_BUFLEN - 1] = '\0';

    size_t len = strlen(tmp);
    while (len > 0)
    {
        char c = tmp[len - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            break;
        tmp[len - 1] = '\0';
        len--;
    }

    //shortest format [a] "b"
    if(strlen(tmp) < 7) return 0;
    
    if(tmp[0] != '[') return 0;

    int i = 0;
    while(tmp[i] != ']' && tmp[i] != '\0') ++i;
    if(tmp[i] != ']') return 0;
    //empty topic: [] "text"
    if(tmp[i - 1] == '[') return 0;

    if(i < strlen(tmp) && tmp[++i] != ' ') return 0;

    if(i < strlen(tmp) && tmp[++i] != '"') return 0;
    //i is index of first "

    if(tmp[strlen(tmp) - 1] != '"') return 0;
    //only one ": [topic] "
    if(i == strlen(tmp) - 1) return 0;
    //empty text: [topic] ""
    if(tmp[strlen(tmp) - 2] == '"' && i == strlen(tmp) - 2) return 0;

    return 1;
}

int main(int argc, char *argv[])
{

    if(argc != 3)
    {
        fprintf(stderr, "Correct usage: %s <server_ip> <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    if(server_port <= 0 || server_port > 65535)
    {
        fprintf(stderr, "Invalid port number.\n");
        return EXIT_FAILURE;
    }

    int client_socket_fd;
    char message[DEFAULT_BUFLEN];

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
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = inet_addr(server_ip);

    // Connect to server
    if (connect(client_socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("failed to connect");
        return EXIT_FAILURE;
    }

    printf("Connected to server [%s:%d]\n", server_ip, server_port);
    printf("Type /exit to quit\n");
    printf("Publish format: [topic] \"text\" \n\n");

    // Send a message to the server to indicate whether this client is a publisher or subscriber
    const char *role_msg = "PUBLISHER";
    if (send(client_socket_fd, role_msg, strlen(role_msg), 0) < 0) 
    {
        perror("failed to send client role to server");
        if(client_socket_fd != -1)
            close(client_socket_fd);
        return EXIT_FAILURE;
    }

    pthread_t monitor_tid;
    if (pthread_create(&monitor_tid, NULL, monitor_server_disconnect, &client_socket_fd) != 0)
    {
        perror("failed to start disconnect monitor");
        if(client_socket_fd != -1)
            close(client_socket_fd);
        return EXIT_FAILURE;
    }
    pthread_detach(monitor_tid);

    while (1)
    {
        fflush(stdout);
        memset(message, '\0', DEFAULT_BUFLEN);

        fgets(message, DEFAULT_BUFLEN, stdin);

        if (should_exit())
        {
            break;
        }

        if(strcmp(message, CMD_EXIT) == 0)
        {
            printf("Disconnecting...\n");
            sleep(2);
            printf("Disconnected.\n");
            break;
        }

        if(strlen(message) >= DEFAULT_BUFLEN - 1)
        {
            printf("ERROR: Message too long. Max %d characters allowed.\n", DEFAULT_BUFLEN - 1);
        }else if(!valid_message_format(message))
        {
            printf("ERROR: Invalid publish format.\n");
            printf("Correct formats:\n\t1. [topic] \"news\" ");
            printf("\n\t2. /exit\n");
        }
        else
        {
            if(send(client_socket_fd, message, strlen(message), 0) < 0) 
            {
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    fprintf(stderr, "Server disconnected. Publisher will exit.\n");
                    close(client_socket_fd);
                    break;
                }
                perror("failed to send publish message");   
                close(client_socket_fd);
                exit(EXIT_FAILURE);
            }  
        }
    }

    close(client_socket_fd);
    return 0;
}
