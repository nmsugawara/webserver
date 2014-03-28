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
#define HTTP_HEADER "HTTP/1.1 %i %s\nContent-type: text/html\n charset=UTF-8\n\n"
#define DOCUMENT_ROOT "/usr/local/bin/scd_education/sugawara/www"
#define STATUS_CODE_NUMBER 6

struct status_list {
	int code; // HTTPステータスコード
	char msg[256]; // ステータスメッセージ
};

static int listen_socket (char *port);
char *get_http_status_msg (struct status_list *slist, int status_code);

int main (int argc, char *argv[]) {
	int lissock, cnt=1;
	struct status_list slist[STATUS_CODE_NUMBER] = {
		{200, "OK"},
		{400, "Bad Request"},
		{403, "Forbidden"},
		{404, "Not Found"},
		{500, "Internal Server Error"},
		{503, "Service Unavailable"}
	};

	lissock = listen_socket (DEFAULT_PORT);
	for (;;) {
		struct sockaddr_storage addr;

		socklen_t addrlen = sizeof addr;
		int accsock, code, content_length=0, count=1, tmp_length;
		char buf[BUF_LINE_SIZE], datestr[32], logfilename[256];
		char method[16], uri[BUF_LINE_SIZE], protocol[16], hresh[100], msg[256];
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
		if (logf == NULL) {
			// 内部エラー?
			code = 500;
			wsockf = fdopen (accsock, "w");
			// header
			sprintf (hresh, HTTP_HEADER, code, get_http_status_msg(slist, code));
			fputs (hresh, wsockf);
                        // body
                        sprintf (msg, "%i %s\n", code, get_http_status_msg(slist, code));
                        fputs(msg, wsockf);
			fclose (wsockf);
			fclose (rsockf);
			continue;
		}
		while (fgets (buf, sizeof buf, rsockf)) {
			if (buf[0] == '\r') {
				// POSTの場合BODYも読み込み
				if (strcmp (method, "POST") == 0) {
					// bufのサイズ毎に分割して出力
					tmp_length = content_length;
					while (tmp_length > 0) {
						if ( tmp_length < sizeof buf) {
							fgets (buf, tmp_length+1, rsockf);
						} else {
							fgets (buf, sizeof buf, rsockf);
						}
						fputs (buf, logf);
						tmp_length = tmp_length - sizeof buf+1;
					}
				}
				break;
			}
			// ログファイル書き込み
			fputs (buf, logf);
			// リクエストライン解析
			if (count == 1) {
              			sscanf (buf, "%s %s %s", method, uri, protocol);
                		if (strcmp (uri, "/") == 0) {
                        		strcat (request_path, "/index.html");
                		} else {
                        		strcat (request_path, uri);
                		}
			}
			// POSTの場合のBODYのサイズ取得
			if (strncmp(buf, "Content-Length", 14) == 0) {
				sscanf (buf, "%*s %d", &content_length);
			}
			count++;
		}
		fclose (logf);

		// HTTPレスポンス
                wsockf = fdopen (accsock, "w");
		outf = fopen (request_path, "r");
		if (outf == NULL) {
			code = 404;
			// header
			sprintf (hresh, HTTP_HEADER, code, get_http_status_msg(slist, code));
			fputs (hresh, wsockf);
			// body
			sprintf (msg, "%i %s\n", code, get_http_status_msg(slist, code));
			fputs(msg, wsockf);
		} else {
			code = 200;
			// header
			sprintf (hresh, HTTP_HEADER, code, get_http_status_msg(slist, code));
			fputs (hresh, wsockf);
			// body
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

char *get_http_status_msg (struct status_list *slist, int status_code) {
	char *res_msg;
	int i;

	for (i=0; i<=STATUS_CODE_NUMBER; i++) {
		if (slist[i].code == status_code) {
			res_msg = malloc (strlen(slist[i].msg)+1);
			sprintf (res_msg, "%s", slist[i].msg);
		}
	}
	return res_msg;
}
