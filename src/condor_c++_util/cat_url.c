/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
