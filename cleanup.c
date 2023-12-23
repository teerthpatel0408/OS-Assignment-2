#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>


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

int main() {
    // Create or get message queue
    int msg_queue = msgget(ftok(".", 'A'), 0666);
    if (msg_queue == -1) {
        perror("msgget");
        exit(1);
    }

    // Add any necessary initialization for cleanup handling
    // ...

    while (1) {
        // Display cleanup menu
        printf("Want to terminate the application? Press Y (Yes) or N (No)\n");

        // Get user input
        char choice;
        scanf(" %c", &choice);

        // Check if the user wants to terminate
        if (choice == 'Y' || choice == 'y') {
            // Cleanup code if needed
            // ...
            struct message terminate_msg;
            terminate_msg.mtype = CLIENT_MSG_TYPE;
            terminate_msg.op = -1;
            if (msgsnd(msg_queue, &terminate_msg, sizeof(terminate_msg) - sizeof(long), 0) == -1) {
                perror("msgsnd");
                exit(1);
            }
            exit(0);
        }
        
        else if (choice == 'N' || choice == 'n') {
        }
        
        else {
            printf("Invalid choice. Please enter Y or N.\n");
        }
    }

    return 0;
}
