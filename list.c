#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

void initSubscriber(SUBSCRIBER_HEAD *head)
{
    head->firstNode = NULL;
} 

// Create new subscriber
SUBSCRIBER* createSubscriber(int socket) 
{
    SUBSCRIBER* newSubscriber = malloc(sizeof(SUBSCRIBER));
    if(newSubscriber == NULL)
    {
        perror("malloc SUBSCRIBER");
        return NULL; 
    }

    newSubscriber->socket = socket;
    newSubscriber->next = NULL;

    return newSubscriber;
}

// Add new subscriber to the list of subscribers
void addSubscriber(SUBSCRIBER_HEAD* head, SUBSCRIBER* newSubscriber)
{
    if(head->firstNode == NULL) 
        head->firstNode = newSubscriber;
    else 
    {
        SUBSCRIBER* current = head->firstNode;

        while (current->next != NULL)
            current = current->next;

        current->next = newSubscriber;
    }
}

// Destroy all subscribers and free memory
void destroySubscribers(SUBSCRIBER_HEAD* head)
{
    SUBSCRIBER* current;
    while (head->firstNode != NULL)
    {
        current = head->firstNode;
        head->firstNode = current->next;
        current->next = NULL;
        free(current);
    }
}

void initTopic(TOPIC_HEAD *head)
{
    head->firstNode = NULL;
} 

// Create new topic
TOPIC* createTopic(const char *name) 
{
    TOPIC* newTopic = malloc(sizeof(TOPIC));
    if (newTopic == NULL)
    {
        perror("malloc TOPIC");
        return NULL;
    }

    newTopic->name = strdup(name);
    if (!newTopic->name)
    {
        perror("strdup topic name");
        free(newTopic);
        return NULL;
    }

    newTopic->subscribers = NULL;
    newTopic->nextTopic = NULL;

    return newTopic;
}

// Add new topic
void addTopic(TOPIC_HEAD* head, TOPIC* newTopic)
{
    if(head->firstNode == NULL) 
        head->firstNode = newTopic;
    else 
    {
        TOPIC* current = head->firstNode;

        while (current->nextTopic != NULL)
            current = current->nextTopic;

        current->nextTopic = newTopic;
    }
}

// Destroy all topics
void destroyTopics(TOPIC_HEAD* head)
{
    TOPIC* current;
    while (head->firstNode != NULL)
    {
        current = head->firstNode;
        head->firstNode = current->nextTopic;

        // Free all subscribers of the topic
        SUBSCRIBER_HEAD tempHead;
        tempHead.firstNode = current->subscribers;
        destroySubscribers(&tempHead);

        // Free topic name
        free(current->name);

        // Free topic
        free(current);
    }
}

// Find topic by name
TOPIC* findTopic(TOPIC_HEAD *head, const char *name)
{
    TOPIC *current = head->firstNode;

    while (current != NULL)
    {
        if (strcmp(current->name, name) == 0)
            return current;

        current = current->nextTopic;
    }

    // topic not found
    return NULL; 
}

// Add subscriber to topic he wants to subscribe to  
int addSubscriberToTopic(TOPIC_HEAD *topics, const char *topicName, int socket)
{
    TOPIC *topic = findTopic(topics, topicName);
    if (topic == NULL)
    {
        printf("Client %d wanted to connect to '%s', which is not in the registry\n", socket, topicName);        return -1;
    }

    // Check if the subscriber is already in topic
    SUBSCRIBER *current = topic->subscribers;
    while (current)
    {
        if (current->socket == socket)
        {
            printf("Subscriber %d already subscribed to '%s'\n", socket, topicName);
            return 1;  
        }
        current = current->next;
    }

    // If not
    SUBSCRIBER *sub = createSubscriber(socket);
    if (sub == NULL)
        return -1;

    sub->next = topic->subscribers;
    topic->subscribers = sub;

    return 0;  
}


// Remove subscriber for specific topic
int removeSubscriberFromTopic(TOPIC *topic, int socket)
{
    if (!topic || !topic->subscribers)
        return -1; // nothing to remove

    SUBSCRIBER *current = topic->subscribers;
    SUBSCRIBER *prev = NULL;

    while (current)
    {
        if (current->socket == socket)
        {
            
            if (prev == NULL)
                topic->subscribers = current->next; // it was the first element
            else
                prev->next = current->next;

            free(current);
            return 0; 
        }

        prev = current;
        current = current->next;
    }

    // subscriber not found
    return -1; 
}

// Remove subscriber from all of the topics he's in
void removeSubscriberFromAllTopics(TOPIC_HEAD *head, int socket)
{
    TOPIC *currentTopic = head->firstNode;
    while (currentTopic)
    {
        removeSubscriberFromTopic(currentTopic, socket);
        currentTopic = currentTopic->nextTopic;
    }
}

// Print topics and subscribers in a clean format
void printTopicsAndSubscribers(TOPIC_HEAD *head)
{
    printf("[TOPICS] Current topics and subscribers:\n");

    TOPIC *t = head->firstNode;
    if (!t)
    {
        printf("  No topics available.\n\n");
        return;
    }

    while (t)
    {
        printf("  - %s\n", t->name);  // Topic name

        SUBSCRIBER *s = t->subscribers;
        if (!s)
        {
            printf("      Subscribers: none\n");
        }
        else
        {
            printf("      Subscribers: ");
            while (s)
            {
                printf("%d", s->socket);
                if (s->next) 
                    printf(", "); // separate multiple subscribers
                s = s->next;
            }
            printf("\n");
        }

        t = t->nextTopic;
    }

    printf("\n"); // extra line for readability
}

// Print topics
void printTopics(TOPIC_HEAD *head)
{
    TOPIC *t = head->firstNode;
    while (t)
    {
        printf("Topic: %s\n", t->name);
        t = t->nextTopic;
    }
}

