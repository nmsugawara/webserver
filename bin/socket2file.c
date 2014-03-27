#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#define MAX_BACKLOG 5
#define DEFAULT_PORT "50000"
#define BUF_LINE_SIZE 1024

static int listen_socket (char *port);

int main (int argc, char *argv[]) {
	int lissock, fcnt=1;

	lissock = listen_socket (DEFAULT_PORT);
	for (;;) {
		struct sockaddr_storage addr;
		socklen_t addrlen = sizeof addr;
		int accsock;
		char buf[BUF_LINE_SIZE], filename[100];
		FILE *sockf, *fp;

		accsock = accept (lissock, (struct sockaddr*) &addr, &addrlen);
		if (accsock < 0) {
			fprintf (stderr, "accept failed\n");
			continue;
		}

		sockf = fdopen (accsock, "r");
		setvbuf (sockf, NULL, _IONBF, 0);
		sprintf (filename, "../log/%d_%04d_request.log", accsock, fcnt);
		fp = fopen (filename, "w");
		if (!fp) {
			fprintf (stdout, "log file not exist error\n");
		}
		while (fgets (buf, sizeof buf, sockf)) {
			fputs (buf, fp);
		}
		fclose (fp);
		fclose (sockf);
		close (accsock);
		fcnt++;
	}
}

static int listen_socket (char *port) {
	struct addrinfo hints, *res, *ai;
	int err;

	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_INET; /* IPv4 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; /* Server Side */
	if ((err = getaddrinfo (NULL, port, &hints, &res)) != 0) {
		fprintf (stderr, "getaddrinfo error");
		exit (-1);
	}
	for (ai = res; ai; ai = ai->ai_next) {
		int sock;

		//fprintf (stdout, "ai_next:%i\n", ai);

		sock = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0) {
			fprintf (stdout, "cannot create socket\n");
			continue;
		}
		if (bind (sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			close (sock);
			fprintf (stdout, "cannot bind socket\n");
			continue;
		}
		if (listen (sock, MAX_BACKLOG) < 0) {
			close (sock);
			fprintf (stdout, "cannot listen socket\n");
			continue;
		}
		freeaddrinfo (res);
		return sock;
	}

	fprintf (stderr, "failed to listen socket\n");
	exit (-1);
}

