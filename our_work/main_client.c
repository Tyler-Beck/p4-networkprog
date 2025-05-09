#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>
#include <arpa/inet.h>

#define PORT_NUM 10044

typedef struct _thread_args {
    int clisockfd;
} thread_args;

void error(const char *msg){
	perror(msg);
	exit(0);
}

void* thread_recv(void* args){
    pthread_detach(pthread_self());

    int sockfd = ((thread_args*) args)->clisockfd;
    free(args);

    char buffer[513];
    int n;

    memset(buffer, 0, 513);
    n = recv(sockfd, buffer, 512, 0);
    buffer[n] = '\0';
    while(n>0) {
        printf("%s\n", buffer);
        memset(buffer, 0, 513);
        n = recv(sockfd, buffer, 512, 0);
        if(n<0) error("ERROR: reading from socket");
        buffer[n] = '\0';
    }
    pthread_exit(0);
}

void* thread_send(void* args){
    pthread_detach(pthread_self());

    int sockfd = ((thread_args*) args)->clisockfd;
    free(args);

    char buffer[256];
    int n;

    while(1) {
        memset(buffer, 0, 256);
        fgets(buffer, 255, stdin);

        // Get rid of newline charatcter
        buffer[strlen(buffer)-1] = '\0';

        n = send(sockfd, buffer, strlen(buffer), 0);
        if(n < 0) error("ERROR writing to socket");

        if(strlen(buffer)==0){
            shutdown(sockfd, SHUT_WR);
            break;
        }
    }

    pthread_exit(0);
}

int main(int argc, char *argv[]){
    char name[50];
    char room[5];
    
    if (argc < 2) error("Please specify hostname");

    // Client socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)  error("ERROR opening socket");

    // Get user's name and get rid of new line character
    memset(name, 0, 50);
    printf("Type your user name: ");
    fgets(name, 50, stdin);
    name[strlen(name)-1] = '\0';

    memset(room, 0, 5);
    if (argc >= 3) {
        memcpy(room, argv[2], strlen(argv[2]));
        room[strlen(argv[2])] = '\0';
    } else {
        // No room specified, leave empty to request room list
        room[0] = '\0';
    }

    // Set up server address structure
    struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

    // Connect to socket
    int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

    // Send name to server for mapping
    int n = send(sockfd, name, 50, 0);
    if(n < 0) error("ERROR send()ing name");

    // Send room to server
    n = send(sockfd, room, 5, 0);
    if(n < 0) error("ERROR send()ing room");

    // If room is empty server will send available rooms
    if (strlen(room) == 0) {
        char room_list[513];
        memset(room_list, 0, 513);
        
        n = recv(sockfd, room_list, 512, 0);
        if (n < 0) error("ERROR recv()ing room list");
        room_list[n] = '\0';
        
        printf("%s", room_list);
        
        // Check if no rooms available
        if (strstr(room_list, "No rooms available") != NULL) {
            strcpy(room, "new");
            
            n = send(sockfd, room, 5, 0);
            if (n < 0) error("ERROR send()ing room choice");
            
            // If room not valid, error and exit
            char valid_msg;
            n = recv(sockfd, &valid_msg, 1, 0);
            if (n < 0) error("ERROR recv()ing room validity");
            if (valid_msg == 'i') {
                close(sockfd);
                error("ERROR: room does not exist or not open");
            }

            // Receive room number
            char room_msg[65];
            memset(room_msg, 0, 65);
            n = recv(sockfd, room_msg, 64, 0);
            if (n < 0) error("ERROR recv()ing room message");
            room_msg[n] = '\0';
            printf("%s", room_msg);
        } else {
            printf("Choose the room number or type [new] to create a new room: ");
            char room_choice[5];
            memset(room_choice, 0, 5);
            fgets(room_choice, 5, stdin);
            room_choice[strlen(room_choice)-1] = '\0';
            
            // Send room to server
            n = send(sockfd, room_choice, 5, 0);
            if (n < 0) error("ERROR send()ing room choice");
            
            // If room not valid, error and exit
            char valid_msg;
            n = recv(sockfd, &valid_msg, 1, 0);
            if (n < 0) error("ERROR recv()ing room validity");
            if (valid_msg == 'i'){
                close(sockfd);
                error("ERROR: room does not exist or not open");
            }

            char room_msg[65];
            memset(room_msg, 0, 65);
            n = recv(sockfd, room_msg, 64, 0);
            if (n < 0) error("ERROR recv()ing room message");
            room_msg[n] = '\0';
            printf("%s", room_msg);
        }
    }
    else {
        // If room not valid, error and exit
        char valid_msg;
        n = recv(sockfd, &valid_msg, 1, 0);
        if (n < 0) error("ERROR recv()ing room validity");
        if (valid_msg == 'i') {
            close(sockfd);
            error("ERROR: room does not exist or not open");
        }

        // Receive room message
        char room_msg[65];
        memset(room_msg, 0, 65);
        n = recv(sockfd, room_msg, 64, 0);
        if (n < 0) error("ERROR recv()ing room message");
        room_msg[n] = '\0';
        printf("%s", room_msg);
    }

    pthread_t tid1;
	pthread_t tid2;

	thread_args* args;
	
	args = (thread_args*) malloc(sizeof(thread_args));
	args->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_send, (void*) args);

	args = (thread_args*) malloc(sizeof(thread_args));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_recv, (void*) args);

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
	close(sockfd);

    return 0;
}

