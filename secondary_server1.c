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
sem_t *mutex;

int readerCount  = 0;

// Define a mutex to protect shared resources
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct Queue{
    int front, rear, size;
    unsigned capacity;
    int* array;
};

// Queue functions
struct Queue* createQueue(unsigned capacity) {
    struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;
    queue->array = (int*)malloc(capacity * sizeof(int));
    return queue;
}

int isQueueEmpty(struct Queue* queue) {
    return (queue->size == 0);
}

void enqueue(struct Queue* queue, int item) {
    if (queue->size == queue->capacity)
        return;

    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size++;
}

int dequeue(struct Queue* queue) {
    if (isQueueEmpty(queue))
        return -1;

    int item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return item;
}

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
    if (msgrcv(msg_queue, msg, sizeof(*msg) - sizeof(long), LOAD_MSG_TYPE_S1, 0) == -1) {
        perror("msgrcv");
        exit(1);
    }
}


int visited[MAX_NODES] = {0};
int bsfvisited[MAX_NODES] = {0};
char ch[2*MAX_NODES+1]="";
int cnt = 0;

// Define your thread function for DFS
void *performDFS(void *arg) {
    struct SharedMemory *shm = (struct SharedMemory *)arg;
    
    int currentVertex = shm->startVertex;
    
    // Lock the mutex before updating the visited array
    pthread_mutex_lock(&lock);
    visited[currentVertex] = 1;
    pthread_mutex_unlock(&lock);
    
    int isleaf=1;

    int cntdfs = 0;
    pthread_t thread[shm->numNodes];
  
    // Traverse adjacent nodes
    for (int i = 0; i < shm->numNodes; ++i) {
        if (shm->adjacencyMatrix[currentVertex][i] == 1 && !visited[i]) {
            // Create a new thread for unvisited adjacent nodes
            pthread_t childThread;
            isleaf=0;
            
            struct SharedMemory* newData = malloc(sizeof(struct SharedMemory));
            // Lock the mutex before copying the adjacency matrix
            pthread_mutex_lock(&lock);
            newData->startVertex = i;
            newData->numNodes = shm->numNodes;
            for (int k = 0; k < shm->numNodes; k++) {
        	for (int j = 0; j < shm->numNodes; j++) {
            		newData->adjacencyMatrix[k][j] = shm->adjacencyMatrix[k][j];
        	}
    	    }
    	    pthread_mutex_unlock(&lock);

            pthread_create(&childThread, NULL, performDFS, newData);

            // Wait for the thread to finish
            thread[cntdfs++] = childThread;
        }
    }
    
    for(int i = 0; i < cntdfs; i++){
       pthread_join(thread[i], NULL);
    }
    // Exit the thread
    
    if(isleaf){
    	// Lock the mutex before updating the shared resource
        pthread_mutex_lock(&lock);
    	ch[cnt++] = (currentVertex + '0');
    	ch[cnt++]=' ';
    	pthread_mutex_unlock(&lock);
    }
    pthread_exit(NULL);
}

// Define your thread function for BFS
void* performBFS(void* arg) {
    struct SharedMemory *shm = (struct SharedMemory *)arg;
    int currentVertex = shm->startVertex;

    // Initialize the queue
    struct Queue* queue = createQueue(MAX_NODES);
    
    // Lock the mutex before updating shared resources
    pthread_mutex_lock(&lock);
    // Mark the start node as visited and enqueue it
    ch[cnt++] = (currentVertex + '0');
    ch[cnt++]=' ';
    bsfvisited[currentVertex] = 1;
    enqueue(queue, currentVertex);
    pthread_mutex_unlock(&lock);

    // Process nodes at each level concurrently
    while (!isQueueEmpty(queue)) {
        int levelSize = queue->size;
        for (int i = 0; i < levelSize; ++i) {
            // Lock the mutex before dequeuing
            pthread_mutex_lock(&lock);
            int currentNode = dequeue(queue);
            pthread_mutex_unlock(&lock);
            
            int cntbfs = 0;
            pthread_t thread[shm->numNodes];
            // Process neighbors of the current node
            for (int i = 0; i < shm->numNodes; ++i) {
            	// Lock the mutex before checking and updating shared resources
            	pthread_mutex_lock(&lock);
                if (shm->adjacencyMatrix[currentNode][i] && !bsfvisited[i]) {
                    bsfvisited[i] = 1;
                    cntbfs++;
                    enqueue(queue, i);
                    
                    struct SharedMemory* newData = malloc(sizeof(struct SharedMemory));
            	    newData->startVertex = i;
            	    newData->numNodes = shm->numNodes;
            	    for (int k = 0; k < shm->numNodes; k++) {
        		for (int j = 0; j < shm->numNodes; j++) {
            			newData->adjacencyMatrix[k][j] = shm->adjacencyMatrix[k][j];
        		}
    	    	    }
            	    pthread_create(&thread[cntbfs-1], NULL, performBFS, newData);
                }
                pthread_mutex_unlock(&lock);
            }
            
            for(int i = 0; i < cntbfs; i++){
            	pthread_join(thread[i], NULL);
            }
        }
    }

    // Cleanup
    free(queue->array);
    free(queue);

    pthread_exit(NULL);
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
    
    // Handle read operation
    // ...
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }
    
    sem_wait(mutex);
    readerCount++;
    if(readerCount == 1) {
    	sem_wait(rw_mutex);
    }
    sem_post(mutex);

    fscanf(file, "%d", &shmPtr->numNodes); // Read the size of the matrix
    // Write the adjacency matrix to the file
    for (int i = 0; i < shmPtr->numNodes; i++) {
       for (int j = 0; j < shmPtr->numNodes; j++) {
            fscanf(file, "%d", &shmPtr->adjacencyMatrix[i][j]);
        }
    }
    
    sem_wait(mutex);
    readerCount--;
    if(readerCount == 0) {
    	sem_post(rw_mutex);
    }
    sem_post(mutex);
    
    fclose(file);
    	pthread_t thread;
    	if (op == 3) {
            if (pthread_create(&thread, NULL, performDFS, (void *)shmPtr) != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
        } 
       	else if (op == 4) {
            if (pthread_create(&thread, NULL, performBFS, (void *)shmPtr) != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
        } 
        else {
            fprintf(stderr, "Invalid operation number\n");
            // Handle error
        }
        pthread_join(thread,NULL);
        
    strcpy(filename, ch);
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
        
	memset(visited,0,sizeof(visited));
	memset(bsfvisited,0,sizeof(bsfvisited));
	memset(ch, '\0', sizeof(ch));
	cnt = 0;
	
	// Initialize the mutex
    	if (pthread_mutex_init(&lock, NULL) != 0) {
        	fprintf(stderr, "Mutex initialization failed\n");
        	return 1;
    	}
    	
        // Create a new thread to handle the client request
        pthread_t thread;
        if (pthread_create(&thread, NULL, handleClientRequest, (void *)&clientRequest) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        pthread_join(thread,NULL);
        printf("\n");
        
        // Allow the main thread to continue receiving other requests
        pthread_detach(thread);
    }
    return 0;
}
