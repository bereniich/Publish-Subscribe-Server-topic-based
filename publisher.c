#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define IP_ADDRESS "127.0.0.1"
#define PORT 12345
#define DEFAULT_BUFLEN 512

// Command types
#define CMD_EXIT        "/exit\n"

/*
Expected message format:
[topic] "text"
*/

int valid_message_format(const char *msg)
{
    //shortest format [a] "b"
    if(strlen(msg) < 7) return 0;
    
    if(msg[0] != '[') return 0;

    int i = 0;
    while(msg[i] != ']' && msg[i] != '\0') ++i;
    if(msg[i] != ']') return 0;
    //empty topic: [] "text"
    if(msg[i - 1] == '[') return 0;

    if(i < strlen(msg) && msg[++i] != ' ') return 0;

    if(i < strlen(msg) && msg[++i] != '"') return 0;
    //i is index of first "

    if(msg[strlen(msg) - 1] == '\n')
    {
        if(msg[strlen(msg) - 2] != '"') return 0;
        //only one ": [topic] "
        if(i == strlen(msg) - 2) return 0;
        //empty text: [topic] ""
        if(msg[strlen(msg) - 3] == '"' && i == strlen(msg) - 3) return 0;
    }
    else 
    {
        if(msg[strlen(msg) - 1] != '"') return 0;
        //only one ": [topic] "
        if(i == strlen(msg) - 1) return 0;
        //empty text: [topic] ""
        if(msg[strlen(msg) - 2] == '"' && i == strlen(msg) - 2) return 0;
    }

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

    while (1)
    {
        fflush(stdout);
        memset(message, '\0', DEFAULT_BUFLEN);

        fgets(message, DEFAULT_BUFLEN, stdin);

        if(strcmp(message, CMD_EXIT) == 0)
        {
            printf("Disconnecting...\n");
            sleep(2);
            printf("Disconnected.\n");
            break;
        }
            
        if(!valid_message_format(message))
        {
            printf("ERROR: Invalid publish format.\n");
            printf("Correct format: [topic] \"news\" \n");
        }
        else
        {
            if(send(client_socket_fd, message, strlen(message), 0) < 0) 
            perror("failed to send publish message");   
        }  
    
    }

    close(client_socket_fd);
    return 0;
}
