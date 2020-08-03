#include <arpa/inet.h>
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
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int repfd = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent *server;

	int one = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(int));
	// int backups[2] = {2345, 3456};
	// int backups[2] = {2345};
	char *role = argv[1];
	int port;
	if(strcmp(role, "p") == 0) {
		port = 1234;
	} else {
		printf("bish *%s*", role);
		port = 2345;
	}

	struct sockaddr_in serveraddr, clientaddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);

	bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	listen(sockfd, 4);

	socklen_t clientlen = sizeof(clientaddr);

	char reg[250] = "empty";

	while(1) {
		printf("%s: try to accept\n", role);
		int client = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
		if(client == -1) {
			perror("accept");
			exit(1);
		}
		char buf[128];
		char cpy[128];
		memset(buf, 0, 128);
		read(client, buf, 128);
		strcpy(cpy, buf);

		char *op = strtok(cpy, "|");
		char *data = strtok(NULL, "|");
		printf("%s: op is %s, rest is %s\n", role, op, data);

		if(strcmp(op, "Put") == 0) {
			strcpy(reg, data);
			// acknowledge the client here.
			const char *msg = "ACK";
			write(client, msg, strlen(msg));
		} else {
			printf("replying %s", reg);
			write(client, reg, strlen(reg));
		}

		if((strcmp(role, "p") == 0) && (strcmp(op, "Put") == 0)) {
			// only THEN disseminate to replicas

			int repfd = socket(AF_INET, SOCK_STREAM, 0);
			server = gethostbyname("localhost");
			struct sockaddr_in repaddr;
			memset(&repaddr, 0, sizeof(repaddr));
			repaddr.sin_family = AF_INET;
			memcpy((char *)&repaddr.sin_addr.s_addr, server->h_addr, server->h_length);
			for(int i = 0; i < 1; i++) {
				repaddr.sin_port = htons(2345);
				if(connect(repfd, (struct sockaddr *)&repaddr, sizeof(repaddr)) == -1) {
					perror("connect");
					exit(1);
				}
				write(repfd, buf, strlen(buf));
			}
		}
	}

	close(sockfd);
	return 0;
}
