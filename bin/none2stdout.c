#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[]) {
	int cnt;

	fprintf (stdout, "argc: %d", argc);
	for (cnt=0;cnt<argc;cnt++) {
		fprintf (stdout, ", argv[%d]: ", cnt);
		fputs (argv[cnt], stdout);
	}
	fputs ("\n", stdout);
	exit (0);
}