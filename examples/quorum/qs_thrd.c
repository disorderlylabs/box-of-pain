#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

_Atomic int votes = 0;
void *thrd_main(void *arg)
{
	int client = (long)arg;

	char buf[128];
	memset(buf, 0, 128);
	read(client, buf, 128);
	fprintf(stderr, "CLIENT %d\n", client);
	// char *ip = inet_ntoa(clientaddr.sin_addr);
	// short port = clientaddr.sin_port;
	// fprintf(stderr, "Client %s:%5.5hu wrote\n", ip, port);

	if(strcmp(buf, "Hello, Server!\n") == 0) {
		votes++;
	}
	const char *msg = "Hello yourself!\n";
	write(client, msg, strlen(msg));
	close(client);
	if(votes == 3) {
		exit(0);
	}
	return NULL;
}

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

	while(votes < 3) {
		int client = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
		if(client == -1) {
			perror("accept");
			exit(1);
		}
		pthread_t thrd;
		pthread_create(&thrd, NULL, thrd_main, (void *)(long)client);
	}
	printf("Success!\n");
	close(sockfd);
	return 0;
}
