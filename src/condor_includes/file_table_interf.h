#ifndef _FILE_TABLE_INTERF_H
#define _FILE_TABLE_INTERF_H

#if defined(__cplusplus)
extern "C" {
#endif

void DumpOpenFds();
void SaveFileState();
void RestoreFileState();
int MapFd( int );
void Set_CWD( const char *working_dir );

#if defined(__cplusplus)
}
#endif

#endif
