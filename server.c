#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "list.h"

#define PORT            12346
#define DEFAULT_BUFLEN  512
#define MAX_CLIENTS     20

typedef enum 
{
    CMD_NONE,
    CMD_SUBSCRIBE,
    CMD_UNSUBSCRIBE
} server_cmd_t;

typedef enum 
{
    SUBSCRIBER_TYPE,
    PUBLISHER_TYPE
} client_type_t;

typedef struct client_st {
    int socket;
    client_type_t type;
} CLIENT;

// Registry of topcis
pthread_mutex_t topicRegistry_mtx = PTHREAD_MUTEX_INITIALIZER;
TOPIC_HEAD topicRegistry;  

// Command parse
server_cmd_t parse_server_command(char *msg, char **topics_start)
{
    if (strncmp(msg, "[SUBSCRIBE] ", 12) == 0)
    {
        *topics_start = msg + 12;
        return CMD_SUBSCRIBE;
    }

    if (strncmp(msg, "[UNSUBSCRIBE] ", 14) == 0)
    {
        *topics_start = msg + 14;
        return CMD_UNSUBSCRIBE;
    }

    return CMD_NONE;
}

// Send news to all subscribers of specific topic 
void send_to_subscribers(TOPIC* topic, const char* msg, int sender_socket)
{
    if (!topic) return;

    SUBSCRIBER* sub = topic->subscribers;
    while(sub)
    {
        if(send(sub->socket, msg, strlen(msg), 0) < 0) 
            perror("send to subscriber failed");
    
        sub = sub->next;
    }
}

// Publisher thread functions
void *handle_publisher(void *arg)
{
    CLIENT *client = (CLIENT *)arg;  
    int sock = client->socket;
    client_type_t type = client->type;

    char buffer[DEFAULT_BUFLEN];
    int read_size;
    char topicName[DEFAULT_BUFLEN];

    while((read_size = recv(client->socket, buffer, DEFAULT_BUFLEN - 1, 0)) > 0)
    {
        memset(&topicName, '\0', DEFAULT_BUFLEN);
        // Taking topic name out of received message
        int j = 0;
        for (int i = 1; i < read_size; i++)
        {
            if(buffer[i] == ']') break;
                topicName[j++] = buffer[i]; 
        }
        topicName[j] = '\0'; 

        // Send news to all subscribed clients 
        // and add topic to the registry if it's not already there
        pthread_mutex_lock(&topicRegistry_mtx);
        {
            TOPIC* topic = findTopic(&topicRegistry, topicName);
            if(!topic)
            {
                // Adding new topis to registry
                topic = createTopic(topicName);
                addTopic(&topicRegistry, topic);
            }

            // Multicast
            send_to_subscribers(topic, buffer, sock);
        }
        pthread_mutex_unlock(&topicRegistry_mtx);
    }
}

// Function handling SUBSCRIBE and UNSUBSCRIBE commands 
void subscriberCommand(char *topics_str, server_cmd_t cmd, int socket)
{
    char *topicName = strtok(topics_str, " \n");

    while (topicName)
    {
        printf("Topic: %s\n", topicName);

        switch (cmd)
        {
            case CMD_SUBSCRIBE:
                printf("SUBSCRIBE command received\n\n");
                pthread_mutex_lock(&topicRegistry_mtx);
                {
                    addSubscriberToTopic(&topicRegistry, topicName, socket);
                }
                pthread_mutex_unlock(&topicRegistry_mtx);
                
                break;

            case CMD_UNSUBSCRIBE:
                printf("UNSUBSCRIBE command received\n\n");
                pthread_mutex_lock(&topicRegistry_mtx);
                {
                    TOPIC *topic = findTopic(&topicRegistry, topicName);   
                    removeSubscriberFromTopic(topic, socket);
                }
                pthread_mutex_unlock(&topicRegistry_mtx);
                
                break;
        }
        topicName = strtok(NULL, " \n");
    }

    printTopicsAndSubscribers(&topicRegistry);
}


// Subscriber thread function
void *handle_subscriber(void *arg)
{
    CLIENT *client = (CLIENT *)arg;  
    int sock = client->socket;
    client_type_t type = client->type;

    // Send the list of topics to a subscriber
    pthread_mutex_lock(&topicRegistry_mtx);
    {
        TOPIC *t = topicRegistry.firstNode;
            char topic_list[DEFAULT_BUFLEN] = "[SERVER] Topics: ";
            while(t)
            {
                strcat(topic_list, t->name);
                strcat(topic_list, " ");
                t = t->nextTopic;
            }
        send(sock, topic_list, strlen(topic_list), 0);
    }
    pthread_mutex_unlock(&topicRegistry_mtx);

    int read_size = 0;
    char buffer[DEFAULT_BUFLEN];
    char *topics_start; 

    // Get command from a subscriber
    while ((read_size = recv(sock, buffer, DEFAULT_BUFLEN - 1, 0)) > 0)
    {
        buffer[read_size] = '\0';

        server_cmd_t cmd = parse_server_command(buffer, &topics_start);
        subscriberCommand(topics_start, cmd, sock);
    }

}

int main(void)
{
    // Topic registy initialization
    initTopic(&topicRegistry);

    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("socket failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        return 1;
    }

    listen(server_socket, MAX_CLIENTS);
    printf("Chat server listening on port %d...\n", PORT);

    int read_size = 0;
    char role_msg[DEFAULT_BUFLEN];

    while (1)
    {
        CLIENT* client = malloc(sizeof(CLIENT));
        if (!client) 
        {
            perror("malloc client");
            continue;
        }

        if ((client->socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len)) < 0)
        {
            perror("accept failed");
            continue;
        }

        memset(&role_msg, '\0', DEFAULT_BUFLEN);
        if((read_size = recv(client->socket, role_msg, DEFAULT_BUFLEN - 1, 0)) > 0)
        {
            role_msg[read_size] = '\0';
            fflush(stdout);
        }

        pthread_t tid;

        if(strcmp(role_msg, "PUBLISHER") == 0)
        {
            client->type = PUBLISHER_TYPE;
            printf("New publisher [%s:%d]\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
            if (pthread_create(&tid, NULL, handle_publisher, (void*)client) != 0) 
            {
                perror("pthread_create handleSubscriber failed");
                if(client->socket != -1) 
                    close(client->socket );
                free(client);
                return EXIT_FAILURE;
            }

            pthread_detach(tid);
        }
        else 
        {
            client->type = SUBSCRIBER_TYPE;
            printf("New subscriber [%s:%d]\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            if (pthread_create(&tid, NULL, handle_subscriber, (void*)client) != 0) 
            {
                perror("pthread_create handleSubscriber failed");
                if(client->socket != -1) 
                    close(client->socket );
                free(client);
                return EXIT_FAILURE;
            }

            pthread_detach(tid);
        }
    }

    close(server_socket);
    return 0;
}