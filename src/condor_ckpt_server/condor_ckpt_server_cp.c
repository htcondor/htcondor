#include <stdio.h>
#include <fcntl.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "server_interface.h"

int copy_file_to_ckpt_server(char *);

main(argc, argv)
int		argc;
char	*argv[];
{
	int		i;
	int		server_num;
	int		do_remove = 0;

	config( argv[0], 0);

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'r') {
				do_remove = 1;
			} else {
				server_num = atoi(&(argv[i][1]));
				set_ckpt_server_number(server_num);
			}
		} else {
			if (do_remove) {
				remove_file_from_ckpt_server(argv[i]);
				do_remove = 0;
			} else {
				copy_file_to_ckpt_server(argv[i]);
			}
		}
	}
}


/*
  Determine the size of the file referenced by the given descriptor.
*/
int
file_size( int fd )
{
	int		cur_pos;
	int		answer;

		/* find out where we are in the file */
	if( (cur_pos=lseek(fd,0,1)) < 0 ) {
		return -1;
	}

		/* determine the file's size */
	if( (answer=lseek(fd,0,2)) < 0 ) {
		return -1;
	}

		/* seek back to where we were in the file */
	if( lseek(fd,cur_pos,0) < 0 ) {
		return -1;
	}

	return answer;
}


copy_file_to_ckpt_server(char *fname)
{
	int		local_fd;
	char	*my_name;
	struct sockaddr_in	sin;
	int		fsize;
	int		rval;
	int		sock_fd;

	local_fd = open(fname, O_RDONLY, 0);
	if (local_fd < 0) {
		return -1;
	}

	fsize = file_size( local_fd );

	my_name = getlogin();
	if (my_name == 0) {
		my_name = "pruyne";
	}

	rval = RequestStore(my_name, fname, fsize, &(sin.sin_addr),
						&(sin.sin_port));

	if (rval < 0) {
		close(local_fd);
		return -1;
	}
	sin.sin_family = AF_INET;

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	rval = connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin));
	if (rval < 0) {
		return -1;
	}

	stream_file_xfer( local_fd, sock_fd, fsize);
	close(local_fd);
	close(sock_fd);
}


remove_file_from_ckpt_server(char *fname)
{
	char	*my_name;

	my_name = getlogin();
	if (my_name == 0) {
		my_name = "pruyne";
	}

	RemoveRemoteFile(my_name, fname);
}


SetSyscalls() {}
