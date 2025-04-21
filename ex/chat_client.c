#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define PORT_NUM 1004

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

int main(int argc, char *argv[])
{
	if (argc < 2) error("Please speicify hostname");

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct hostent* server = gethostbyname(argv[1]);
	if (server == NULL) error("ERROR, no such host");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((char*)server->h_addr, 
			(char*)&serv_addr.sin_addr.s_addr,
			server->h_length);
	serv_addr.sin_port = htons(PORT_NUM);

	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

	char buffer[256];
	int n;

	while (1) {
		printf("Please enter the message: ");
		memset(buffer, 0, 256);
		fgets(buffer, 255, stdin);

		// since fgets() considers '\n' as a valid character,
		// even if you type nothing and enter, you will still
		// have message of length 1. so, you want to remove that
		if (strlen(buffer) == 1) buffer[0] = '\0';

		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");

		if (n == 0) break; // we stop transmission when user type empty string

		memset(buffer, 0, 256);
		n = recv(sockfd, buffer, 255, 0);
		if (n < 0) error("ERROR reading from socket");

		printf("Message from server: %s\n", buffer);
	}

	close(sockfd);

	return 0;
}

