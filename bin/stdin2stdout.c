#include <stdio.h>
#include <stdlib.h>
#define BUF_LINE_SIZE 1024

int main (int argc, char *argv[]) {
	char buf[BUF_LINE_SIZE];

	if (!fgets(buf, sizeof(buf), stdin)) {
		fprintf(stderr, "no input line\n");
		exit (1);
	}
	fputs(buf, stdout);
	exit (0);
}
