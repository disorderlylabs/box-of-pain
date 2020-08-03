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
	int sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent *server;

	printf("Using localhost\n");
	server = gethostbyname("localhost");

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	memcpy((char *)&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);
	serveraddr.sin_port = htons(1234);

	if(connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
		perror("connect");
		exit(1);
	}

	const char *msg = "Put|foo";
	// sleep(getpid() % 2);
	write(sockfd, msg, strlen(msg));

	char buf[128];
	memset(buf, 0, 128);
	read(sockfd, buf, 20);

	printf("GOT %s", buf);

	close(sockfd);
	if(strcmp(buf, "ACK") == 0) {
		// request the value
		if(connect(sockfd2, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
			printf("primary fails\n");
			perror("connect");
			serveraddr.sin_port = htons(2345);
			int sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
			if(connect(sockfd2, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
				perror("connect back");
				printf("No one is alive\n");
				exit(0);
			}

			const char *msg = "Get";
			write(sockfd2, msg, strlen(msg));
			read(sockfd2, buf, 128);
			if(strcmp(buf, "foo") != 0) {
				printf("OH NO!");
				exit(1);
			}
		} else {
			const char *msg = "Get";
			write(sockfd2, msg, strlen(msg));
			read(sockfd2, buf, 128);
			if(strcmp(buf, "foo") != 0) {
				printf("OH NO!");
				exit(1);
			}
		}
	}
	printf("YAY");
	return 0;
}
