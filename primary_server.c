#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>


#define MAX_NODES 30
#define CLIENT_MSG_TYPE 1
#define SERVER_MSG_TYPE 2
#define LOAD_MSG_TYPE_P 3
#define LOAD_MSG_TYPE_S1 41
#define LOAD_MSG_TYPE_S2 42

// Define your message structure
struct message {
    long mtype;  
    int seq;
    int op;      
    char filename[256];
};

// Define your shared memory structure
struct SharedMemory {
    int numNodes;
    int startVertex;
    int adjacencyMatrix[MAX_NODES][MAX_NODES];
};

// Define semaphores
sem_t *rw_mutex;
sem_t *read_count;
sem_t *mutex;

void send_message(int msg_queue, int sequence_number, int operation_number, const char* filename, int mtype) {
    struct message msg;
    msg.mtype = mtype;
    msg.seq = sequence_number;
    msg.op = operation_number;
    if (filename != NULL) {
        strncpy(msg.filename, filename, sizeof(msg.filename));
    }

    if (msgsnd(msg_queue, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        exit(1);
    }
}


void receive_message(int msg_queue, struct message* msg) {
    if (msgrcv(msg_queue, msg, sizeof(*msg) - sizeof(long), LOAD_MSG_TYPE_P, 0) == -1) {
        perror("msgrcv");
        exit(1);
    }
}

// Define your thread function
void *handleClientRequest(void *arg) {
    int msg_queue = msgget(ftok(".", 'A'), 0666);
    if (msg_queue == -1) {
        perror("msgget");
        exit(1);
    }
    struct message *clientRequest = (struct message *)arg;

    // Extract information from the client request
    int seq = clientRequest->seq;
    int op = clientRequest->op;
    char filename[256];
    strcpy(filename, clientRequest->filename);

    // Get information from shared memory
    int shmId = shmget(ftok(".", 'S'), sizeof(struct SharedMemory), 0666);
    if (shmId == -1) {
        perror("shmget");
        exit(1);
    }
    
    struct SharedMemory *shmPtr = (struct SharedMemory *)shmat(shmId, NULL, 0);
    if(shmPtr == (void*)-1){
    	perror("shmat");
        exit(1);
    }
    
    // Handle write operation
    // ...
    FILE *file = fopen(filename, "w");
    sem_wait(rw_mutex);
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    // Write the number of nodes to the file
    fprintf(file, "%d\n", shmPtr->numNodes);

    // Write the adjacency matrix to the file
    for (int i = 0; i < shmPtr->numNodes; i++) {
        for (int j = 0; j < shmPtr->numNodes; j++) {
            fprintf(file, "%d ", shmPtr->adjacencyMatrix[i][j]);
        }
        fprintf(file, "\n");
    }
    sem_post(rw_mutex);
    fclose(file);

    // Detach from shared memory
    shmdt(shmPtr);

    // Send success message back to the client
    if(op == 1){
    	strcpy(filename, "File successfully added.");
    }
    else{
    	strcpy(filename, "File successfully modified.");
    }
    send_message(msg_queue, seq, op, filename, SERVER_MSG_TYPE);

    // Exit the thread
    pthread_exit(NULL);
}

int main() {
    // Create or get message queue and shared memory
    // ...
    int msg_queue = msgget(ftok(".", 'A'), 0666);
    if (msg_queue == -1) {
        perror("msgget");
        exit(1);
    }
    rw_mutex = sem_open("/rw_mutex", O_CREAT ,0644, 1);
    mutex = sem_open("/mutex",O_CREAT ,0644, 1);
    
    while (1) {
        // Receive client requests from the message queue
        struct message clientRequest;
        receive_message(msg_queue, &clientRequest);
        
        if (clientRequest.op == -1) {
            exit(1);
        }
        // Create a new thread to handle the client request
        pthread_t thread;
        if (pthread_create(&thread, NULL, handleClientRequest, (void *)&clientRequest) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        // Allow the main thread to continue receiving other requests
        pthread_detach(thread);
    }
    return 0;
}
