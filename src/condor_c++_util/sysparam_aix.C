#include <stdio.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <string.h>
#include <malloc.h>

#include "condor_debug.h"
#include "condor_sysparam.h"

// C definitions
static char *_FileName_ = __FILE__;
extern "C"
{
	int set_root_euid();
	int set_condor_euid();
	int	knlist();
}

class AixSpecific
{
public:
	AixSpecific();
	~AixSpecific();
	static struct nlist nl[];   // table for OS symbols
	int	kd;					// fd for kvm calls
} specific;

enum { X_AVENRUN =0 };

struct nlist AixSpecific::nl[] =
   {
	{ { "avenrun"}  },
   };

Aix::Aix()
{
	set_root_euid();
	
    // Open kmem for reading
    if( (specific.kd=open("/dev/kmem",O_RDONLY,0)) < 0 )
    {
        set_condor_euid();
        EXCEPT( "open(/dev/kmem,O_RDONLY,0)" );
    }
    // Get the kernel's name list
    if( knlist(specific.nl,1,sizeof(struct nlist)) < 0 ) 
	{
        set_condor_euid();
        EXCEPT( "Can't get kernel name list" );
	}
	set_condor_euid();
}

int
Aix::readSysParam(const SysParamName sys, char*& buffer, int& size,SysType& t)
{
	set_root_euid();

	switch ( sys )
	{
	case Arch:                            // get_arch()
		struct utsname buf;
		if( uname(&buf) < 0 )  return -1;
		size =  strlen(buf.machine) + 1;
		t    =  String;
		buffer = new char[size];
		memcpy(buffer, buf.machine, size);
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
		int avenrun[3];
		off_t addr = specific.nl[X_AVENRUN].n_value;
        if( lseek(specific.kd,addr,0) != addr ) 
        {
            dprintf( D_ALWAYS, "Can't seek to addr 0x%x in /dev/kmem\n", addr );
            EXCEPT( "lseek in LoadAvg" );
        }
        if( read(specific.kd,(void *)avenrun,sizeof (avenrun)) !=sizeof (avenrun))
        {
            EXCEPT( "read in LoadAvg" );
        }
        size = sizeof(float);
        t    =  Float;
        buffer = (char *)new  float[1];
        *(float *)buffer = ((float)avenrun[0])/ 65536.0;
		break;
#if 0	
	case SwapSpace:  					// calc_virt_memory()
		struct anoninfo a_info;
		if (kvm_read (specific.kd, specific.nl[X_ANONINFO].n_value, 
						&a_info, sizeof (a_info)) != sizeof(a_info))
		{
                kvm_close( specific.kd );
                specific.kd = NULL;
                set_condor_euid();
                dprintf (D_ALWAYS, "kvm_read error.\n");
                return (-1);
        }
		size = sizeof(int);
		t    =  Integer;
		buffer = (char *)new int[1];
		int page_to_k = getpagesize() / 1024;
		*(int *)buffer = (int)(a_info.ani_max - a_info.ani_resv)* page_to_k;
		break;

	case PhyMem:						//  calc_phys_memory()
		unsigned int	phy;
		if (kvm_read (specific.kd, SunOsSpecific::nl[X_PHYMEM].n_value, 
								&phy, sizeof (phy)) != sizeof(phy))
        {
                kvm_close( specific.kd );
                specific.kd = NULL;
                set_condor_euid();
                dprintf (D_ALWAYS, "kvm_read error.\n");
                return (-1);
        }
		phy *= getpagesize();
        phy += 300000;      /* add about 300K to offset for RAM that the
                   Sun PROM program gobbles at boot time (at
                   least I think that's where its going).  If
                   we fail to do this the integer divide we do
                   next will incorrectly round down one meg. */

        phy /= 1048576;     /* convert from bytes to megabytes */
		size = sizeof(int);
		t    =  Integer;
		buffer = (char *)new int[1];
		*(int *)buffer = phy ;
		break;

	case CpuType:
		dprintf(D_ALWAYS,"Cpu Type not implemented in this platform\n");
		return -1;
#endif
	default:
		dprintf(D_ALWAYS, "readSysParam: default\n");
		return -1;
	}
	set_condor_euid();
	return 1;
}

// Function to print out system parameter to log file
void
SunOs::disSysParam(const SysParamName sys, const char* buffer, 
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

Ultrix:: Ultrix() {}
int  Ultrix::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Ultrix::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Hpux:: Hpux() {}
int  Hpux::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Hpux::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Linux:: Linux() {}
int  Linux::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Linux::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}


