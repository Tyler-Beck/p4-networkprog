#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT_NUM 10044

#define MAX_COLORS 8
const char* colors[MAX_COLORS] = {
	"\033[31m", // Red
	"\033[32m", // Green
	"\033[33m", // Yellow
	"\033[34m", // Blue
	"\033[35m", // Magenta
	"\033[36m", // Cyan
	"\033[1;31m", // Bright red
	"\033[1;32m", // bright green
};

const char* reset_color = "\033[0m";

int rooms[10]; // Tracks which room is currently the last used

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	char name[50];		// name of client
	int color_index;	// color index of client
	struct _USR* next;	// for linked list queue
	int room;			// room client is in
} USR;

USR *head = NULL;
USR *tail = NULL;

pthread_mutex_t mutex;

void print_list() {
	printf("\n-----Connected clients-----\n");
	USR* cur = head;
	while(cur != NULL) {
		char* name = cur->name;
		struct sockaddr_in addr;
		socklen_t clen = sizeof(addr);
		getpeername(cur->clisockfd, (struct sockaddr*)&addr, &clen);
		printf("%s (%s)\n", name, inet_ntoa(addr.sin_addr));

		cur = cur->next;
	}
}

// Function to list available rooms and their occupants
void list_available_rooms(int sockfd) {
    char room_list[512] = "Server says following options are available:\n";
    char room_info[64];
    int roomCount = 0;
    
    for (int i = 0; i < 10; i++) {
        if (rooms[i] > 0) {
            sprintf(room_info, "Room %d: %d %s\n", i+1, rooms[i], 
                    rooms[i] == 1 ? "person" : "people");
            strcat(room_list, room_info);
            roomCount++;
        }
    }
    
    if (roomCount == 0) {
        strcat(room_list, "No rooms available. Creating new room.\n");
    }

    int n = send(sockfd, room_list, 512, 0);
	if(n < 0) error("ERROR send()ing room list");
}

void add_tail(int newclisockfd, char* name, int room)
{

	// static array to track used colors
	static int used_colors[MAX_COLORS] = {0};
	int colors_avail[MAX_COLORS];
	int num_avail = 0;
	int color_index;

	// Figure out what colors are available
	for(int i = 0; i < MAX_COLORS; i++){
		if(used_colors[i] == 0){
			colors_avail[num_avail++] = i;
		}
	}

	// if all the colors get taken, reset and start again
	if(num_avail == 0){
		for(int i = 0; i < MAX_COLORS; i++){
			used_colors[i] = 0;
			colors_avail[num_avail++] = i;
		}
	}

	// pick a random color and tag it as used
	color_index = colors_avail[rand() % num_avail];
	used_colors[color_index] = 1;

	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		memcpy(head->name, name, strlen(name));
		head->name[strlen(name)] = '\0';
		head->color_index = color_index; // assiging color here
		head->room = room;
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		memcpy(tail->next->name, name, strlen(name));
		tail->next->name[strlen(name)] = '\0';
		tail->next->color_index = color_index; // assigning color here
		tail->next->room = room;
		tail->next->next = NULL;
		tail = tail->next;
	}
}

void broadcast(int fromfd, char* message, int option, int room)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	// Get name of sender (and color)
	char name[50];
	memset(name, 0, 50);
	int sender_color_index = 0;

	pthread_mutex_lock(&mutex);

	USR* cur = head;
	while(cur != NULL) {
		if(cur->clisockfd == fromfd) {
			memcpy(name, cur->name, strlen(cur->name));
			sender_color_index = cur->color_index; // sender color
			break;
		}
		cur = cur->next;
	}

	// traverse through all connected clients
	cur = head;
	while (cur != NULL) {
		// Connection, send to everyone in the same room
		if(option==2 && cur->room == room) {
			char buffer[256];
			memset(buffer, 0, 256);
			sprintf(buffer, "%s (%s) joined room %d!", name, inet_ntoa(cliaddr.sin_addr), room);

			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if(nsen != nmsg) error("ERROR send() failed");
		}
		// check if cur is not the one who sent the message and that current is in the same room
		else if (cur->clisockfd != fromfd && cur->room == room) {
			char buffer[512];

			// prepare message
			memset(buffer, 0, 512);

			if(option==1) { /* Disconnection */ 
				sprintf(buffer, "%s (%s) left room %d!", name, inet_ntoa(cliaddr.sin_addr), room);
			} else { /* Normal message */ 
				char colored_prefix[256];
				sprintf(colored_prefix, "%s[%s (%s)] %s%s", colors[sender_color_index], name, inet_ntoa(cliaddr.sin_addr), message, reset_color);
				strcat(buffer, colored_prefix);
			}

			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
	pthread_mutex_unlock(&mutex);
}

typedef struct _ThreadArgs {
	int clisockfd;
	int room;
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	int room = ((ThreadArgs*) args)->room;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nrcv;

	memset(buffer, 0, 256);

	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) {
		error("ERROR recv() failed!!!");
	}
	buffer[nrcv] = '\0';

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(clisockfd, buffer, 0, room);

		memset(buffer, 0, 256);
		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failing");
		buffer[nrcv] = '\0';
	}

	// Handle disconnect (broadcast leaving, remove node from linked list)
	broadcast(clisockfd, NULL, 1, room);

	pthread_mutex_lock(&mutex);

	USR* cur = head;
	if(cur->clisockfd == clisockfd) {
		head = cur->next;
		cur->next = NULL;
		free(cur);
	}
	while(cur->next != NULL) {
		if(cur->next->clisockfd == clisockfd) {
			// Remove node from linked list
			if(cur->next == tail) {
				tail = cur;
				free(cur->next);
				cur->next = NULL;
				break;
			}
			USR* temp = cur->next;
			cur->next = temp->next;
			free(temp);
			break;
		}
		cur = cur->next;
	}

	pthread_mutex_unlock(&mutex);

	print_list(); // Print all connected clients
	close(clisockfd);
	//-------------------------------
	rooms[room-1]--;

	pthread_exit(0);
}

int main(int argc, char *argv[])
{
	pthread_mutex_init(&mutex, NULL);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT_NUM);
	srand(time(NULL));

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

		// Get name
		char name[51];
		memset(name, 0, 50);
		int n = recv(newsockfd, name, 50, 0);
		if(n < 0) error("ERROR recv()ing name");
		name[n] = '\0';

		// Get room and place client into it
		char in_room[6];
		memset(in_room, 0, 5);
		n = recv(newsockfd, in_room, 5, 0);
		if(n < 0) error("ERROR recv()ing room");
		in_room[n] = '\0';

		int room;
		if(strcmp(in_room, "new") == 0) {
			int i;
			for(i=0; i<10; i++) {
				if(rooms[i] == 0)
					break;
			}

			// If all rooms are taken, communicate that to the client
			
			rooms[i]++;		// Indicate a client joined the room
			room = i + 1;	// Puts client in next room

			// Validate room
			char valid_room = 'v';
			n = send(newsockfd, &valid_room, 1, 0);
			if(n < 0) error("ERROR send()ing room validity");
			
			// Send room number to client
			char room_msg[64];
			memset(room_msg, 0, 64);
			sprintf(room_msg, "Connected to server with new room number %d\n", room);
			n = send(newsockfd, room_msg, 64, 0);
			if(n < 0) error("ERROR send()ing room message");

		} else if (strlen(in_room) == 0) {

			list_available_rooms(newsockfd);
			
			// Receive room from client
			char room_choice[6];
			memset(room_choice, 0, 6);
			n = recv(newsockfd, room_choice, 5, 0);
			if (n < 0) error("ERROR recv()ing room choice");
			room_choice[n] = '\0';
			
			if (strcmp(room_choice, "new") == 0) {
				// Create new room
				int i;
				for (i = 0; i < 10; i++) {
					if (rooms[i] == 0)
						break;
				}
				rooms[i]++;
				room = i + 1;

				// Validate room
				char valid_room = 'v';
				n = send(newsockfd, &valid_room, 1, 0);
				if(n < 0) error("ERROR send()ing room validity");
				
				// Notify client of room number
				char room_msg[64];
				memset(room_msg, 0, 64);
				sprintf(room_msg, "Connected to server with new room number %d\n", room);
				n = send(newsockfd, room_msg, 64, 0);
				if(n < 0) error("ERROR send()ing room message");
			} 
			else {
				// User selected existing room
				room = atoi(room_choice);

				// Validate room choice
				char valid_room;
				if (room > 10 || room < 1 || rooms[room-1] == 0) {
					valid_room = 'i';
					int n = send(newsockfd, &valid_room, 1, 0);
					if (n < 0) error("ERROR send()ing room validity");
					
					close(newsockfd);
					continue;
				} 
				else {
					valid_room = 'v';
					int n = send(newsockfd, &valid_room, 1, 0);
					if (n < 0) error("ERROR send()ing room validity");
					rooms[room-1]++;
				}

				// Send room message
				char room_msg[64];
				memset(room_msg, 0, 64);
				sprintf(room_msg, "Connected to server with new room number %d\n", room);
				n = send(newsockfd, room_msg, 64, 0);
				if(n < 0) error("ERROR send()ing room message");
			}
		}
		else {
			room = atoi(in_room);

			// If invalid room, send error message and close socket
			char valid_room;
			if(room > 10 || room < 1 || rooms[room-1] == 0) {
				valid_room = 'i';
				int n = send(newsockfd, &valid_room, 1, 0);
				if(n < 0) error("ERROR send()ing room validity");

				close(newsockfd);
				continue;
			}
			else {
				valid_room = 'v';
				int n = send(newsockfd, &valid_room, 1, 0);
				if(n < 0) error("ERROR send()ing room validity");
				rooms[room-1]++;
			}

			// Send message
			char room_msg[64];
			memset(room_msg, 0, 64);
			sprintf(room_msg, "Connected to server with new room number %d\n", room);
			n = send(newsockfd, room_msg, 64, 0);
			if(n < 0) error("ERROR send()ing room message");
		}

		add_tail(newsockfd, name, room); // add this new client to the client list
		broadcast(newsockfd, NULL, 2, room);
		print_list(); // print all connected clients

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;
		args->room = room;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	pthread_mutex_destroy(&mutex);
	return 0; 
}