#include <iostream.h>
#include <kvm.h>
#include <nlist.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <vm/anon.h>
#include <sys/utsname.h>
#include <string.h>

#include "condor_debug.h"
#include "condor_sysparam.h"

// C definitions
static char *_FileName_ = __FILE__;
extern "C"
{
	int set_root_euid();
	int set_condor_euid();
	int getpagesize();
}

class SunOsSpecific
{
public:
	static struct nlist nl[];   // table for OS symbols
	kvm_t*	kd;					// fd for kvm calls
} specific;

enum { X_AVENRUN =0, X_ANONINFO,X_PHYMEM};	
struct nlist SunOsSpecific::nl[] =
    {
        { "_avenrun" },
        { "_anoninfo" },
        { "_physmem" },
        { NULL }
    };


SunOs::SunOs()
{
	set_root_euid();
	
	// initialize kvm
	specific.kd = kvm_open (NULL, NULL, NULL, O_RDONLY, "sysparam_sun.C");
	if ( specific.kd ==  NULL) 
	{
    	set_condor_euid();
    	dprintf (D_ALWAYS, "Open failure on kernel. First call\n");
    	EXCEPT("kvm_open");
    }
    if (( kvm_nlist (specific.kd, specific.nl)) != 0)  // get kernel namelist
	{
    	(void)kvm_close( specific.kd );
    	specific.kd = (kvm_t *)NULL;
    	set_condor_euid();
    	dprintf (D_ALWAYS, "kvm_nlist failed. First call.\n");
    	EXCEPT("kvm_nlist");
    }
	set_condor_euid();
}

int
SunOs::readSysParam(const SysParamName sys, char*& buffer, int& size,SysType& t)
{
	set_root_euid();

	switch ( sys )
	{
	case Arch:                            // get_arch()
		{
		struct utsname buf;
		if( uname(&buf) < 0 )  return -1;
		size =  strlen(buf.machine) + 1;
		t    =  String;
		buffer = new char[size];
		memcpy(buffer, buf.machine, size);
		}
		break;	

	case OpSys:							// get_op_sys()
		{
        struct utsname buf1;
        if( uname(&buf1) < 0 )  return -1;
		size = strlen(buf1.sysname) + strlen(buf1.release) +1;
		t    =  String;
		buffer = new char[size];
		strcpy( buffer, buf1.sysname );
		strcat( buffer, buf1.release );
		}
		break;

	case LoadAvg:						// lookup_load_avg()
		{
		long avenrun[3];
        if (kvm_read (specific.kd, specific.nl[X_AVENRUN].n_value, 
						avenrun, sizeof (avenrun)) != sizeof(avenrun))
        {
                kvm_close( specific.kd );
                specific.kd = NULL;
                set_condor_euid();
                dprintf (D_ALWAYS, "kvm_read error.\n");
        		return -1;
        }
		size = sizeof(float);
		t    =  Float;
		buffer = (char *)new  float[1];
		*(float *)buffer = ((float)avenrun[0])/FSCALE;
		}
		break;
	
	case SwapSpace:  					// calc_virt_memory()
		{
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
		}
		break;

	case PhyMem:						//  calc_phys_memory()
		{
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
		}
		break;

	case CpuType:
		dprintf(D_ALWAYS,"Cpu Type not implemented in this platform\n");
		return -1;

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


Aix:: Aix() {}
int  Aix::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Aix::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Hpux:: Hpux() {}
int  Hpux::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Hpux::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Linux:: Linux() {}
int  Linux::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Linux::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}


