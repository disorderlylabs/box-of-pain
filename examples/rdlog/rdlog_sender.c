#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// stolen from:  http://ntrg.cs.tcd.ie/undergrad/4ba2/multicast/antony/example.html

#define HELLO_PORT 12345
#define HELLO_GROUP "225.0.0.37"

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	struct sockaddr_in addr;
	int fd;
	// int cnt;
	// struct ip_mreq mreq;
	const char *message = "Hello, World!";

	/* create what looks like an ordinary UDP socket */
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/* set up destination address */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(HELLO_GROUP);
	addr.sin_port = htons(HELLO_PORT);

	/* now just sendto() our destination! */
	while(1) {
		if(sendto(fd, message, sizeof(message), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("sendto");
			exit(1);
		}
		sleep(1);
	}
}
