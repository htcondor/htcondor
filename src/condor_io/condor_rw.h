#ifndef __CONDOR_RW_H__
#define __CONDOR_RW_H__

int condor_read(int fd, char *buf, int sz, int timeout);
int condor_write(int fd, char *buf, int sz, int timeout);

#endif
