#include <stdio.h>
#include <stdlib.h>

int main( int argc, char **argv )
{
	char *null = NULL;
	char *space;
	int dumpnow;

	space = malloc(600000);

	/* try a couple of different ways to dump core. */
	dumpnow = 7 / 0;
	*null = '\0';

	return 0;
}
