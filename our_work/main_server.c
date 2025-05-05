#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 10044
/*
void add_client();
void remove_client();
void display_clients();
void broadcast_msg();
void notification();
void assign_color();

typedef struct client_info{
	int sockfd;
	char username[20];
	char ip_addr[20];
	struct client_info* next; // pointer to the next client
} Client;

Client* head = NULL; // head of the linked list
Client* tail = NULL; // tail of the linked list
int num_clients = 0; // connected clients


void add_client(int newclisockfd, struct sockaddr_in* cli_addr, char* username){
	// Add a new client
	Client* new_client = (Client*)malloc(sizeof(Client));
	new_client->sockfd = newclisockfd;
	strncpy(new_client->username, username, sizeof(new_client->username));
	strncpy(new_client->ip_addr, ip_addr, sizeof(new_client->ip_addr));
	new_client->next = NULL;

	if(head == NULL){
		head = new_client;
		tail = head;
	} else {
		tail->next = new_client;
		tail = new_client;
	}

	num_clients++;
}

void remove_client(int sockfd){
	Client* temp = head;
	Client* prev = NULL;

	while(temp != NULL){
		if(temp->sockfd == sockfd){
			if(prev == NULL){
				head = temp->next;
			} else {
				prev->next = temp->next;
			}
		}
	}

	if(temp == NULL){
		printf("Client not found\n");
	} else if(temp == tail){
		tail = prev;
	} else {
		prev->next = temp->next;
	}

	// notify clients that a client has left
	char msg[50];
	sprintf(msg, "%s has left the chat\n", temp->username);
	broadcast_msg(msg, sockfd);

	free(temp);
	num_clients--;
}

void display_clients(){
	Client* temp = head;
	while(temp != NULL){
		printf("Client: %s, IP: %s\n", temp->username, temp->ip_addr);
		temp = temp->next;
	}
}

void broadcast_msg(char* msg, int sockfd){
	Client* temp = head;
	while(temp != NULL){
		if(temp->sockfd != sockfd){
			send(temp->sockfd, msg, strlen(msg), 0);
		}
		temp = temp->next;
	}
}

void notification(char* msg){
	Client* temp = head;
	while(temp != NULL){
		send(temp->sockfd, msg, strlen(msg), 0);
		temp = temp->next;
	}
}
*/


void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	char* name;			// name of client
	struct _USR* next;	// for linked list queue
} USR;

USR *head = NULL;
USR *tail = NULL;

void add_tail(int newclisockfd, char* name)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		memcpy(head->name, name, strlen(name));
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		memcpy(tail->next->name, name, strlen(name));
		tail->next->next = NULL;
		tail = tail->next;
	}
}

void broadcast(int fromfd, char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	// Get name of sender
	char name[50];
	memset(name, 0, 50);

	USR* cur = head;
	while(cur != NULL) {
		if(cur->clisockfd == fromfd) {
			memcpy(name, cur->name, strlen(cur->name));
			break;
		}
	}

	// traverse through all connected clients
	cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd) {
			char buffer[512];

			// prepare message
			memset(buffer, 0, 512);
			sprintf(buffer, "[%s (%s)]:%s", name, inet_ntoa(cliaddr.sin_addr), message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;

	memset(buffer, 0, 256);
	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(clisockfd, buffer);

		memset(buffer, 0, 256);
		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}

	close(clisockfd);
	//-------------------------------

	return NULL;
}

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 5); // maximum number of connections = 5

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		// Get name and map in node
		char name[50];
		memset(name, 0, 50);
		int n = recv(newsockfd, name, 50, 0);
		if(n < 0) error("ERROR recv()ing name");

		printf("Connected: %s (%s)\n", name, inet_ntoa(cli_addr.sin_addr));

		add_tail(newsockfd, name); // add this new client to the client list

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}