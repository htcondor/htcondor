#include <stdio.h>
#include "server_interface.h"

main()
{
	int		foo;
	struct in_addr	addr;
	u_short			port;
	int		sock;
	struct  sockaddr_in sin;
	int		i;
	int		len;

	foo = RequestStore("pruyne", "foo", 100, &addr, &port);
	sock = I_socket();
	sin.sin_port = port;
	sin.sin_addr = addr;
	sin.sin_family = AF_INET;
	foo = connect(sock, (struct sockaddr *) &sin, sizeof(sin));
	for (i = 0; i < 100; i++) {
		write(sock, &i, 1);
	}
	close(sock);
	
	foo = FileOnServer("pruyne", "foo");

	RenameRemoteFile("pruyne", "foo", "foo2");

	len = 0;
	foo = RequestRestore("pruyne", "foo2", &len, &addr, &port);

	sock = I_socket();
	sin.sin_port = port;
	sin.sin_addr = addr;
	sin.sin_family = AF_INET;
	foo = connect(sock, (struct sockaddr *) &sin, sizeof(sin));
	for (i = 0; i < len; i++) {
		read(sock, &foo, 1);
		printf("%d ", foo);
	}
	close(sock);
	printf("\n"); fflush(stdout);
}

SetSyscalls() {}
