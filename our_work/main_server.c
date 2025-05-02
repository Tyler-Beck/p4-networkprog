#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 1004

void add_client();
void remove_client();
void display_clients();
void broadcast_msg();
void notification();
void assign_color();

/* 
	Everything so far is basically from chat_server_full in /ex folder.
	I think the functions above are all we need for checkpoint 1.
*/

typedef struct client_info{
	int sockfd;
	char username[20];
	char ip_addr[20];
	struct client_info* next; // pointer to the next client
} Client;

Client* head = NULL; // head of the linked list
Client* tail = NULL; // tail of the linked list
int num_clients = 0; // connected clients


void add_client(int newclisockfd, char* username, char* ip_addr){
	// Add a new client
	if(head == NULL){
		head = (Client*) malloc(sizeof(Client));
		head->sockfd = newclisockfd;
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (Client*) malloc(sizeof(Client));
		tail->next->sockfd = newclisockfd;
		tail->next->next = NULL;
		tail = tail->next;
	}
}

void error(const char *msg){
	perror(msg);
	exit(1);
}

int main(int argc, char* argv[]){

	if(argc < 2) error("Specify port number");


    return 0;
}