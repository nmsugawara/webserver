#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#define MAX_BACKLOG 5
#define DEFAULT_PORT "50000"
#define BUF_LINE_SIZE 1024
#define DEFAULT_VAR_SIZE 256
#define STATUS_CODE_NUMBER 6
#define LOG_FILE_NAME "/usr/local/bin/scd_education/sugawara/log/%s_%04d_request.log"
#define HTTP_HEADER "HTTP/1.1 %i %s\nContent-type: text/html\n charset=UTF-8\n\n"
#define DOCUMENT_ROOT "/usr/local/bin/scd_education/sugawara/www"
#define DIRECTORY_INDEX "/index.html"
#define ERR_FORBIDDEN "/err/403.html"
#define ERR_NOTFOUND "/err/404.html"
#define ERR_INTERNAL "/err/500.html"

struct status_list {
	int code; // http status code
	char msg[DEFAULT_VAR_SIZE]; // status message
};

static int listen_socket (char *port);
void http_response (int code, FILE *wsockf, FILE *outf);

int main (int argc, char *argv[]) {
	int lissock, cnt=1;
	lissock = listen_socket (DEFAULT_PORT);

	for (;;) {
		struct sockaddr_storage addr;

		socklen_t addrlen = sizeof addr;
		int accsock, code, content_length=0, count=1, tmp_length, file_status;
		char buf[BUF_LINE_SIZE], datestr[DEFAULT_VAR_SIZE], logfilename[DEFAULT_VAR_SIZE];
		char method[DEFAULT_VAR_SIZE], uri[BUF_LINE_SIZE], protocol[DEFAULT_VAR_SIZE];
		char request_path[BUF_LINE_SIZE] = DOCUMENT_ROOT;
		FILE *rsockf, *wsockf, *logf, *outf;
		time_t timer;
		struct tm *date;
		struct stat st;

		accsock = accept (lissock, (struct sockaddr*) &addr, &addrlen);

		if (accsock < 0) {
			fprintf (stderr, "accept failed\n");
			continue;
		}
		if ((rsockf = fdopen (accsock, "r")) == NULL) {
			fprintf (stderr, "socket fdopen read failed\n");
			continue;
		}
		if ((wsockf = fdopen (accsock, "w")) == NULL) {
			fprintf (stderr, "socket fdopen write failed\n");
			continue;
		}

		// log file
		timer = time (NULL);
		date = localtime (&timer);
		strftime (datestr, 20, "%Y%m%d%H%M%S", date);
		sprintf (logfilename, LOG_FILE_NAME, datestr, cnt);
		if ((logf = fopen (logfilename, "w")) == NULL) {
			code = 500; // 500 error
			outf = fopen (DOCUMENT_ROOT ERR_INTERNAL, "r");
			http_response (code, wsockf, outf);
			fclose (outf);
			fclose (wsockf);
			fclose (rsockf);
			continue;
		}

		while (fgets (buf, sizeof buf, rsockf)) {
 			// write log file
 			fputs (buf, logf);
			// analyze status line
			if (count == 1) {
				sscanf (buf, "%s %s %s", method, uri, protocol);
				if (strcmp (uri, "/") == 0) {
					// directory index
					strcat (request_path, DIRECTORY_INDEX);
				} else {
					strcat (request_path, uri);
				}
			}
			// case POST: get content length
			if (strncmp(buf, "Content-Length", 14) == 0) {
				sscanf (buf, "%*s %d", &content_length);
			}

			if (buf[0] == '\r') {
				// case POST: read body
				if (strcmp (method, "POST") == 0) {
					// output logfile while divide "buf" size
					tmp_length = content_length;
					while (tmp_length > 0) {
						if ( tmp_length < sizeof buf) {
							fgets (buf, tmp_length, rsockf);
						} else {
							fgets (buf, sizeof buf, rsockf);
						}
						fputs (buf, logf);
						tmp_length = tmp_length - sizeof buf+1;
					}
				}
				break;
			}
			count++;
		}
		fclose (logf);

		// check target file
		file_status = stat(request_path, &st);
		if (file_status == -1) {
			code = 404;	// 404 error
			outf = fopen (DOCUMENT_ROOT ERR_NOTFOUND, "r");
		} else {
			if ((outf = fopen (request_path, "r")) == NULL) {
				code =403; // 403 error
				outf = fopen (DOCUMENT_ROOT ERR_FORBIDDEN, "r");
			} else {
				code = 200; // OK
			}
		}

		// http response
		http_response (code, wsockf, outf);
		fclose (outf);
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

void http_response (int code, FILE *wsockf, FILE *outf) {
	// http status code list
	struct status_list slist[STATUS_CODE_NUMBER] = {
		{200, "OK"},
		{400, "Bad Request"},
		{403, "Forbidden"},
		{404, "Not Found"},
		{500, "Internal Server Error"},
		{503, "Service Unavailable"}
	};
	char hresh[BUF_LINE_SIZE], buf[BUF_LINE_SIZE], res_msg[DEFAULT_VAR_SIZE];
	int i;

	// get status message
	for (i=0; i<=STATUS_CODE_NUMBER; i++) {
		if (slist[i].code == code) {
			sprintf (res_msg, "%s", slist[i].msg);
		}
	}

	// header
	sprintf (hresh, HTTP_HEADER, code, res_msg);
	fputs (hresh, wsockf);

	// body
	while (fgets (buf, sizeof buf, outf)) {
		fputs (buf, wsockf);
	}
}
