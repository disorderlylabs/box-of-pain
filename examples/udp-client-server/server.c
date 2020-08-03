#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	int one = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(int));

	struct sockaddr_in serveraddr, clientaddr;

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)1234);

	if(bind(sockfd, (struct sockaddr *)(&serveraddr), sizeof(struct sockaddr_in)) < 0) {
		perror("Server: bind failed");
		return EXIT_FAILURE;
	}
	listen(sockfd, 4);

	socklen_t clientlen = sizeof(clientaddr);

	char buf[128];
	memset(buf, 0, 128);

	recvfrom(sockfd, buf, 128, 0, (struct sockaddr *)(&clientaddr), &clientlen);

	fprintf(stderr, "Client wrote %s\n", buf);

	const char *msg = "Hello yourself!\n";
	if(sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)(&clientaddr), clientlen) <= 0) {
		fprintf(stderr, "DETECTED FAILURE TO WRITE TO SOCKET\n");
	}

	msg = "Another message from server\n";
	if(sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)(&clientaddr), clientlen) <= 0) {
		fprintf(stderr, "DETECTED FAILURE TO WRITE TO SOCKET\n");
	}

	close(sockfd);
	return 0;
}
