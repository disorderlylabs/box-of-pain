#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent *server;

	printf("Using containerized server\n");
	server = gethostbyname("server");

	if(server == NULL) {
		printf("Using localhost\n");
		server = gethostbyname("localhost");
	}

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	memcpy((char *)&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);
	serveraddr.sin_port = htons(1234);

	if(connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
		perror("connect");
		exit(1);
	}

	const char *msg = "Hello, Server!\n";
	// sleep(getpid() % 2);
	write(sockfd, msg, strlen(msg));

	char buf[128];
	memset(buf, 0, 128);
	read(sockfd, buf, 20);

	fprintf(stderr, "Reply: \nLooking for second message...\n");

	if(read(sockfd, buf, 128) <= 0) {
		fprintf(stderr, "Second message not found!\n");
		return 1;
	}
	fprintf(stderr, "Found it!\n");
	close(sockfd);
	return 0;
}
