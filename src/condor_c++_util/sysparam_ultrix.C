#include <iostream.h>
#include <stdio.h>
#include <nlist.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <string.h>

#include <sys/fixpoint.h>
#include <malloc.h>
#include <stdlib.h>

#include "condor_debug.h"
#include "condor_sysparam.h"
#include "_condor_fix_resource.h"

// C definitions
static char *_FileName_ = __FILE__;
extern int	HasSigchldHandler;
extern "C"
{
	int set_root_euid();
	int set_condor_euid();
	int nlist();
	char* param();
}

class UltrixSpecific
{
public:
	static struct nlist nl[];   // table for OS symbols
	int    kd;					// fd for kvm calls
} specific;

enum { X_AVENRUN =0, X_ANONINFO,X_PHYMEM};	
struct nlist UltrixSpecific::nl[] =
    {
        { "_avenrun" },
        { "_anoninfo" },
        { "_physmem" },
        { NULL }
    };


Ultrix::Ultrix()
{
	set_root_euid();

    // Open kmem for reading 
    if( (specific.kd=open("/dev/kmem",O_RDONLY,0)) < 0 ) 
	{
        set_condor_euid();
        EXCEPT( "open(/dev/kmem,O_RDONLY,0)" );
    }
    // Get the kernel's name list 
	(void)nlist( "/vmunix", specific.nl );
	
	set_condor_euid();
}

int
Ultrix::readSysParam(const SysParamName sys, char*& buffer, int& size,SysType& t)
{
	buffer = NULL;
	size   = 0;
	t 	   = Other;
	set_root_euid();

	switch ( sys )
	{
	case Arch:                            // get_arch()
		char*	tmp;
		if ( (tmp = param("ARCH")) == NULL)
		{
            set_condor_euid();
			return -1;
		}
		size =  strlen(tmp) + 1;
		t    =  String;
		buffer = new char[size];
		memcpy(buffer, tmp, size);
		free ( tmp );
		break;	

	case OpSys:							// get_op_sys()
        struct utsname buf1;
        if( uname(&buf1) < 0 )  return -1;
		size = strlen(buf1.sysname) + strlen(buf1.release) +1;
		t    =  String;
		buffer = new char[size];
		strcpy( buffer, buf1.sysname );
		strcat( buffer, buf1.release );
		break;

	case LoadAvg:						// lookup_load_avg()
		fix avenrun[3];
    	off_t addr = specific.nl[X_AVENRUN].n_value;
		if( lseek(specific.kd,addr,0) != addr ) 
		{
        	dprintf( D_ALWAYS, "Can't seek to addr 0x%x in /dev/kmem\n", addr );
        	EXCEPT( "lseek" );
		}
    	if( read(specific.kd,(void *)avenrun,sizeof avenrun) !=sizeof avenrun ) 
		{
        	EXCEPT( "read" );
    	}
		size = sizeof(float);
		t    =  Float;
		buffer = (char *)new  float[1];
		*(float *)buffer = ((float)avenrun[0])/(1<<FBITS);
		break;

	case SwapSpace:  					// calc_virt_memory()
 		FILE    *fp;
    	char    buf[1024];
    	char    buf2[1024];
    	int     size1 = -1;
    	int     limit;
    	struct  rlimit lim;
    	int     read_error = 0;
    	int     configured, reserved;

 		if( (fp=popen("/etc/pstat -s","r")) == NULL ) 
		{
			dprintf(D_ALWAYS, "Unable to popen \n");
			set_condor_euid();
			return -1;
		}
    	(void)fgets( buf, sizeof(buf), fp );
    	(void)fgets( buf2, sizeof(buf2), fp );
    	configured = atoi( buf );
    	reserved = atoi( buf2 );
    	if( configured == 0 || reserved == 0 ) size1 = -1;
    		else size1 = configured - reserved;
//  
//   		Some programs which call this routine will have their own handler
//  		for SIGCHLD.  In this case, don't cll pclose() because it calls
// 			wait() and will interfere with the other handler.
// 			Fix #3 from U of Wisc.
//

		if ( HasSigchldHandler  ) fclose(fp);
			else  pclose(fp);

    	if( size1 < 0 ) 
		{
        	if( ferror(fp) ) dprintf( D_ALWAYS, "Error reading from pstat\n" );
        		else 
            	dprintf( D_ALWAYS, "Can't parse pstat line \"%s\"\n", buf );
			set_condor_euid();
        	return -1;
    	}
    	if( getrlimit(RLIMIT_DATA,&lim) < 0 ) 
		{
        	dprintf( D_ALWAYS, "Can't do getrlimit()\n" );
			set_condor_euid();
        	return -1;
    	}
    	limit = lim.rlim_max / 1024;

		size = sizeof(int);
		t    =  Integer;
		buffer = (char *)new int[1];
		if( limit < size1 ) {
        	dprintf( D_FULLDEBUG, "Returning %d\n", limit );
			*(int *)buffer = limit;
    	} else {
        	dprintf( D_FULLDEBUG, "Returning %d\n", size );
			*(int *)buffer = size1;
    	}
		break;

	case PhyMem:						//  calc_phys_memory()
		set_condor_euid();
		dprintf(D_FULLDEBUG,"PhyMem not implemented \n");
		return -1;

	case CpuType:
		set_condor_euid();
		dprintf(D_ALWAYS,"Cpu Type not implemented in this platform\n");
		return -1;

	default:
		set_condor_euid();
		dprintf(D_ALWAYS, "readSysParam: default\n");
		return -1;
	}
	set_condor_euid();
	return 1;
}

// Function to print out system parameter to log file
void
Ultrix::disSysParam(const SysParamName sys, const char* buffer, 
								const int size, const SysType type)
{
    if ( !buffer )
    {
        dprintf(D_ALWAYS, "display():Buffer pointer is null\n");
        return;
    }
    switch ( sys )
    {
		case Arch:
            if ( size  == 0 ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != String ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New Arch = %s\n", buffer);
            break;

		case OpSys:
            if ( size  == 0 ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != String ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New OpSys = %s\n", buffer);
            break;
			
        case LoadAvg:
            if ( size != sizeof(float) ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != Float ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New load = %f\n", *(float *)buffer);
            break;

		case SwapSpace:
            if ( size != sizeof(int) ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != Integer ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New virt_mem = %i\n", *(int *)buffer);
			break;
			
		case PhyMem:
            if ( size != sizeof(int) ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != Integer ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New phys_mem = %i\n", *(int *)buffer);
			break;

		case CpuType:
			dprintf(D_ALWAYS, "disSysParam : CpuType not implemented\n");
			break;

        default:
            dprintf(D_ALWAYS, "disSysParam : no match\n");
            break;
    }
}

// dummy stubs
Osf1:: Osf1() {}
int  Osf1::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Osf1::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

SunOs:: SunOs() {}
int  SunOs::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void SunOs::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}


Aix:: Aix() {}
int  Aix::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Aix::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Hpux:: Hpux() {}
int  Hpux::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Hpux::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Linux:: Linux() {}
int  Linux::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Linux::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}
