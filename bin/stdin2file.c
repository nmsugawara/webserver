#include <stdio.h>
#include <stdlib.h>
#define BUF_LINE_SIZE 1024

int main (int argc, char *argv[]) {
	FILE *f;
	char buf[BUF_LINE_SIZE];

	if (argc != 2) {
		fprintf (stderr, "wrong argments\n");
		exit (1);
	}
	f = fopen(argv[1], "w");
	fgets (buf, sizeof buf, stdin);
	fputs (buf, f);
	fclose(f);
	exit (0);
}
