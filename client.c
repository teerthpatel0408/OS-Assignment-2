#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <string.h>

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
    if (msgrcv(msg_queue, msg, sizeof(*msg) - sizeof(long), SERVER_MSG_TYPE, 0) == -1) {
        perror("msgrcv");
        exit(1);
    }
}

int main() {
    // Create or get message queue and shared memory
    int msg_queue = msgget(ftok(".", 'A'), 0666);
    if (msg_queue == -1) {
        perror("msgget");
        exit(1);
    }
    
    int shmId = shmget(ftok(".", 'S'), sizeof(struct SharedMemory), IPC_CREAT | 0666);
    if (shmId == -1) {
        perror("shmget");
        exit(1);
    }
    
    // Attach to the shared memory
    struct SharedMemory *shmPtr = (struct SharedMemory *)shmat(shmId, NULL, 0);
    if(shmPtr == (void*)-1){
    	perror("shmat");
        exit(1);
    }
    
    while (1) {
        printf("\nMenu:\n");
        printf("1. Add a new graph to the database\n");
        printf("2. Modify an existing graph of the database\n");
        printf("3. Perform DFS on an existing graph of the database\n");
        printf("4. Perform BFS on an existing graph of the database\n\n");

    	// Get input from the user or use your own logic to create a client request
    	int seq, op;
    	char filename[256];

    	printf("Enter Sequence Number: ");
    	scanf("%d", &seq);

    	printf("Enter Operation Number (1-4): ");
    	scanf("%d", &op);

    	printf("Enter Graph File Name: ");
    	scanf("%s", filename);

    	// For write operations
    	if (op == 1 || op == 2) {
        	printf("Enter number of nodes of the graph: ");
        	scanf("%d", &shmPtr->numNodes);

        	printf("Enter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters:\n");
        	for (int i = 0; i < shmPtr->numNodes; ++i) {
            		for (int j = 0; j < shmPtr->numNodes; ++j) {
                		scanf("%d", &shmPtr->adjacencyMatrix[i][j]);
            		}
        	}
    	}
    	else if(op == 3 || op == 4){
    		printf("Enter the starting vertex: ");
        	scanf("%d", &shmPtr->startVertex);
    	}
    	else{
    		printf("Invalid option!");	
    	}
    	send_message(msg_queue, seq, op, filename, CLIENT_MSG_TYPE);

    	// Receive the response from the server
    	struct message response;
    	receive_message(msg_queue, &response);

    	// Process the server response
    	printf("Server Response: %s\n", response.filename);
    }

    // Detach from the shared memory
    shmdt(shmPtr);

    // Remove the message queue and shared memory if needed
    // ...

    return 0;
}
