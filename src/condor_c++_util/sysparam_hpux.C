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

#include <malloc.h>
#include <stdlib.h>

#include "condor_debug.h"
#include "condor_sysparam.h"
#include "_condor_fix_resource.h"

// C definitions
static char *_FileName_ = __FILE__;
extern "C"
{
	int set_root_euid();
	int set_condor_euid();
	int nlist();
	char*	param();
}

class HpuxSpecific
{
public:
	static struct nlist nl[];   // table for OS symbols
	int    kd;					// fd for kvm calls
} specific;

enum { X_AVENRUN =0, X_SWAPCNT,X_PHYMEM};	
struct nlist HpuxSpecific::nl[] =
    {
        { "avenrun" },
        { "swapspc_cnt" },
        { "physmem" },
        { NULL }
    };


Hpux::Hpux()
{
	set_root_euid();

    // Open kmem for reading 
    if( (specific.kd=open("/dev/kmem",O_RDONLY,0)) < 0 ) 
	{
        set_condor_euid();
        EXCEPT( "open(/dev/kmem,O_RDONLY,0)" );
    }
    // Get the kernel's name list 
	(void)nlist( "/hp-ux", specific.nl );
	
	set_condor_euid();
}

int
Hpux::readSysParam(const SysParamName sys, char*& buffer, int& size,SysType& t)
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
		double avenrun[3];
    	off_t addr = specific.nl[X_AVENRUN].n_value;
		if( lseek(specific.kd,addr,0) != addr ) 
		{
        	dprintf( D_ALWAYS, "Can't seek to addr 0x%x in /dev/kmem\n", addr );
        	EXCEPT( "lseek for LoadAvg" );
		}
    	if( read(specific.kd,(void *)avenrun,sizeof avenrun) !=sizeof avenrun ) 
		{
        	EXCEPT( "read for LoadAvg" );
    	}
		size = sizeof(float);
		t    =  Float;
		buffer = (char *)new  float[1];
		*(float *)buffer = ((float)avenrun[0]);
		break;

	case SwapSpace:  					// calc_virt_memory()
		int	freeswap;
    	addr = specific.nl[X_SWAPCNT].n_value;
		if( lseek(specific.kd,addr,0) != addr ) 
		{
        	dprintf( D_ALWAYS,"Can't seek to addr 0x%x in /dev/kmem\n", addr );
        	EXCEPT( "lseek for SwapSpace" );  
		}
    	if( read(specific.kd,(void*)&freeswap,sizeof (int)) !=sizeof (int) ) 
		{
        	EXCEPT( "read for SwapSpace" );
    	}
		size = sizeof(int);
		t    =  Integer;
		buffer = (char *)new  int[1];
		*(int *)buffer = 4 * freeswap;
		break;

	case PhyMem:						//  calc_phys_memory()
		int	physmem;
    	addr = specific.nl[X_PHYMEM].n_value;
		if( lseek(specific.kd,addr,0) != addr ) 
		{
        	dprintf( D_ALWAYS,"Can't seek to addr 0x%x in /dev/kmem\n", addr );
        	EXCEPT( "lseek for PhyMem" );
		}
    	if( read(specific.kd,(void*)&physmem,sizeof(int)) !=sizeof(int) ) 
		{
        	EXCEPT( "read for PhyMem" );
    	}
		physmem/=256; 		// convert to megabytes
		size = sizeof(int);
		t    =  Integer;
		buffer = (char *)new  int[1];
		*(int *)buffer = physmem;
		break;

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
Hpux::disSysParam(const SysParamName sys, const char* buffer, 
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


Ultrix:: Ultrix() {}
int  Ultrix::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Ultrix::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

