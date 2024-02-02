#include<stdbool.h>
#include <pthread.h>
#include "interface/log.h"
#include "../ds/interface/hashmap.h"
#include "../time/interface/timestamp.h"

static FILE* logfile;
static int serverLogLevel;
static LogMessageQueue messageQueue;
static HashMap threadRegister;
static bool terminateThreads = false;

typedef struct LogInfoParams {
    pthread_t parentId;
    int log_level;
    char* message;
} LogInfoParams;

char* getLogLevelString(int code) {
    switch (code) {
        case 0: return "DEBUG";
                break;
        case 1: return "INFO";
                break;
        case 2: return "WARN";
                break;
        case 3: return "ERROR";
                break;
        case 4: return "CRITICAL";
                break;   
        default: return NULL;                                                             
    }
}

int getLogLevelCode(char* loglevelStr) {
    if(strcmp(loglevelStr, "DEBUG") == 0) {
        return LOG_DEBUG;
    } else if(strcmp(loglevelStr, "INFO") == 0) {
        return LOG_INFO;
    } else if(strcmp(loglevelStr, "WARN") == 0) {
        return LOG_WARN;
    } else if(strcmp(loglevelStr, "ERROR") == 0) {
        return LOG_ERROR;
    } else if(strcmp(loglevelStr, "CRTITICAL") == 0) {
        return LOG_CRITICAL;
    } else {
        return -1;
    }
}

void freeLogMessageNode(LogMessageNode* node) {
    node->message = NULL;
    free(node->threadUUID);
    free(node->message);
    free(node->timeStamp);
    free(node);
}

void freeLogThreadRegisterNode(void* value) {
    free(value);
}

void enqueueLogMessage(const int log_level, char* threadUUID, char* message, char* timeStamp) {
    LogMessageNode* newNode = (LogMessageNode*)malloc(sizeof(LogMessageNode));
    if (newNode == NULL) {
        printf("XX Failed to allocate memory for a new message node XX \n");
        return;
    }

    newNode->log_level = log_level;
    newNode->threadUUID = strdup(threadUUID);
    newNode->message = strdup(message);
    newNode->timeStamp = strdup(timeStamp);
    newNode->next = NULL;

    pthread_mutex_lock(&messageQueue.mutex);

    if (messageQueue.head == NULL) {
        messageQueue.head = newNode;
    } else {
        LogMessageNode* current = messageQueue.head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }

    pthread_cond_signal(&messageQueue.cond);
    pthread_mutex_unlock(&messageQueue.mutex);
}

LogMessageNode dequeueLogMessage() {
    LogMessageNode messageNode;
    pthread_mutex_lock(&messageQueue.mutex);

    while (messageQueue.head == NULL) {
        pthread_cond_wait(&messageQueue.cond, &messageQueue.mutex);
    }

    LogMessageNode* head = messageQueue.head;
    messageNode.log_level = head->log_level;
    messageNode.message = strdup(head->message);
    messageNode.threadUUID = strdup(head->threadUUID);
    messageNode.timeStamp = strdup(head->timeStamp);

    messageQueue.head = head->next;
    freeLogMessageNode(head);

    pthread_mutex_unlock(&messageQueue.mutex);

    return messageNode;
}

void* logReaderThreadFunction(void* arg) {
    while (1) {
        LogMessageNode messageNode = dequeueLogMessage();
    
        char* log_level_str = getLogLevelString(messageNode.log_level);
        if(log_level_str && messageNode.log_level >= serverLogLevel) {
            
            if(logfile != NULL && fprintf(logfile, "%s %s %s %s\n", messageNode.timeStamp, log_level_str, messageNode.threadUUID, messageNode.message) >= 0) {
                fflush(logfile);
            } else {
                printf("%s %s %s %s\n", messageNode.timeStamp, log_level_str, messageNode.threadUUID, messageNode.message);
            }

        }

        if (terminateThreads && messageQueue.head == NULL) {
            pthread_exit(NULL);
        }
    }

    return NULL;
}

void initLogUtil(char* log_level, const char* path) {
    char logFilePath[strlen(path) + strlen("/log.txt") + 1];
    strcpy(logFilePath, path);
    strcat(logFilePath, "/log.txt");
    
    messageQueue.head = NULL;
    pthread_mutex_init(&messageQueue.mutex, NULL);
    pthread_cond_init(&messageQueue.cond, NULL);

    initializeHashMap(&threadRegister);

    logfile = fopen(logFilePath, "a");
    if (logfile == NULL) {
        printf("XX Error opening log file for writing. XX \n");
    }

    serverLogLevel = getLogLevelCode(log_level);

    pthread_t reader;
    if (pthread_create(&reader, NULL, logReaderThreadFunction, NULL) != 0) {
        pthread_join(reader, NULL);
        logWriter(LOG_INFO, "Error creating thread for async log.\n");
    } else {
        logWriter(LOG_INFO, "Log message reader Thread Started.\n");
    }

    logWriter(LOG_INFO, "log initLogUtil completed.\n");
}

void* logWriterThreadFunction(void* arg) {
    LogInfoParams* params = (LogInfoParams*) arg;

    char buffer[25]; 
    snprintf(buffer, sizeof(buffer), "%p", (void *) params->parentId);

    char* threadUUID = (char*) getHashMap(&threadRegister, buffer);
    if(threadUUID == NULL) {
        threadUUID = buffer;
    }

    char timestampString[30];  
    timestampToString(timestampString, sizeof(timestampString));

    enqueueLogMessage(params->log_level, threadUUID, params->message, timestampString);

    free(params->message);
    free(params);

    return NULL;
}

void logWriter(const int log_level, const char* message) {
    if(log_level < serverLogLevel) {
        return;
    }

    pthread_t parentId, writer;
    parentId = pthread_self();

    LogInfoParams* params = malloc(sizeof(LogInfoParams));
    params->parentId = parentId;
    params->log_level = log_level; 
    params->message = strdup(message);

    if (pthread_create(&writer, NULL, logWriterThreadFunction, (void*) params) != 0) {
        printf("Error creating child thread during log writer\n");
    }
}


void freeLogUtil() {
    terminateThreads = true;

    cleanupValueFunc cleanupValueFuncPtr = (cleanupValueFunc) freeLogThreadRegisterNode;
    cleanupHashMap(&threadRegister, cleanupValueFuncPtr);

    LogMessageNode* current = messageQueue.head;
    while (current != NULL) {
        LogMessageNode* nextNode = current->next;
        free(current->threadUUID);
        free(current->message);
        free(current);
        current = nextNode;
    }
    messageQueue.head = NULL; 

    pthread_mutex_destroy(&messageQueue.mutex);
    pthread_cond_destroy(&messageQueue.cond);

    printf("** Log util resources cleanup successfull. **\n");

    if(fclose(logfile) != 0) {
        printf("XX Error closing Log File. XX\n");
    }
}

void setLogThreadRegisterUUID(char* threadID, char* UUID) {
    char* uuidCopy = strdup(UUID);
    insertHashMap(&threadRegister, threadID, uuidCopy, strlen(uuidCopy) + 1);
}

void removeLogThreadRegisterUUID(char* threadID) {
    cleanupValueFunc cleanupValueFuncPtr = (cleanupValueFunc) freeLogThreadRegisterNode;
    deleteHashMap(&threadRegister, threadID, cleanupValueFuncPtr);
}
