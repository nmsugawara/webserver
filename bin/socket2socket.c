#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#define MAX_BACKLOG 5
#define DEFAULT_PORT "50000"
#define BUF_LINE_SIZE 1024
#define LOG_FILE_NAME "/usr/local/bin/scd_education/sugawara/log/%s_%04d_request.log"
#define HTTP_HEADER "HTTP/1.1 %s\nContent-type: text/html\n\n"
#define DOCUMENT_ROOT "/usr/local/bin/scd_education/sugawara/www"

static int listen_socket (char *port);

int main (int argc, char *argv[]) {
	int lissock, cnt=1;

	lissock = listen_socket (DEFAULT_PORT);
	for (;;) {
		struct sockaddr_storage addr;
		socklen_t addrlen = sizeof addr;
		int accsock, count=1;
		char buf[BUF_LINE_SIZE], datestr[20], logfilename[256], method[100], uri[BUF_LINE_SIZE], protocol[100], hresh[100];
		char request_path[BUF_LINE_SIZE] = DOCUMENT_ROOT;
		FILE *rsockf, *wsockf, *logf, *outf;
		time_t timer;
		struct tm *date;

		accsock = accept (lissock, (struct sockaddr*) &addr, &addrlen);
		if (accsock < 0) {
			fprintf (stderr, "accept failed\n");
			continue;
		}

		// HTTPリクエスト
		timer = time (NULL);
		date = localtime (&timer);
		strftime (datestr, 20, "%Y%m%d%H%M%S", date);
		rsockf = fdopen (accsock, "r");
		sprintf (logfilename, LOG_FILE_NAME, datestr, cnt);
		logf = fopen (logfilename, "w");
		while (fgets (buf, sizeof buf, rsockf)) {
			if (buf[0] == '\r') {
				break;
			}	
			fputs (buf, logf);
			if (count == 1) {
              			sscanf (buf, "%s %s %s", method, uri, protocol);
                		if (strcmp (uri, "/") == 0) {
                        		strcat (request_path, "/index.html");
                		} else {
                        		strcat (request_path, uri);
                		}
			}
			count++;
		}
		fclose (logf);

		// HTTPレスポンス
                wsockf = fdopen (accsock, "w");
		outf = fopen (request_path, "r");
		if (outf == NULL) {
			sprintf (hresh, HTTP_HEADER, "404 Not Found");
			fputs (hresh, wsockf);
			fputs("404 Not Found\n", wsockf);
		} else {
			sprintf (hresh, HTTP_HEADER, "200 Ok");
			fputs (hresh, wsockf);
	                while (fgets (buf, sizeof buf, outf)) {
        	                fputs (buf, wsockf);
                	}
                	fclose (outf);
		}
		fclose (wsockf);
                fclose (rsockf);
		cnt++;
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
