#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
	struct stat statbuf;
	int r = stat("/proc/self/ns/user", &statbuf);
	if (r == 0) {
		printf("%ld\n", statbuf.st_ino);
	}
	exit(37); // Starter expects this.
}
