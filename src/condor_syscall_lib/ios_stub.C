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

 

/* The client file for I/O server
   Written by : Abhinav Gupta
                S.Chandrasekar
*/

#include "condor_common.h"
#include "condor_debug.h"
#include "ios_common.h"
#include "ios_stub.h"
#include "shadow.h"
#include "../condor_ckpt/file_state.h"
#include "condor_file_info.h"
#include "syscall_numbers.h"
#include "condor_sys.h"

extern OpenFileTable   *FileTab; 


/* packet format used for sending/receiving messages from shadow:
     The numbers in brackets indicate the number of bytes.. 
 a)  sending:
      1) OPEN_REQ
          Packet_type (1), Filename_len (4), file_name(Filename_len)
      2) SERVER_DOWN
          Packet_type(1), Filename_len (4), file_name(Filename_len), 
          Port (4), Servername_len (4), Servername (Servername_len)
          (Port and servername correspond to the IO server that has 
           gone down)
 b)  receiving response from shadow:
      1) OPEN_REQ 
          Packet_type (1), Servername_len (4), Servername (Servername_len)
      2) SERVER_DOWN
          Same as above
     
*/
  
extern "C"
int ioserver_open(const char *path, int oflag, mode_t mode)
{
 int i; 
 OpenMsg_t open_req(path, oflag, mode);
 char *buf;
 int len;
 int fd;
 struct in_addr *SRVR_ADDR;
 struct hostent *address;
 int sockfd;
 struct sockaddr_in srvr_IP;
 int temp_int;
 char server_host[30];
 int server_port = 0;
 
 // call shadow to get SERVER_HOST and SERVER_PORT
 // right now, this is taken from the header file..

 int req_type = OPEN_REQ ;
 
 bzero(server_host, sizeof(server_host));
 
 dprintf(D_ALWAYS, "calling get IO server \n");
 
 int status = REMOTE_syscall(CONDOR_get_IOServerAddr, &req_type, path, server_host , &server_port);

 dprintf(D_ALWAYS,"server %s, %d\n",server_host, server_port);
 dprintf(D_ALWAYS,"status %d\n",status);
   
   int local_sockfd ;
 while (status >= 0)
   {
     // if ((address = gethostbyname(SERVER_HOST))==NULL) 
     dprintf(D_ALWAYS,"in while loop\n");
 
     if ((address = gethostbyname(server_host))==NULL) 
       {
	 dprintf(D_ALWAYS,"invalid server name\n");
	 return -1;
       }

     dprintf(D_ALWAYS,"got IP address of server\n");

     
     SRVR_ADDR = (struct in_addr *)(*(address->h_addr_list));
     
     
     if (SRVR_ADDR == NULL)
       { 
	 dprintf(D_ALWAYS,"invalid server name");
	 return -1;
       } 
     
     dprintf(D_ALWAYS,"valid server name");

     
     /* Initialise the struct sockaddr_in for server */
     bzero((char *)&srvr_IP,sizeof(srvr_IP)); 
     srvr_IP.sin_family = AF_INET;
     //  memcpy( (void *)&srvr_IP.sin_addr, address->h_addr, address->h_length);
     srvr_IP.sin_addr.s_addr = SRVR_ADDR->s_addr;
     srvr_IP.sin_port = htons(server_port);
	FileTab->Display();
     int scm =SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
     if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
       {
	 dprintf(D_ALWAYS,"sockfd to server %d\n",sockfd);
	 SetSyscalls(scm); 
	 return -1;
       }  
     SetSyscalls(scm); 
     
		 /* bind to the right local interface */
	 _condor_local_bind( sockfd );

     /* Connect to server */
     if (connect(sockfd, (struct sockaddr *)&srvr_IP, sizeof(srvr_IP)) < 0)
       {
	 dprintf(D_ALWAYS,"unable to connect\n");
	 req_type = SERVER_DOWN ;
	 status = REMOTE_syscall(CONDOR_get_IOServerAddr, &req_type, path, server_host, &server_port);
	 dprintf(D_ALWAYS,"server %s, %d\n",server_host, server_port);
       }
     else
       {
	 FileTab->Display();
	 local_sockfd = FileTab->DoOpen("/IO_Socket", 0, sockfd, FALSE);
	 
	 FileTab->Display();
	 dprintf(D_ALWAYS,"sockfd to server %d (%d)\n",sockfd, local_sockfd);
	 int i0, i1, i2;
	 
	 dprintf(D_ALWAYS,"connected to IOserver\n");
	 
	 len = sizeof(srvr_IP);
	 i0 = open_req.Send(local_sockfd);
	 
	 dprintf(D_ALWAYS,"open..bytes sent %d\n", i0);
	 
	 if (i0 > 0)
	   i0 = read(local_sockfd,(char *)&temp_int, sizeof(int));
	 
	 
	 if ( i0 > 0 )
	   {
	     dprintf(D_ALWAYS,"Received open reply of length %d\n",ntohl(temp_int));
	     buf = new char [ntohl(temp_int)];
	     i0 = read(local_sockfd,buf,ntohl(temp_int)); 
	   }
	 
	 if (i0 <= 0)
	   { 
	     /////////////////////////////////////////////////////////////////////////////
	       FileTab->DoClose(local_sockfd);
	       
	       /*	       file[local_sockfd].open = FALSE;
			       delete [] file[local_sockfd].pathname;
			       file[local_sockfd].pathname = NULL; */
	       ////////////////////////////////////////////////////////////////////////////
	       
	       dprintf (D_ALWAYS, "unable to receive data");
	       req_type = SERVER_DOWN;
	       status = REMOTE_syscall(CONDOR_get_IOServerAddr, &req_type, path, server_host, &server_port);
	       dprintf(D_ALWAYS,"server %s, %d\n",server_host, server_port);
	     }
	 else break;
       }
   }
 
 
 
 if (status < 0)
   return -1;
 
 switch (*buf)
   {
   case OPEN:
     memcpy(&fd,buf+1,sizeof(int));
     free(buf);
     if (fd >=0)
       {
	 int local_fd =  FileTab->DoOpen(path, oflag, fd, FALSE, IS_IOSERVER);
	 dprintf(D_ALWAYS, "After Doopen local_fd = %d",local_fd);
	 FileTab->setServerName(local_fd,server_host);
	 FileTab->setServerPort(local_fd,server_port);
	 FileTab->setOffset(local_fd,0);
	 FileTab->setSockFd(local_fd,local_sockfd);
	 FileTab->setMode(local_fd, mode);
	 FileTab->setFlag(local_fd, oflag);
	 FileTab->Display();
	 return local_fd;
       }
     else
       return fd;
   default:
     dprintf(D_ALWAYS,"problems in return value of open\n");
     free(buf);
     return -1;     
   }
}

extern "C"
int ioserver_close(int filedes)
{
  int i; 
  CloseMsg_t close_req(FileTab->getRemoteFd(filedes));
  char *buf;
  int len;
  int fd;
  int sockfd;
  int temp_int;
  
  dprintf(D_ALWAYS,"ioserver_close called with fd = %d\n", filedes);

  sockfd = FileTab->getSockFd(filedes);
  i = close_req.Send(sockfd);
  
  if (i > 0)
    i = read(sockfd,(char *)&temp_int,sizeof(int));
  if ( i > 0 )
    {
      buf = new char [ntohl(temp_int)];
      i = read(sockfd,buf,ntohl(temp_int));
    }
  
  if (i <= 0)
    {
      dprintf (D_ALWAYS, "unable to receive data while closing\n");
      FileTab->DoClose(FileTab->getSockFd(filedes));
      FileTab->DoClose(filedes);
      return 0;
    }
  
  switch (*buf)
    {
    case CLOSE:
      memcpy(&fd,buf+1,sizeof(int));
      if (fd != FileTab->getRemoteFd(filedes))
	{
	  dprintf(D_ALWAYS,"incorrect fd in close response\n");
	  free(buf);
	  return -1;
	}
      memcpy(&fd,buf+1+sizeof(int),sizeof(int)); // the return value for close
      free(buf);
      close (sockfd);
      FileTab->DoClose(filedes);
      return fd;
    default:
      dprintf(D_ALWAYS,"problems in return value of close\n");
      free(buf);
      return -1;      
    }
}

// reopens the file corresponding to filedes with offset = 0
int reopen(int filedes)
{
  char *path = FileTab->getFileName(filedes);
  mode_t mode = FileTab->getMode(filedes);
  int oflag = FileTab->getFlag(filedes);
  int i; 
  OpenMsg_t open_req(path, oflag, mode);
  char *buf;
  int len;
  int fd;
  struct in_addr *SRVR_ADDR;
  struct hostent *address;
  int sockfd;
  struct sockaddr_in srvr_IP;
  int temp_int;
  char server_host[30];
  int server_port;
  char *temp;
  int status;
  int local_sockfd;
  

  dprintf(D_ALWAYS,"\n in reopen %d. filename = %s...................\n",filedes, path);

/////////////////////////////////////////////////////////////////////////////
  local_sockfd = FileTab->getSockFd(filedes);
  FileTab->DoClose(local_sockfd);
  
/*  file[local_sockfd].open = FALSE;
  delete [] file[local_sockfd].pathname;
  file[local_sockfd].pathname = NULL; */
/////////////////////////////////////////////////////////////////////////////

  // call shadow to get SERVER_HOST and SERVER_PORT
  // right now, this is taken from the header file..
  temp = FileTab->getServerName(filedes);
  strcpy(server_host,temp);
  free(temp);
  
  int req_type = SERVER_DOWN ;
  server_port = FileTab->getServerPort(filedes);
  status = REMOTE_syscall(CONDOR_get_IOServerAddr, &req_type, path, server_host , &server_port);

  dprintf(D_ALWAYS,"reopen.. server %s, %d\n",server_host, server_port);
  
  while (status >= 0)
    {
      dprintf(D_ALWAYS,"reopen ..in while loop \n");
      
      if ((address = gethostbyname(server_host))==NULL) 
	{ 
	  dprintf(D_ALWAYS, "invalid server name\n");
	  return -1;
	}
      dprintf(D_ALWAYS,"reopen ..get host by name successful \n");
      
      
      SRVR_ADDR = (struct in_addr *)(*(address->h_addr_list));
      
      
      if (SRVR_ADDR == NULL)
	{ 
	  dprintf(D_ALWAYS, "invalid server name");
	  return -1;
	} 
      
      dprintf(D_ALWAYS,"valid server name");

      /* Initialise the struct sockaddr_in for server */
      bzero((char *)&srvr_IP,sizeof(srvr_IP)); 
      srvr_IP.sin_family = AF_INET;

      srvr_IP.sin_addr.s_addr = SRVR_ADDR->s_addr;
      srvr_IP.sin_port = htons(server_port);
      
      FileTab->Display();
      int scm =SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );      
      if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{ 
	  dprintf(D_ALWAYS, "reopen .. socket failed \n");
	  SetSyscalls(scm); 
	  return -1;
	}  

      SetSyscalls(scm); 
      
      dprintf(D_ALWAYS, "reopen .. socket successful \n");

		 /* bind to the right local interface */
	 _condor_local_bind( sockfd );
      
      /* Connect to server */
      if (connect(sockfd, (struct sockaddr *)&srvr_IP, sizeof(srvr_IP)) < 0)
	{
	  dprintf(D_ALWAYS, "reopen.. unable to connect \n");
	  req_type = SERVER_DOWN ;
	  status = REMOTE_syscall(CONDOR_get_IOServerAddr, &req_type, path, server_host, &server_port);
	  dprintf(D_ALWAYS,"server %s, %d\n",server_host, server_port);
	}
      else
	{
	  FileTab->Display();
	  local_sockfd = FileTab->DoOpen("/IO_Socket", 0, sockfd, FALSE);
	  
	  FileTab->Display();
	  dprintf(D_ALWAYS,"sockfd to server %d (%d)\n",sockfd, local_sockfd);

	  dprintf(D_ALWAYS, "reopen.. connect done \n");

	  len = sizeof(srvr_IP);
	  i = open_req.Send(local_sockfd);
	  if (i > 0)
	    i = read(local_sockfd,(char *)&temp_int, sizeof(int));
	  dprintf(D_ALWAYS,"reopen.. bytes expected %d\n",temp_int);
	  
	  if ( i > 0 )
	    {
	      buf = new char [ntohl(temp_int)];
	      i = read(local_sockfd,buf,ntohl(temp_int)); 
	      dprintf(D_ALWAYS,"reopen.. bytes recieved %d\n",i);
	    }

	  if (i <= 0)
	    { 
//////////////////////////////////////////////////////////////////////////////
	      FileTab->DoClose(local_sockfd);

/*	      file[local_sockfd].open = FALSE;
	      delete [] file[local_sockfd].pathname;
	      file[local_sockfd].pathname = NULL; */
//////////////////////////////////////////////////////////////////////////////

	      dprintf(D_ALWAYS,"unable to receive data");
	      req_type = SERVER_DOWN ;
	      status = REMOTE_syscall(CONDOR_get_IOServerAddr, &req_type, path,
				      server_host, &server_port);
	      dprintf(D_ALWAYS,"server %s, %d\n",server_host, server_port);
	    }
	  else break;
	}
    }
  
  
  if (status < 0)
    return -1;
  
  
  switch (*buf)
    {
    case OPEN:
      memcpy(&fd,buf+1,sizeof(int));
      free(buf);
      if (fd >=0)
	{
	  int local_fd = filedes;
     	  // FileTab->DoOpen(path, oflag, fd, FALSE, IS_IOSERVER);
	  // avoid mem leak in fd_table here..
	  FileTab->setRemoteFd(local_fd, fd);
	  FileTab->setServerName(local_fd,server_host);
	  FileTab->setServerPort(local_fd,server_port);
	  FileTab->setOffset(local_fd,0);
	  FileTab->setSockFd(local_fd,local_sockfd);
	  FileTab->setMode(local_fd, mode);
	  FileTab->setFlag(local_fd, oflag);
	  FileTab->Display();
	  return local_fd;
	}
      else
	return fd;
    default:
      dprintf(D_ALWAYS,"problems in return value of open\n");
      free(buf);
      return -1;     
    }
}  


off_t ioserver_lseek(int filedes, off_t offset, int whence)
{
  int i; 
  LseekMsg_t lseek_req(FileTab->getRemoteFd(filedes),offset,whence);
  char *buf;
  int fd;
  int sockfd;
  int temp_int;
 
  sockfd = FileTab->getSockFd(filedes);
  dprintf(D_ALWAYS, "In ioserver_lseek fd : %d sock : %d off %d whence %d\n",filedes,sockfd,offset,whence);
  i = lseek_req.Send(sockfd);
  if (i > 0)
    i = read(sockfd,(char *)&temp_int,sizeof(int));
  if ( i > 0 ) 
    {
      buf = new char [ntohl(temp_int)];
      i = read(sockfd,buf,ntohl(temp_int));
    }
  
  if (i <= 0)
   { 
     int fd;
     
     dprintf (D_ALWAYS, "unable to receive data in seek..");

     fd = reopen(filedes);
     if (fd == filedes)
       return ioserver_lseek(fd, offset, whence);
     else return -1;
   }
    
  switch (*buf)
    {
    case LSEEK:
      memcpy(&fd,buf+1,sizeof(int));
      if (fd != FileTab->getRemoteFd(filedes))
	{
	 dprintf(D_ALWAYS,"incorrect fd in close response\n");
	  free(buf);
	  return -1;
	}
      memcpy(&fd,buf+1+sizeof(int),sizeof(int)); // the return value
      FileTab->setOffset(filedes, fd);
      free(buf);
      return fd;
    default:
      dprintf(D_ALWAYS,"problems in return value of lseek\n");
      free(buf);
      return -1;
    }
}

// assumend the size <= DEFLT_MSG size
ssize_t ioserver_read(int filedes, char *ret_buf, unsigned int size)
{
  int i;
  int remote_fd = FileTab->getRemoteFd(filedes);
  ReadMsg_t read_req(remote_fd,size);
  char *buf;
  int fd;
  int sockfd;
  int temp_int;
  
  dprintf(D_ALWAYS,"In IOserver read.. file = %s\n",FileTab->getFileName(filedes));

  sockfd = FileTab->getSockFd(filedes);
  dprintf(D_ALWAYS, "In ioserver_read fd : %d sock : %d\n",filedes,sockfd);
  i = read_req.Send(sockfd);
  if (i > 0) 
      i = read(sockfd,(char *)&temp_int,sizeof(int));
     
  
  if (i > 0) 
     {
       buf = new char [ntohl(temp_int)];
       i = read(sockfd,buf,ntohl(temp_int));
     }
  dprintf(D_ALWAYS,"bytes read = %d\n",i);
  
  if (i <= 0) 
    { 
     int fd;
     int offset = FileTab->getOffset(filedes);
     
     dprintf (D_ALWAYS, "unable to receive data in read..");

     fd = reopen(filedes);
     if (fd == filedes)
       {
	 ioserver_lseek(fd, offset, 0);
	 return ioserver_read(fd, ret_buf, size);
       }
     else return -1;
    }
  
  switch (*buf)
    {
    case READ:
      memcpy(&fd,buf+1,sizeof(int));
      if (fd != remote_fd)
	{
	  dprintf(D_ALWAYS,"incorrect fd in close response\n");
	  free(buf);
	  return -1;
	}
      memcpy(&i,buf+1+sizeof(int),sizeof(int));
      dprintf(D_ALWAYS,"ret value = %d.. curr offset = %d\n",i,FileTab->getOffset(filedes));
      if (i > 0)
	FileTab->setOffset(filedes, FileTab->getOffset(filedes)+i);
      
      memcpy(ret_buf,buf+1+2*sizeof(int),i); // the return value
      free(buf);
      return (i);
    default:
      dprintf(D_ALWAYS,"problems in return value of lseek\n");
      free(buf);
      return -1;
    }
}

ssize_t ioserver_write(int filedes, const char *buffer, unsigned int size)
{

  int i;
  int remote_fd = FileTab->getRemoteFd(filedes);
  char *buf;
  WriteMsg_t write_req(remote_fd,buffer,size);
  int len;
  int fd;
  int sockfd, temp_int;
  char *path = FileTab->getFileName(filedes);

  dprintf(D_ALWAYS,"In IOserver write.. file = %s, filedes = %d\n",path, filedes);
  dprintf(D_ALWAYS,"size = %d\n",size);

/*  for (i=0;i<size;i++)
    dprintf(D_ALWAYS,"%c",buffer[i]);
  dprintf(D_ALWAYS,"\n");
*/

  sockfd = FileTab->getSockFd(filedes);
  i = write_req.Send(sockfd);
  if (i > 0)
    i = read(sockfd,(char *)&temp_int,sizeof(int));
  if ( i > 0 )
    {
      buf = new char [ntohl(temp_int)];
      i = read(sockfd,buf,ntohl(temp_int));
    }
 
  if (i <= 0)
    { 
     int fd;
     int offset = FileTab->getOffset(filedes);
     
     dprintf (D_ALWAYS, "unable to receive data in write..");

     fd = reopen(filedes);

     if (fd == filedes)
       {
	 ioserver_lseek(fd, offset, 0);
	 return ioserver_write(fd, buffer, size);
       }
     else return -1;
    }
  
  switch (*buf)
    {
    case WRITE:
      memcpy(&fd,buf+1,sizeof(int));
      if (fd != remote_fd)
	{
	  dprintf(D_ALWAYS,"incorrect fd in close response\n");
	  free(buf);
	  return -1;
	}
      memcpy(&fd,buf+1+sizeof(int),sizeof(int)); 
      // the return value (no. of bytes written)
      
      if (fd>0)
	FileTab->setOffset(filedes, FileTab->getOffset(filedes)+fd);

      free(buf);
      return fd;
    default:
      dprintf(D_ALWAYS,"problems in return value of lseek\n");
      free(buf);
      return -1;
    }
}    

int ioserver_fstat(int filedes, struct stat * stat_buf)
{
  int i; 
  FstatMsg_t fstat_req(FileTab->getRemoteFd(filedes));
  
  char *buf;
  int fd;
  int sockfd;
  int temp_int;
  void *x;
  
  sockfd = FileTab->getSockFd(filedes);
  dprintf(D_ALWAYS, "In ioserver_fstat fd : %d sock : %d\n",filedes,sockfd);
  i = fstat_req.Send(sockfd);
  if (i > 0)
    i = read(sockfd,(char *)&temp_int,sizeof(int));
  if ( i > 0 ) 
    {
      dprintf(D_ALWAYS,"fstat ------ bytes to recv %d\n",ntohl(temp_int));
      buf = new char [ntohl(temp_int)];
      i = read(sockfd,buf,ntohl(temp_int));
      dprintf(D_ALWAYS,"fstat ------ bytes received %d\n",i);
    }
  
  if (i <= 0)
   { 
     int fd;
     int offset = FileTab->getOffset(filedes);
     
     dprintf (D_ALWAYS,"unable to receive data in stat.. for file %s\n", 
	      FileTab->getFileName(filedes));
     dprintf (D_ALWAYS,"trying again\n");
 
     fd = reopen(filedes);

     if (fd == filedes)
       {
	 ioserver_lseek(fd, offset, 0);
	 return ioserver_fstat(fd, stat_buf);
       }
     else return -1;
   }
    
  switch (*buf)
    {
    case FSTAT:
      memcpy(&fd,buf+1,sizeof(int));
      if (fd != FileTab->getRemoteFd(filedes))
	{
	  dprintf(D_ALWAYS,"incorrect fd in close response\n");
	  free(buf);
	  return -1;
	}
#if 0      
      mode_t   st_mode;     /* File mode (see mknod(2)) */
      ino_t    st_ino;      /* Inode number */
      dev_t    st_dev;      /* ID of device containing */
      /* a directory entry for this file */
      dev_t    st_rdev;     /* ID of device */
      /* This entry is defined only for */
      /* char special or block special files */
      nlink_t  st_nlink;    /* Number of links */
      uid_t    st_uid;      /* User ID of the file's owner */
      gid_t    st_gid;      /* Group ID of the file's group */
      off_t    st_size;     /* File size in bytes */
      time_t   st_atime;    /* Time of last access */
      time_t   st_mtime;    /* Time of last data modification */
      time_t   st_ctime;    /* Time of last file status change */
      /* Times measured in seconds since */
      /* 00:00:00 UTC, Jan. 1, 1970 */
      long     st_blksize;  /* Preferred I/O block size */
      long     st_blocks;   /* Number of 512 byte blocks allocated*/
#endif
      
      x = buf+1+sizeof(int);
      
      memcpy(&stat_buf->st_mode,x,sizeof(mode_t));
      memcpy(&stat_buf->st_ino,x+=sizeof(mode_t),sizeof(ino_t));
      memcpy(&stat_buf->st_dev,x+=sizeof(ino_t),sizeof(dev_t));
      memcpy(&stat_buf->st_rdev,x+=sizeof(dev_t),sizeof(dev_t));
      memcpy(&stat_buf->st_nlink,x+=sizeof(dev_t),sizeof(nlink_t));
      memcpy(&stat_buf->st_uid,x+=sizeof(nlink_t),sizeof(uid_t));
      memcpy(&stat_buf->st_gid,x+=sizeof(uid_t),sizeof(gid_t));
      memcpy(&stat_buf->st_size,x+=sizeof(gid_t),sizeof(off_t));
      memcpy(&stat_buf->st_atime,x+=sizeof(off_t),sizeof(time_t));
      memcpy(&stat_buf->st_mtime,x+=sizeof(time_t),sizeof(time_t));
      memcpy(&stat_buf->st_ctime,x+=sizeof(time_t),sizeof(time_t));
      memcpy(&stat_buf->st_blksize,x+=sizeof(time_t),sizeof(long));
      memcpy(&stat_buf->st_blocks,x+=sizeof(long),sizeof(long));
      // return value	
      memcpy(&fd,x+=sizeof(long),sizeof(int));
      
      free(buf);
      dprintf(D_ALWAYS,"Return value of fstat %d\n",fd);
      return fd;
    default:
      dprintf(D_ALWAYS,"problems in return value of lseek\n");
      free(buf);
      return -1;
    }
}

