#include <stdio.h>
#include <stdlib.h>
#define BUF_LINE_SIZE 1024

int main (int argc, char *argv[]) {
	FILE *fin, *fout;
	char buf[BUF_LINE_SIZE];

	if (argc != 3) {
		fprintf (stderr, "wrong argments\n");
		exit (1);
	}

	fin = fopen(argv[1], "r");
	if (!fin) {
		fprintf (stderr, "no such input file exist\n");
		exit (1);
	}

	fout = fopen(argv[2], "w");
	if (!fout) {
		fprintf (stderr, "output file open error\n");
		exit (1);
	}

	while(fgets (buf, sizeof(buf), fin)) {
		fputs (buf, fout);
	}

	fclose (fin);
	fclose (fout);
	exit (0);
}