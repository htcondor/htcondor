#include <stdio.h>
#include <sys/utsname.h>
#include <string.h>


#include "condor_debug.h"
#include "condor_sysparam.h"
#include "_condor_fix_resource.h"

// C definitions
static char *_FileName_ = __FILE__;

class LoadVector {
public:
    int     Update();
    float   Short()     { return short_avg; }
    float   Medium()    { return medium_avg; }
    float   Long()      { return long_avg; }
private:
    float   short_avg;
    float   medium_avg;
    float   long_avg;
};


Linux::Linux()
{
}

float
Linux::get_load_avg()
{
	float	load_avg;
	char	*buf;
	int	size = sizeof(char *);
	SysType	type=Float;

	if(readSysParam(LoadAvg, buf, size, type) < 0) {
		dprintf(D_ALWAYS, "Failed to read load average\n");
		return(0.0);
	}
	memcpy(&load_avg, buf, sizeof(float));
	return(load_avg);
}

int
Linux::readSysParam(const SysParamName sys, char*& buffer, int& size,SysType& t)
{
	char	c;

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
		{
		LoadVector MyLoad;
		int temp = MyLoad.Update();
		if ( temp == -1 )
		{
			dprintf( D_ALWAYS, "LoadAvg failed from Kernel \n");
			return -1;
		}
		size = sizeof(float);     // we return the long average as of now
		t    =  Float;
		buffer = (char *)new float[1];
		*(float *)buffer = MyLoad.Short();
		break;
		}
	
	case SwapSpace:  					// calc_virt_memory()
	{
	    FILE 	*proc;
	    long	free_vm;
	    char	c;
	    char	tmp_c[20];
	    long	tmp_l;
		struct utsname buf;
		int		major, minor, patch;


		// Obtain the kernel version so that we know what /proc looks like..
		if( uname(&buf) < 0 )  return -1;
		sscanf(buf.release, "%d.%d.%d", &major, &minor, &patch);

	    proc=fopen("/proc/meminfo","r");
	    if(!proc) {
		dprintf(D_ALWAYS, "Could not open /proc/meminfo");
		return -1;
	    }
	    // The /proc/meminfo looks something like this for V1.x.x:
  	    //		total:	used:	free:	shared:	buffers:
	    //Mem:     7294976 7159808 135168  4349952  1748992
	    //Swap:   33898496 8929280 24969216

		// and like this for 2.0.0...
        //       total:    used:    free:  shared: buffers:  cached:
		//Mem:  19578880 19374080   204800  7671808  1191936  8253440
		//Swap: 42831872  8368128 34463744
		//MemTotal:     19120 kB
		//MemFree:        200 kB
		//MemShared:     7492 kB
		//Buffers:       1164 kB
		//Cached:        8060 kB
		//SwapTotal:    41828 kB
		//SwapFree:     33656 kB

		switch(major) {
			case 1:
			case 2:
	    		while((c=fgetc(proc))!='\n');
	    		while((c=fgetc(proc))!='\n');
	    		fscanf(proc, "%s %d %d %d", tmp_c, &tmp_l, &free_vm);
	    		dprintf(D_ALWAYS, "Swap Space:%d\n", free_vm);
				break;

			default:
				dprintf(D_ALWAYS, "/proc format unknown for kernel version %d.%d.%d",
					major, minor, patch);
	    		fclose(proc);
				return -1;
				break;
		}


	    fclose(proc);

  	    size = sizeof(long);
	    t    =  Long;
	    buffer = (char *)new long[1];
	    *(long *)buffer = free_vm / 1024;
	    break;
	}

	case PhyMem:						//  calc_phys_memory()
	//We can't get the exact amount, but we can get close.  What we return
	//is the amount of physical memory - size of kernel.
	{
	    FILE 	*proc;
	    long	phys_mem;
	    char	tmp_c[20];
	    long	tmp_l;
		struct utsname buf;
		int		major, minor, patch;


		// Obtain the kernel version so that we know what /proc looks like..
		if( uname(&buf) < 0 )  return -1;
		sscanf(buf.release, "%d.%d.%d", &major, &minor, &patch);

	    proc=fopen("/proc/meminfo","r");
	    if(!proc) {
		dprintf(D_ALWAYS, "Could not open /proc/meminfo");
		return -1;
	    }
	    // The /proc/meminfo looks something like this:
  	    //		total:	used:	free:	shared:	buffers:
	    //Mem:     7294976 7159808 135168  4349952  1748992
	    //Swap:   33898496 8929280 24969216

		// and like this for 2.0.0...
        //       total:    used:    free:  shared: buffers:  cached:
		//Mem:  19578880 19374080   204800  7671808  1191936  8253440
		//Swap: 42831872  8368128 34463744
		//MemTotal:     19120 kB
		//MemFree:        200 kB
		//MemShared:     7492 kB
		//Buffers:       1164 kB
		//Cached:        8060 kB
		//SwapTotal:    41828 kB
		//SwapFree:     33656 kB

		switch(major) {
			case 1:
			case 2:
	    		while((c=fgetc(proc))!='\n');
	    		fscanf(proc, "%s %d", tmp_c, &phys_mem);
				break;

			default:
				dprintf(D_ALWAYS, "/proc format unknown for kernel version %d.%d.%d",
					major, minor, patch);
	    		fclose(proc);
				return -1;
				break;
		}

		fclose(proc);

  	    size = sizeof(long);
	    t    =  Long;
	    buffer = (char *)new long[1];
	    *(long *)buffer = phys_mem / 1024;
	    break;
	}

	case CpuType:
	{
	    FILE 	*proc;
	    char	cpu[20];
	    char	tmp_c[20];
	    long	tmp_l;
		struct utsname buf;
		int		major, minor, patch;


		// Obtain the kernel version so that we know what /proc looks like..
		if( uname(&buf) < 0 )  return -1;
		sscanf(buf.release, "%d.%d.%d", &major, &minor, &patch);

	    proc=fopen("/proc/cpuinfo","r");
	    if(!proc) {
		dprintf(D_ALWAYS, "Could not open /proc/cpuinfo");
		return -1;
	    }

		// cpuinfo looks like:
		// Kernel version 1.x.x:

		// Really not sure anymore... But, CPU on the third line...

		// Kernel version 2.0.0:

		// processor       : 0
		// cpu             : 486
		// model           : DX/4-WB
		// vendor_id       : AuthenticAMD
		// stepping        : 4
		// fdiv_bug        : no
		// hlt_bug         : no
		// fpu             : yes
		// fpu_exception   : yes
		// cpuid           : yes
		// wp              : yes
		// flags           : fpu
		// bogomips        : 3.72

		switch(major) {
			case 1:
	    		fscanf(proc, "%s %s %s", tmp_c, tmp_c, cpu);
				break;
			case 2:
	    		while((c=fgetc(proc))!='\n');
	    		fscanf(proc, "%s : %s", tmp_c, cpu);
				break;

			default:
				dprintf(D_ALWAYS, "/proc format unknown for kernel version %d.%d.%d",
					major, minor, patch);
	    		fclose(proc);
				return -1;
				break;
		}

  	    fclose(proc);

  	    size = strlen(cpu)+1;
	    t    =  String;
	    buffer = new char[size];
	    memcpy(buffer, buf.machine, size);
	    break;
	}


	default:
		dprintf(D_ALWAYS, "readSysParam: default\n");
		return -1;
	}
	return 1;
}

int
LoadVector::Update()
{
    FILE	*proc;
	struct utsname buf;
	int		major, minor, patch;


	// Obtain the kernel version so that we know what /proc looks like..
	if( uname(&buf) < 0 )  return -1;
	sscanf(buf.release, "%d.%d.%d", &major, &minor, &patch);

	// /proc/loadavg looks like:

	// Kernel Version 2.0.0:
	// 0.03 0.03 0.09 2/42 15582


    proc=fopen("/proc/loadavg","r");
    if(!proc)
	return -1;

	switch(major) {
		case 1:
		case 2:
    		fscanf(proc, "%f %f %f", &short_avg, &medium_avg, &long_avg);
			break;

		default:
			dprintf(D_ALWAYS, "/proc format unknown for kernel version %d.%d.%d",
				major, minor, patch);
    		fclose(proc);
			return -1;
			break;
	}

    fclose(proc);

    dprintf(D_FULLDEBUG, "Load avg: %f %f %f\n", short_avg, medium_avg, long_avg);

    return 1;
}

// Function to print out system parameter to log file
void
Linux::disSysParam(const SysParamName sys, const char* buffer, 
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
            if ( size != sizeof(long) ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != Long ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New virt_mem = %i\n", *(long *)buffer);
			break;
			
	case PhyMem:
            if ( size != sizeof(long) ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != Long ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New phys_mem = %i\n", *(long *)buffer);
			break;

	case CpuType:
            if ( size == 0 ) dprintf(D_ALWAYS,"Error in size\n");
            else if ( type != String ) dprintf(D_ALWAYS, "Error in type\n");
            else dprintf(D_ALWAYS,"New cpu_type = %s\n", *(char *)buffer);
			break;

        default:
            dprintf(D_ALWAYS, "disSysParam : no match\n");
            break;
    }
}


// dummy stubs
SunOs:: SunOs() {}
int  SunOs::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void SunOs::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

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
