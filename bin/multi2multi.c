#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <getopt.h>

#define MAX_BACKLOG 5
#define DEFAULT_PORT "50000"
#define BUF_LINE_SIZE 1024
#define DEFAULT_VAR_SIZE 256
#define LOG_FILE_NAME "/usr/local/bin/scd_education/sugawara/log/%s_%04d_request.log"
#define MAX_POST_PARAM 10
#define LOGIN_MAIL_PARAM "mail"
#define LOGIN_JOB_PARAM "job"
#define LOGIN_PURPOSE_PARAM "purpose"
#define POST_INFO_FORMAT "mail: %s\njob: %s\npurpose: %s\n"

//cookie
#define COOKIE_EXPIRE (60 * 60 * 24 * 30)
#define COOKIE_FORMAT "Set-Cookie: %s=%d; expires=%s; path=/;\n"
#define COOKIE_NAME "login"
#define COOKIE_VALUE 1
#define DELETE_COOKIE_DATE "Fri, 31-Dec-1999 23:59:59 GMT"

#define HTTP_HEADER "HTTP/1.1 %i %s\nContent-type: text/html\ncharset=UTF-8\n"
#define REDIRECT_FORMAT "Location: %s:%s%s\n"
#define DOMAIN "http://54.199.190.207"
#define DOCUMENT_ROOT "/usr/local/bin/scd_education/sugawara/www"
#define DIRECTORY_INDEX "/login.html"
#define LOGIN_CHECK_PATH "/login_check.html"
#define LOGOUT_PATH "/logout.html"
#define WELCOME_PATH "/welcome.html"
#define STATUS_LIST_NUMBER ((sizeof slist)/(sizeof (struct status_list)))
#define CMD_PHP_PATH "/usr/bin/php %s"

struct status_list {
	int code; // http status code
	char msg[DEFAULT_VAR_SIZE]; // status message
	char path[DEFAULT_VAR_SIZE]; // error page file path
};

static int listen_socket (char *port);
char *get_http_status_message (struct status_list *slist, int list_number, int code);
char *get_http_response_path (struct status_list *slist, int list_number, int code);
void response_http (char *http_response_header, FILE *readf, FILE *wsockf);
void kill_child_process();

int main (int argc, char *argv[]) {

	//---------------------
	// daemonize
	//---------------------
	switch (fork ()) {
		case -1:
			return (-1);
		case 0:
			break;
		default:
			exit (0);
	}

	if (setsid() == -1) {
		return (-1);
	}

	if( chdir("/") < 0 ) {
		return (-1);
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	//---------------------
	// if set arguments, receive value
	//---------------------
	int opt, option_index = 0;
	char *document_root, *port;
	document_root = DOCUMENT_ROOT;
	port = DEFAULT_PORT;

	struct option long_options[] = {
		{"docroot", required_argument, NULL, 'd'}, // document_root
		{"port", required_argument, NULL, 'p'}, // port
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long (argc, argv, "d:p:", long_options, &option_index)) != -1) {
		switch (opt) {
			case 'd':
				document_root = optarg;
				fprintf (stdout, "%c\n", opt);
				fprintf(stdout, "%s\n", document_root);
				break;
			case 'p':
				port = optarg;
				fprintf (stdout, "%c\n", opt);
				fprintf(stdout, "%s\n", port);
				break;
			case '?':
				exit(1);
		}

	}

	//---------------------
	// main 
	//---------------------
	int lissock, cnt=1;
	// http status code list
	struct status_list slist[] = {
		{200, "OK", ""},
		{303, "See Other", ""},
		{400, "Bad Request", ""},
		{403, "Forbidden", "/err/403.html"},
		{404, "Not Found", "/err/404.html"},
		{500, "Internal Server Error", "/err/500.html"},
		{503, "Service Unavailable", "/err/503.html"}
	};

	//---------------------
	// make socket, bind, listen
	//---------------------
	lissock = listen_socket (port);

	for (;;) {
		struct sockaddr_storage addr;
		socklen_t addrlen = sizeof addr;
		int accsock, fork_status, code, file_status;
		int content_length = 0, line = 1, login_flg = 0, list_number = STATUS_LIST_NUMBER;
		char buf[BUF_LINE_SIZE];
		// use log
		char datestr[DEFAULT_VAR_SIZE], logfilename[DEFAULT_VAR_SIZE];
		// use status line
		char method[DEFAULT_VAR_SIZE], uri[BUF_LINE_SIZE], protocol[DEFAULT_VAR_SIZE];
		// use cookie
		char header_cookie[DEFAULT_VAR_SIZE], expire[DEFAULT_VAR_SIZE], cookie_info[DEFAULT_VAR_SIZE];
		char *cookie_name;
		int set_cookie_flg = 0, cookiev = COOKIE_VALUE;
		// use read post info
		char *post_body_buf;
		char *parse_param, *parse_string;
		char mail[DEFAULT_VAR_SIZE], job[DEFAULT_VAR_SIZE], purpose[DEFAULT_VAR_SIZE], post_format[DEFAULT_VAR_SIZE];
		char parse_param_tmp[MAX_POST_PARAM][DEFAULT_VAR_SIZE], parse_string_tmp[DEFAULT_VAR_SIZE];
		int param_count = 0, param_count_tmp;
		// use request/response info
		char request_path[BUF_LINE_SIZE], redirect_path[BUF_LINE_SIZE];
		char response_message[DEFAULT_VAR_SIZE], http_response_header[BUF_LINE_SIZE], header_location[BUF_LINE_SIZE];
		// use output tmp file each filename extension
		char *filename_extension;
		char cmd_path[DEFAULT_VAR_SIZE];

		FILE *rsockf, *wsockf, *logf, *tmpf, *outf;
		time_t timer;
		struct tm *date;
		struct stat st;

		//---------------------
		// accept
		//---------------------
		accsock = accept (lissock, (struct sockaddr*) &addr, &addrlen);
		if (accsock < 0) {
			fprintf (stderr, "accept failed\n");
			continue;
		}

		//---------------------
		// fork
		//---------------------
		fork_status = fork ();
		// case fork error:
		if (fork_status == -1) {
			fprintf (stderr, "fork failled\n");
			continue;
		// case parent process
		} else if (fork_status > 0) {
			// case child process exit: receive status and kill proscess
			signal (SIGCHLD, kill_child_process);
			close (accsock);
			continue;
		// case chiled process:
		} else {
			//---------------------
			// make socket file descriptor
			//---------------------
			if ((rsockf = fdopen (accsock, "r")) == NULL) {
				fprintf (stderr, "socket fdopen read failed\n");
				close(accsock);
				close(lissock);
				exit (0);
			}
			if ((wsockf = fdopen (accsock, "w")) == NULL) {
				fprintf (stderr, "socket fdopen write failed\n");
				close(accsock);
				close(lissock);
				exit (0);
			}

			//---------------------
			// read http request
			//---------------------
			// initialize request_path
			sprintf(request_path, "%s", document_root);

			// log file
			timer = time (NULL);
			date = localtime (&timer);
			strftime (datestr, 20, "%Y%m%d%H%M%S", date);
			sprintf (logfilename, LOG_FILE_NAME, datestr, cnt);
			if ((logf = fopen (logfilename, "w")) == NULL) {
				code = 500;
				sprintf (response_message, "%s", get_http_status_message (slist, list_number, code));
				strcat (request_path, get_http_response_path (slist, list_number, code));
				sprintf(http_response_header, HTTP_HEADER, code, response_message);
				strcat (http_response_header, "\n");
				if ((outf = fopen (request_path, "r")) == NULL) {
					fprintf (stderr, "log file can't open\n");
					fclose (wsockf);
					fclose (rsockf);
					close (accsock);
					close (lissock);
					exit (-1);
				}
				response_http (http_response_header, outf, wsockf);
				fclose (outf);
				fclose (wsockf);
				fclose (rsockf);
				close (accsock);
				close (lissock);
				exit (-1);
			}

			// read until EOF
			while (fgets (buf, sizeof buf, rsockf)) {
 				/// write log file
 				fputs (buf, logf);

				/// analyze
				// case status line: variable assignment
				if (line == 1) {
					sscanf (buf, "%s %s %s", method, uri, protocol);
					line++;
					continue;
				}
				// get content length
				if (strncmp(buf, "Content-Length:", 15) == 0) {
					sscanf (buf, "%*s %d", &content_length);
					continue;
				}
				// case set cookie: login auth check
				// TODO: case multi cookie exist
				if (strncmp(buf, "Cookie:", 7) == 0) {
					sscanf (buf, "%*s %s", cookie_info);
					if ((cookie_name = strtok (cookie_info, "=")) != NULL) {
						if (strcmp(cookie_name, COOKIE_NAME) == 0) {
							// login ok
							login_flg = 1;
						}
					}
					continue;
				}
				// case not "line feed code" line: continue
				if (buf[0] != '\r') {
					continue;
				}
				// case not POST: finish analyze
				if (strcmp (method, "POST") != 0) {
					break;
				}
				// case POST: write logfile
				post_body_buf = (char *) malloc (content_length+1);
				if (post_body_buf == NULL) {
					fprintf (stderr, "failure to reserve memory\n");
					break;
				}

				fgets (post_body_buf, content_length+1, rsockf);
				fputs (post_body_buf, logf);

				// divide POST data to "parameter unit"
				parse_param = strtok (post_body_buf, "&");
				while (parse_param != NULL) {
					sprintf(parse_param_tmp[param_count], "%s", parse_param);
					param_count++;
					parse_param = strtok (NULL, "&");
				}

				for (param_count_tmp=0; param_count_tmp<=param_count; param_count_tmp++) {
					parse_string = strtok (parse_param_tmp[param_count_tmp], "=");
					snprintf(parse_string_tmp, sizeof parse_string_tmp, "%s", parse_string);
					// case parameter = "mail": assigns values to variable
					if (strstr(parse_string_tmp, LOGIN_MAIL_PARAM) != NULL) {
					parse_string = strtok (NULL, "=");
						snprintf (mail, sizeof parse_string_tmp, "%s", parse_string);
					}
					// case parameter = "job": assigns values to variable
					if (strstr(parse_string_tmp, LOGIN_JOB_PARAM) != NULL) {
						parse_string = strtok (NULL, "=");
						snprintf (job, sizeof parse_string_tmp, "%s", parse_string);
					}
					// case parameter = "purpose": assigns values to variable
					if (strstr(parse_string_tmp, LOGIN_PURPOSE_PARAM) != NULL) {
						parse_string = strtok (NULL, "=");
						strncat (purpose, parse_string_tmp, sizeof parse_string);
						strcat (purpose, ",");
					}
				}

				free(post_body_buf);
				// write file to POST parameter data
				fputs ("\n\nLoginFormInfo:\n", logf);
				sprintf (post_format, POST_INFO_FORMAT, mail, job, purpose);
				fputs (post_format, logf);
				break;
			}
			fclose (logf);

			sleep(10);

			//---------------------
			// 1. analyze request path
			// 2. check request path validity
			// 3. output tmp file each filename extension
			// 4. make response header
			// 5. response http
			//---------------------

			//---------------------
			// analyze request path
			//---------------------
			// case uri is "directory_index"
			if (strcmp (uri, "/") == 0 || strcmp (uri, DIRECTORY_INDEX) == 0) {
				strcat (request_path, DIRECTORY_INDEX);
			// case uri is "login check path"
			} else if (strcmp (uri, LOGIN_CHECK_PATH) == 0) {
				// add cookie flg
				set_cookie_flg = 1;
				// http status code
				code = 303;
				// request path
				strcat (request_path, LOGIN_CHECK_PATH);
				// redirect path
				sprintf (redirect_path, WELCOME_PATH);
			// case uri is "logout path"
			} else if (strcmp (uri, LOGOUT_PATH) == 0) {
				// remove cookie flg
				set_cookie_flg = -1;
				// http status code
				code = 303;
				// request path
				strcat (request_path, LOGOUT_PATH);
				// redirect path
				sprintf (redirect_path, DIRECTORY_INDEX);
			// case uri is other
			} else {
				if (login_flg == 0) {
					code = 403;
				}
				strcat (request_path, uri);
			}

			//---------------------
			// check request path validity
			//---------------------
			file_status = stat(request_path, &st);
			// case no such file:
			if (file_status == -1) {
				code = 404;
				sprintf (request_path, "%s", document_root);
				strcat (request_path, get_http_response_path (slist, list_number, code));
			} else {
				// case not login & request expect "directory index"
				if (code == 403) {
					sprintf (request_path, "%s", document_root);
					strcat (request_path, get_http_response_path (slist, list_number, code));
				} else if (code == 0) {
					code = 200;
				}
			}

			//---------------------
			// output tmp file each filename extension
			//---------------------
			filename_extension = strstr (request_path, ".");
			if (strcmp(filename_extension, ".html") == 0) {
				if ((outf = fopen (request_path, "r")) == NULL) {
					code = 403;
					sprintf (request_path, document_root);
					strcat (request_path, get_http_response_path (slist, list_number, code));
					if ((outf = fopen (request_path, "r")) == NULL) {
						fprintf (stdout, "file can't open\n");
						fclose (wsockf);
						fclose (rsockf);
						close (accsock);
						close (lissock);
						exit (-1);
					}
				}
			} else {
				// case php:
				if (strcmp(filename_extension, ".php") == 0) {
					sprintf(cmd_path, CMD_PHP_PATH, request_path);
				}
				// output tmp file
				if ((outf = popen (cmd_path, "r")) == NULL) {
					code = 403;
					sprintf (request_path, document_root);
					strcat (request_path, get_http_response_path (slist, list_number, code));
					if ((outf = fopen (request_path, "r")) == NULL) {
						fprintf (stdout, "file can't open\n");
						fclose (wsockf);
						fclose (rsockf);
						close (accsock);
						close (lissock);
						exit (-1);
					}
				}
				if ((tmpf = tmpfile ()) == NULL) {
					code = 500;
					sprintf (request_path, document_root);
					strcat (request_path, get_http_response_path (slist, list_number, code));
					if ((outf = fopen (request_path, "r")) == NULL) {
						fprintf (stdout, "file can't open\n");
						fclose (wsockf);
						fclose (rsockf);
						close (accsock);
						close (lissock);
						exit (-1);
					}
				}
				while (fgets (buf, sizeof buf, outf)) {
					fputs (buf, tmpf);
				}
				pclose (outf);
				rewind (tmpf);
				outf = tmpf;
			}

			//---------------------
			// make response header
			//---------------------
			sprintf (response_message, "%s", get_http_status_message (slist, list_number, code));
			sprintf(http_response_header, HTTP_HEADER, code, response_message);

			/// cookie
			if (set_cookie_flg == 1) {
				// set expire
				timer = time(NULL);
				timer += COOKIE_EXPIRE;
				date = gmtime(&timer);
				strftime(expire, DEFAULT_VAR_SIZE, "%a, %d-%b-%Y %H:%M:%S GMT", date);
				// add "Set-Cookie" line
				sprintf(header_cookie, COOKIE_FORMAT, COOKIE_NAME, cookiev, expire);
				strcat (http_response_header, header_cookie);
			} else if (set_cookie_flg == -1) {
			// remove cookie
				// set expire
				sprintf (expire, "%s", DELETE_COOKIE_DATE);
				// add "Set-Cookie" line
				sprintf(header_cookie, COOKIE_FORMAT, COOKIE_NAME, cookiev, expire);
				strcat (http_response_header, header_cookie);
			}

			/// redirect (Location)
			if (code == 303) {
				// add "Location" line
				sprintf (header_location, REDIRECT_FORMAT, DOMAIN, port, redirect_path);
				strcat (http_response_header, header_location);
			}
			strcat (http_response_header, "\n");

			//---------------------
			// response http
			//---------------------
			response_http (http_response_header, outf, wsockf);
			fclose (outf);
			fclose (wsockf);
			fclose (rsockf);
			close (accsock);
			close (lissock);
			cnt++;
			exit (0);
		}
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

char *get_http_status_message (struct status_list *slist, int list_number, int code) {
	char message[DEFAULT_VAR_SIZE], *messagept;
	int i;

	messagept = message;
	for (i=0; i<=list_number; i++) {
		if (slist[i].code == code) {
			sprintf (messagept, "%s", slist[i].msg);
		}
	}
	return messagept;
}

char *get_http_response_path (struct status_list *slist, int list_number, int code) {
	char path[DEFAULT_VAR_SIZE], *pathpt;
	int i;

	pathpt = path;
	for (i=0; i<=list_number; i++) {
		if (slist[i].code == code) {
			sprintf (pathpt, "%s", slist[i].path);
		}
	}
	return pathpt;
}

void response_http (char *http_response_header, FILE *readf, FILE *wsockf) {
	char buf[BUF_LINE_SIZE];

	// header
	fputs (http_response_header, wsockf);

	// body
	while (fgets (buf, sizeof buf, readf)) {
		fputs (buf, wsockf);
	}
}

void kill_child_process () {
	int status;

	wait(&status);
}
