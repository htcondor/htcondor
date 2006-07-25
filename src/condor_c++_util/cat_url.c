/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include <sys/fcntl.h>
#include <stdio.h>

main(argc, argv)
int		argc;
char	*argv[];
{
	int		i;
	char	buf[1024];
	int		count;
	int		read_fd = -1;
	int		write_fd = 1;

	if (argv[1][0] == '-' && argv[1][1] == 'w') {
		read_fd = 0;
		write_fd = open_url(argv[2], O_WRONLY, 0);
		argv++;
		argc--;
	}

	for (i = 1; i < argc; i++) {
		if (read_fd) {
			read_fd = open_url(argv[i], O_RDONLY, 0);
		}
		if (read_fd >= 0) {
			do {
				count = read(read_fd, buf, sizeof(buf));
				if (count > 0) {
					write(write_fd, buf, count);
				}
			} while (count > 0);
		} else {
			fprintf(stderr, "Failed to open URL %s\n", argv[i]);
		}
		close(read_fd);
		if (write_fd != 1) {
			close(write_fd);
			write_fd = 1;
		}
	}
}


char *
param(char *string)
{
	return 0;
}
