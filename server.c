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
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int one = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(int));

	struct sockaddr_in serveraddr, clientaddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)1234);

	bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	listen(sockfd, 4);

	socklen_t clientlen = sizeof(clientaddr);
	int client = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
	if(client == -1) {
		perror("accept");
		exit(1);
	}

	char buf[128];
	memset(buf, 0, 128);
	read(client, buf, 128);
	printf("Client wrote %s\n", buf);

	const char *msg = "Hello yourself!\n";
	write(client, msg, strlen(msg));
	close(client);
	close(sockfd);
	return 0;
}
