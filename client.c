#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

int main()
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct hostent *server = gethostbyname("localhost");
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
	write(sockfd, msg, strlen(msg));

	char buf[128];
	memset(buf, 0, 128);
	read(sockfd, buf, 20);

	fprintf(stderr, "Reply: %s\nLooking for second message...\n", buf);

	if(read(sockfd, buf, 128) <= 0) {
		fprintf(stderr, "Second message not found!\n");
		return 1;
	}
	fprintf(stderr, "Found it!\n");
	close(sockfd);
	return 0;
}

