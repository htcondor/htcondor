
#define _ALL_SOURCE

#include <stdio.h>
#include <filehdr.h>
#include <aouthdr.h>
#include <model.h>
#include <magic.h>
#include <nlist.h>
#include "condor_debug.h"
#include "_condor_fix_resource.h"
#include "types.h"
#include "proto.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

extern "C" {
#ifdef HPUX10
	int nlist( const char *FileName, struct nlist *N1 );
#else
	int nlist( char *FileName, struct nlist *N1 );
#endif
}

int magic_check( char *a_out )
{
	int exec_fd = -1;
	struct header exec_header;
	struct som_exec_auxhdr hpux_header;
	int nbytes;

	if ( (exec_fd=open(a_out,O_RDONLY)) < 0) {
		dprintf(D_ALWAYS,"error opening executeable file %s\n",a_out);
		return(-1);
	}

	nbytes = read(exec_fd, (char *)&exec_header, sizeof( exec_header) );
	if ( nbytes != sizeof( exec_header) ) {
		dprintf(D_ALWAYS,"read executeable main header error \n");
		close(exec_fd);
		return(-1);
	}

	close(exec_fd);

	if ( exec_header.a_magic != SHARE_MAGIC &&
	   	 exec_header.a_magic != EXEC_MAGIC ) {
		dprintf(D_ALWAYS,"EXECUTEABLE %s HAS BAD MAGIC NUMBER (0x%x)\n",a_out,exec_header.a_magic);
		return -1;
	}
/****
#define MYSYS    0x20B // this is for g++ installation bug : dhruba
	if ( exec_header.system_id != MYSYS ) {
		dprintf(D_ALWAYS,"EXECUTEABLE %s NOT COMPILED FOR THIS ARCHITECTURE: system_id = %d\n",a_out,exec_header.system_id);
		return -1;
	}
***********/

	return 0;
}

/*
Check to see that the checkpoint file is linked with the Condor
library by looking for the symbol "_START".
*/
int symbol_main_check( char *name )
{
	int status;
	struct nlist nl[2];

	nl[0].n_name = "_START";
	nl[1].n_name = "";

	status = nlist( name, nl);

	/* Return TRUE even if nlist reports an error because the executeable
	 * may have simply been stripped by the user */
	if ( status < 0 ) {
		dprintf(D_ALWAYS,"Error: nlist returns %d, errno=%d\n",status,errno);
		return 0;
	}

	if ( nl[0].n_type == 0 ) {
		dprintf(D_ALWAYS,"No symbol _START found in executeable %s\n",name);
		return -1;

	}

	return 0;
}

int
calc_hdr_blocks()
{
	return (sizeof(struct header) + sizeof(struct som_exec_auxhdr) + 1023) / 1024;
}


int 
calc_text_blocks( char *a_out )
{
	int exec_fd = -1;
	struct header exec_header;
	struct som_exec_auxhdr hpux_header;
	int nbytes;

	if ( (exec_fd=open(a_out,O_RDONLY)) < 0) {
		printf("error opening file\n");
		return(-1);
	}

	nbytes = read(exec_fd, (char *)&exec_header, sizeof( exec_header) );
	if ( nbytes != sizeof( exec_header) ) {
		dprintf(D_ALWAYS,"reading executeable %s main header error \n",a_out);
		close(exec_fd);
		return(-1);
	}

	nbytes = read(exec_fd, (char *)&hpux_header, sizeof( hpux_header) );
	if ( nbytes != sizeof( hpux_header) ) {
		dprintf(D_ALWAYS,"read executeable %s hpux header error \n",a_out);
		close(exec_fd);
		return(-1);
	}

	close(exec_fd);

	if ( hpux_header.som_auxhdr.type != HPUX_AUX_ID ) {
		dprintf(D_ALWAYS,"in executeable %s, hpux header does not immediately follow executeable header!\n",a_out);
		return(-1);
	}

	return hpux_header.exec_tsize / 1024;
}

