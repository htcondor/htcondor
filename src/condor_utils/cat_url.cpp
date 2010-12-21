/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
		write_fd = open_url(argv[2], O_WRONLY);
		argv++;
		argc--;
	}

	for (i = 1; i < argc; i++) {
		if (read_fd) {
			read_fd = open_url(argv[i], O_RDONLY);
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
