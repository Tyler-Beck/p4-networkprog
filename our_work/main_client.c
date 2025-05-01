#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define PORT_NUM 1004

void error(const char *msg){
	perror(msg);
	exit(0);
}

int main(int argc, char *argv[]){
    
    if (argc < 2) error("Please speicify hostname");

    return 0;
}

