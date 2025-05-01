#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT_NUM 1004


void display_clients();
void add_client();
void remove_client();
void broadcast_msg();
void notification();
void assign_color();



void error(const char *msg){
	perror(msg);
	exit(1);
}

int main(int argc, char* argv[]){


    return 0;
}