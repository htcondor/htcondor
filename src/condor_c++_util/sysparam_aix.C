#include <iostream.h>
#include <nlist.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <string.h>
#include <sys/sysinfo.h>

#include "condor_debug.h"
#include "except.h"
#include "condor_sysparam.h"

// C definitions
static char *_FileName_ = __FILE__;
extern "C"
{
	int set_root_euid();
	int set_condor_euid();
	int	nlist();
}

class AixSpecific
{
public:
	AixSpecific() { n_polls = 0; };
	static struct nlist nl[];   // table for OS symbols
	int*	kd;					// fd for kvm calls
	const	int AvgInterval = 60; // Calculate a one minute load average 
	int     PrevInRunQ, PrevTicks, CurInRunQ, CurTicks;
	time_t  LastPoll;
    float   long_avg;
    time_t  now, elapsed_time;
    int     n_polls ;
	void 	poll(int* queue, int* ticks, time_t* poll_time);
} specific;

enum { X_SYSINFO =0 };
struct nlist AixSpecific::nl[] =
    {
        { "sysinfo" ,0,0,0,0,0},
        { ""      ,0,0,0,0,0}
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
    (void)nlist( "/vmunix", specific.nl );

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
    	float   short_avg;
    	float   weighted_long, weighted_short;

		specific.poll( &specific.CurInRunQ, &specific.CurTicks, &specific.now );

    	if( specific.n_polls == 0 ) 
		{    			/* first time through, just set things up */
        	specific.PrevInRunQ = specific.CurInRunQ;
        	specific.PrevTicks = specific.CurTicks;
        	specific.LastPoll = specific.now;
        	specific.n_polls = 1;
        	short_avg = 1.0;
        	specific.long_avg = 1.0;
        	dprintf( D_LOAD, "First Time, returning 1.0\n" );
			t = Float;
			size = sizeof(float);
			buffer = (char *)new float[1];
			*(float *)buffer = 1.0;
			break;
    	}
    	if( specific.CurTicks == specific.PrevTicks ) 
		{
        	dprintf( D_LOAD, "Too short of time to compute avg, returning 
														%5.2f\n", specific.long_avg );
			t = Float;
			size = sizeof(float);
			buffer = (char *)new float[1];
			*(float *)buffer = specific.long_avg;
			break;
    	}
    	short_avg = (float)(specific.CurInRunQ - specific.PrevInRunQ) / 
							(float)(specific.CurTicks - specific.PrevTicks) - 1.0;
    	specific.elapsed_time = specific.now - specific.LastPoll;
    	if( specific.n_polls == 1 ) 
		{    		/* second time through, init long average */
        	specific.long_avg = short_avg;
        	specific.PrevInRunQ = specific.CurInRunQ;
        	specific.PrevTicks = specific.CurTicks;
        	specific.LastPoll = specific.now;
        	specific.n_polls = 2;
        	dprintf( D_LOAD, "Second time, returning %5.2f\n", specific.long_avg );
			t = Float;
			size = sizeof(float);
			buffer = (char *)new float[1];
			*(float *)buffer = specific.long_avg;
    	}
        // after second time through, update long average with new info 

    	weighted_short = short_avg * (float)specific.elapsed_time / (float)specific.AvgInterval;
    	weighted_long = specific.long_avg *
                (float)(specific.AvgInterval - specific.elapsed_time) / (float)specific.AvgInterval;
    	specific.long_avg = weighted_long + weighted_short;
    	dprintf( D_LOAD, "ShortAvg = %5.2f LongAvg = %5.2f\n",
                                                        short_avg, specific.long_avg );
    	dprintf( D_LOAD, "\n" );
    	specific.PrevInRunQ = specific.CurInRunQ;
    	specific.PrevTicks = specific.CurTicks;
    	specific.LastPoll = specific.now;

		t = Float;
		size = sizeof(float);
		buffer = (char *)new float[1];
		*(float *)buffer = specific.long_avg;
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

void
AixSpecific::poll(int*	queue, int*	ticks, time_t*	poll_time)
{
    struct sysinfo   info;

    if( lseek(kd,nl[X_SYSINFO].n_value,L_SET) < 0 ) 
	{
        perror( "lseek" );
        exit( 1 );
    }
    if( read(kd,(char *)&info,sizeof(struct sysinfo)) < sizeof(struct sysinfo)) 
	{
        perror( "read from kmem" );
        exit( 1 );
    }
    *queue = info.runque;
    *ticks = info.runocc;
    (void)time( poll_time );
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

