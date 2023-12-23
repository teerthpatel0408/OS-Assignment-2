#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>
#include<string.h>

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
    if (msgrcv(msg_queue, msg, sizeof(*msg) - sizeof(long), CLIENT_MSG_TYPE, 0) == -1) {
        perror("msgrcv");
        exit(1);
    }
}

int main() {
    // Create or get message queue
    int msg_queue = msgget(ftok(".", 'A'), IPC_CREAT | 0666);
    if (msg_queue == -1) {
        perror("msgget");
        exit(1);
    }
    
    while (1){
    	struct message msg;
    	if(msgrcv(msg_queue, &msg,  sizeof(struct message) - sizeof(long), 0, IPC_NOWAIT) == -1){
    		break;
    	}
    }

    while (1) {
        // Receive client requests from the message queue
        struct message clientRequest;
        receive_message(msg_queue, &clientRequest);

        // Process the client request
        // ...
        
        if(clientRequest.op == 1 || clientRequest.op == 2){
        	send_message(msg_queue, clientRequest.seq, clientRequest.op, clientRequest.filename, LOAD_MSG_TYPE_P);
        }
        
        else if(clientRequest.op == 3 || clientRequest.op == 4){
        	if(clientRequest.seq % 2 == 1){
        		send_message(msg_queue, clientRequest.seq, clientRequest.op, clientRequest.filename, LOAD_MSG_TYPE_S1);
        	}
        	
        	else{
        		send_message(msg_queue, clientRequest.seq, clientRequest.op, clientRequest.filename, LOAD_MSG_TYPE_S2);
        	}
        }
        // Handle cleanup request
        if (clientRequest.op == -1) {
            // Inform servers to terminate
            // ...
            send_message(msg_queue, clientRequest.seq, clientRequest.op, clientRequest.filename, LOAD_MSG_TYPE_P);
            send_message(msg_queue, clientRequest.seq, clientRequest.op, clientRequest.filename, LOAD_MSG_TYPE_S1);
            send_message(msg_queue, clientRequest.seq, clientRequest.op, clientRequest.filename, LOAD_MSG_TYPE_S2);
            // Sleep for 5 seconds as mentioned in the problem statement
            sleep(5);

            // Delete message queue
            if (msgctl(msg_queue, IPC_RMID, NULL) == -1) {
                perror("msgctl");
                exit(EXIT_FAILURE);
            }

            // Terminate load balancer
            exit(EXIT_SUCCESS);
        }
    }

    return 0;
}
