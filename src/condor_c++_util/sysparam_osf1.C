#include <iostream.h>
#include <nlist.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <string.h>
#include <sys/table.h>

#include "condor_debug.h"
#include "condor_sysparam.h"
#include "_condor_fix_resource.h"

// C definitions
static char *_FileName_ = __FILE__;
extern "C"
{
	int set_root_euid();
	int set_condor_euid();
}
class LoadVector {
public:
	int		Update();
    float   Short()     { return short_avg; }
    float   Medium()    { return medium_avg; }
    float   Long()      { return long_avg; }
private:
    float   short_avg;
    float   medium_avg;
    float   long_avg;
};


Osf1::Osf1()
{
}

int
Osf1::readSysParam(const SysParamName sys, char*& buffer, int& size,SysType& t)
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

	case LoadAvg: {						// lookup_load_avg()
		LoadVector MyLoad;
		int temp = MyLoad.Update();
		if ( temp == -1 )
		{
			dprintf( D_ALWAYS, "LoadAvg failed from Kernel \n");
			set_condor_euid();
			return -1;
		}
		size = sizeof(float);     // we return the long average as of now
		t    =  Float;
		buffer = (char *)new float[1];
		*(float *)buffer = MyLoad.Long();
		break;
	}
	
	case SwapSpace:  					// calc_virt_memory()
		//
		// Try to determine the swap space available on our own machine.  
		// The answer is in kilobytes.
		// Don't know how to do this one.  We just return the soft 
		// limit on data space for any children of the calling process.
    	struct rlimit   lim;
    	if( getrlimit(RLIMIT_DATA,&lim) < 0 ) 
		{
        	dprintf( D_ALWAYS, "getrlimit() failed - errno = %d", errno );
            set_condor_euid();
        	return -1;
    	}
		size = sizeof(long);
		t    =  Long;
		buffer = (char *)new long[1];
		*(long *)buffer = lim.rlim_cur / 1024;
		break;

	case PhyMem:						//  calc_phys_memory()
		set_condor_euid();
		dprintf(D_FULLDEBUG,"Do not know how to calculate PhyMem\n");
		return -1;
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
int
LoadVector::Update()
{
	struct tbl_loadavg  avg;

    if( table(TBL_LOADAVG,0,(char *)&avg,1,sizeof(avg)) < 0 ) {
        dprintf( D_ALWAYS, "Can't get load average info from kernel\n" ) ;
        return -1;
    }

    if( avg.tl_lscale ) {
        short_avg = avg.tl_avenrun.l[0] / (float)avg.tl_lscale;
        medium_avg = avg.tl_avenrun.l[1] / (float)avg.tl_lscale;
        long_avg = avg.tl_avenrun.l[2] / (float)avg.tl_lscale;
    } else {
        short_avg = avg.tl_avenrun.d[0];
        medium_avg = avg.tl_avenrun.d[1];
        long_avg = avg.tl_avenrun.d[2];
    }
	return 1;
}

// Function to print out system parameter to log file
void
Osf1::disSysParam(const SysParamName sys, const char* buffer, 
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
            if ( size != sizeof(long) ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != Long ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New load = %f\n", *(long *)buffer);
            break;

		case SwapSpace:
            if ( size != sizeof(long) ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != Long ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New virt_mem = %i\n", *(long *)buffer);
			break;
			
		case PhyMem:
			dprintf(D_ALWAYS, "disSysParam : PhyMem not implemented\n");
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
/* Solaris specific change ..dhaval 6/27 */
solaris:: solaris() {}
int  solaris::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void solaris::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

SunOs:: SunOs() {}
int  SunOs::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void SunOs::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

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
