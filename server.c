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
    SUBSCRIBER_TYPE,
    PUBLISHER_TYPE
} client_type_t;

typedef struct client_st {
    int socket;
    client_type_t type;
} CLIENT;

pthread_mutex_t topicRegistry_mtx = PTHREAD_MUTEX_INITIALIZER;
TOPIC_HEAD topicRegistry;  

// Send news to all subscribers of specific topic 
void send_to_subscribers(TOPIC* topic, const char* msg, int sender_socket)
{
    if (!topic) return;

    SUBSCRIBER* sub = topic->subscribers;
    while(sub)
    {
        if(send(sub->socket, msg, strlen(msg), 0) < 0) 
            perror("send to subscriber failed");
    
        printf("POSLATO");
        sub = sub->next;
    }
}

// Client thread
void *handleClient(void *arg)
{
    CLIENT *client = (CLIENT *)arg;  
    int sock = client->socket;
    client_type_t type = client->type;

    char buffer[DEFAULT_BUFLEN];
    int read_size;

    if(type == PUBLISHER_TYPE)
    {
        char topicName[DEFAULT_BUFLEN];
        while((read_size = recv(client->socket, buffer, DEFAULT_BUFLEN - 1, 0)) > 0)
        {
            // Taking topic name out of received message
            memset(&topicName, '\0', DEFAULT_BUFLEN);
            int j = 0;

            for (int i = 1; i < read_size; i++)
            {
                if(buffer[i] == ']') break;
                topicName[j++] = buffer[i]; 
            }

            topicName[j] = '\0'; 
            pthread_mutex_lock(&topicRegistry_mtx);

            TOPIC* topic = findTopic(&topicRegistry, topicName);
            if(!topic)
            {
                topic = createTopic(topicName);
                addTopic(&topicRegistry, topic);
            }

            send_to_subscribers(topic, buffer, sock);

            pthread_mutex_unlock(&topicRegistry_mtx);
        }
    }
    else
    {
        pthread_mutex_lock(&topicRegistry_mtx);
        TOPIC *t = topicRegistry.firstNode;
        char topic_list[DEFAULT_BUFLEN] = "[SERVER] Topics: ";
        while(t)
        {
            strcat(topic_list, t->name);
            strcat(topic_list, " ");
            t = t->nextTopic;
        }
        pthread_mutex_unlock(&topicRegistry_mtx);

        send(sock, topic_list, strlen(topic_list), 0);

        while((read_size = recv(sock, buffer, DEFAULT_BUFLEN - 1, 0)) > 0)
        {
            buffer[read_size] = '\0';

            // TREBA DA IDE WHILE JER MOZE VISE SUBSCRIBE I UNSUBSCRIBE
            if(strncmp(buffer, "[SUBSCRIBE] ", 12) == 0)
            {
                char* topic_name = buffer + 12;

                // Add new subscriber to the topic
                pthread_mutex_lock(&topicRegistry_mtx);
                if(findTopic(&topicRegistry, topic_name) != NULL) 
                {
                    // OVDE IDE ADD
                } 
                
                pthread_mutex_unlock(&topicRegistry_mtx);

                send(sock, "[SERVER] Subscribed.\n", 21, 0);
            }
            else if(strncmp(buffer, "[UNSUBSCRIBE] ", 14) == 0)
            {
                char* topic_name = buffer + 14;

                pthread_mutex_lock(&topicRegistry_mtx);
                pthread_mutex_lock(&topicRegistry_mtx);
                if(findTopic(&topicRegistry, topic_name) != NULL) 
                {
                    // OVDE IDE REMOVE
                } 
                pthread_mutex_unlock(&topicRegistry_mtx);

                send(sock, "[SERVER] Unsubscribed.\n", 23, 0);
            }
        }

        // Klijent se diskonektovao
        pthread_mutex_lock(&topicRegistry_mtx);
        removeSubscriberFromAllTopics(&topicRegistry, sock);
        pthread_mutex_unlock(&topicRegistry_mtx);
    }


    close(sock);
    free(client);

    return NULL;

}

int main(void)
{
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
        }
        else 
        {
            client->type = SUBSCRIBER_TYPE;
            printf("New subscriber [%s:%d]\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }

        if (pthread_create(&tid, NULL, handleClient, (void*)client) != 0) 
            {
                perror("pthread_create handleSubscriber failed");
                if(client->socket != -1) 
                    close(client->socket );
                free(client);
                return EXIT_FAILURE;
            }
       
        pthread_detach(tid);
    }

    close(server_socket);
    return 0;
}
