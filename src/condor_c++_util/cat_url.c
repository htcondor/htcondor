#include <sys/fcntl.h>

main(argc, argv)
int		argc;
char	*argv[];
{
	int		i;
	char	buf[1024];
	int		count;
	int		fd;

	for (i = 1; i < argc; i++) {
		fd = open_url(argv[i], O_RDONLY, 0);
		if (fd >= 0) {
			do {
				count = read(fd, buf, sizeof(buf));
				if (count > 0) {
					write(1, buf, count);
				}
			} while (count > 0);
		}
	}
}
