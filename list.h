#ifndef LIST_H
#define LIST_H

#include <stdio.h>

// Subscriber
typedef struct subscriber_st {
    int socket;
    struct subscriber_st *next;
} SUBSCRIBER;

typedef struct subscriberHead_st {
    SUBSCRIBER *firstNode;
} SUBSCRIBER_HEAD;

void initSubscriber(SUBSCRIBER_HEAD *head);
SUBSCRIBER* createSubsciber(int socket);
void addSubsciber(SUBSCRIBER_HEAD* head, SUBSCRIBER* newSubscriber);
void destroySubscribers(SUBSCRIBER_HEAD* head);

// Topic
typedef struct topic_st {
    char *name;
    SUBSCRIBER *subscribers;
    struct topic_st *nextTopic;
} TOPIC;

typedef struct topicHead_st {
    TOPIC *firstNode;
} TOPIC_HEAD;

void initTopic(TOPIC_HEAD *head);
TOPIC* createTopic(const char *name);
void addTopic(TOPIC_HEAD* head, TOPIC* newTopic);
void destroyTopics(TOPIC_HEAD* head);
TOPIC* findTopic(TOPIC_HEAD *head, const char *name);

int addSubscriberToTopic(TOPIC_HEAD *topics, const char *topicName, int socket);
int removeSubscriberFromTopic(TOPIC *topic, int socket);
void removeSubscriberFromAllTopics(TOPIC_HEAD *head, int socket);

void printTopicsAndSubscribers(TOPIC_HEAD *head);
void printTopics(TOPIC_HEAD *head);

#endif // LIST_H
