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

    char buffer[512];
    int n;

    n = recv(sockfd, buffer, 512, 0);
    buffer[n] = '\0';
    while(n>0) {
        printf("%s\n", buffer);
        memset(buffer, 0, 512);
        n = recv(sockfd, buffer, 512, 0);
        if(n<0) error("ERROR: reading from socket");
        buffer[n] = '\0';
    }
    return NULL;
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

        if(strlen(buffer)==0) break;
    }
    return NULL;
}

int main(int argc, char *argv[]){
    char name[50];
    char room[5];
    
    if (argc < 3) error("Please specify hostname and room");

    // Client socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)  error("ERROR opening socket");
    
    // Get user's name and get rid of new line character
    memset(name, 0, 50);
    printf("Type your user name: ");
    fgets(name, 50, stdin);
    name[strlen(name)-1] = '\0';

    // Set room
    memset(room, 0, 5);
    memcpy(room, argv[2], strlen(argv[2]));
    room[strlen(argv[2])] = '\0';

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

    // If room not valid, error and exit
    char valid_msg;
    n = recv(sockfd, &valid_msg, 1, 0);
    if(n < 0) error("ERROR recv()ing room validity");
    if(valid_msg == 'i') error("ERROR: room does not exist or not open");

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

	close(sockfd);


    return 0;
}

