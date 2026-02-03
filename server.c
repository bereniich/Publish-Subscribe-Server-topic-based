#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "list.h"

#define PORT            12345
#define DEFAULT_BUFLEN  512
#define MAX_CLIENTS     20

typedef enum 
{
    CMD_NONE,
    CMD_SUBSCRIBE,
    CMD_UNSUBSCRIBE,
    CMD_LIST_TOPICS
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

    if (strncmp(msg, "[LIST_TOPICS] ", 14) == 0)
    {
        *topics_start = NULL;
        return CMD_LIST_TOPICS;
    }

    return CMD_NONE;
}

// Send news to all subscribers of specific topic 
void send_to_subscribers(TOPIC* topic, const char* msg, int sender_socket)
{
    if (!topic) return;
    printf("[PUBLISH] Sending message on topic '%s': %s\n", topic->name, msg);

    SUBSCRIBER* sub = topic->subscribers;
    while(sub)
    {
        if(sub->socket != -1 && send(sub->socket, msg, strlen(msg), 0) < 0) 
            perror("send to subscriber failed");
    
        sub = sub->next;
    }
}

void send_topics_to_subscribers(int socket)
{
    pthread_mutex_lock(&topicRegistry_mtx);
    {
        char topic_list[DEFAULT_BUFLEN] = "[SERVER] Topics: ";
        TOPIC *t = topicRegistry.firstNode;

        while(t)
        {
            strncat(topic_list, t->name, DEFAULT_BUFLEN - strlen(topic_list) - 1);
            strncat(topic_list, " ", DEFAULT_BUFLEN - strlen(topic_list) - 1);
            t = t->nextTopic;
        }
        send(socket, topic_list, strlen(topic_list), 0);
    }
    pthread_mutex_unlock(&topicRegistry_mtx);
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
        buffer[read_size] = '\0';
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
    
    printf("[INFO] Publisher (socket = %d) disconnected.\n", client->socket);

    if(client->socket != -1)
        close(client->socket);
    free(client);

    return NULL;
}

// Function handling SUBSCRIBE and UNSUBSCRIBE commands 
void subscriberCommand(char *topics_str, server_cmd_t cmd, int socket)
{

    if(cmd == CMD_LIST_TOPICS)
    {
        send_topics_to_subscribers(socket);
        return;
    }

    char *topicName = strtok(topics_str, " \n");

    while (topicName)
    {
        switch (cmd)
        {
            case CMD_SUBSCRIBE:
                pthread_mutex_lock(&topicRegistry_mtx);
                {
                    if(addSubscriberToTopic(&topicRegistry, topicName, socket) == 0)
                        printf("[SUBSCRIBE] Client %d  subscribed to topic '%s'\n", socket, topicName);
                }
                pthread_mutex_unlock(&topicRegistry_mtx);
                
                break;

            case CMD_UNSUBSCRIBE:
                pthread_mutex_lock(&topicRegistry_mtx);
                {
                    TOPIC *topic = findTopic(&topicRegistry, topicName);   
                    if(removeSubscriberFromTopic(topic, socket) == 0) 
                        printf("[UNSUBSCRIBE] Client %d unsubscribed from topic '%s'\n", socket, topicName);
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
    send_topics_to_subscribers(sock);

    int read_size = 0;
    char buffer[DEFAULT_BUFLEN];
    char *topics_start; 

    memset(buffer, 0, DEFAULT_BUFLEN);
    // Get command from a subscriber
    while ((read_size = recv(sock, buffer, DEFAULT_BUFLEN - 1, 0)) > 0)
    {
        buffer[read_size] = '\0';

        server_cmd_t cmd = parse_server_command(buffer, &topics_start);
        subscriberCommand(topics_start, cmd, sock);
        memset(&buffer, '\0', DEFAULT_BUFLEN);
    }

    pthread_mutex_lock(&topicRegistry_mtx);
    removeSubscriberFromAllTopics(&topicRegistry, sock);
    printf("[INFO] Subscriber (socket = %d) disconnected.\n", client->socket);
    pthread_mutex_unlock(&topicRegistry_mtx);
    if(client->socket != -1)
        close(client->socket);
    free(client);

    return NULL;
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
    printf("Topic-based server listening on port %d...\n", PORT);

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
            printf("[INFO] New publisher (socket = %d) connected: %s:%d\n", client->socket, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
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
            printf("[INFO] New subscriber (socket = %d) connected: %s:%d\n", client->socket, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

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
    
    // Destroy topics
    pthread_mutex_lock(&topicRegistry_mtx);
    {
        destroyTopics(&topicRegistry);
    }
    pthread_mutex_unlock(&topicRegistry_mtx);

    return 0;
}
