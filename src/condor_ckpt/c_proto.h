const int FI_RSC =          0; 		/* access via remote sys calls           */
const int FI_OPEN =         1<<0;	/* This file is open                     */
const int FI_DUP =          1<<1;	/* This file is a dup of 'fi_fdno'       */
const int FI_PREOPEN =      1<<2;	/* This file was opened previously       */
const int FI_NFS =          1<<3;	/* access via direct sys calls           */
const int FI_DIRECT = 		1<<3;
const int FI_WELL_KNOWN =   1<<4;	/* well know socket to the shadow        */

const int	SYS_RECORDED = 2;
const int	SYS_MAPPED = 2;

const int	SYS_UNRECORDED = 0;
const int	SYS_UNMAPPED = 0;


#if defined(__cplusplus)
extern "C" {
#endif

int PreOpen( int fd );
void DumpOpenFds();
void MarkFileClosed( int fd );
int MarkFileOpen( int fd, const char *path, int flags, int method );
int	SetSyscalls( int mode );
int	MapFd( int user_fd );
int DoDup( int fd );
int DoDup2( int orig_fd, int new_fd );

#if defined(__cplusplus)
}
#endif
