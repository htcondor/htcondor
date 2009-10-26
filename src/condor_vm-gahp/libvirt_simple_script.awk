#!/usr/bin/awk -f

BEGIN {
# So that the awk interpreter does not attempt to read the command
# line as a list of input files.
    ARGC = 1;

# Debugging output is requested
    if(ARGV[1] != "")
    {
	debug_file = ARGV[1];
    }
    else
    {
# By doing this, we ensure that we only need to test for debugging
# output once; if it is not requested, we just write all the debugging
# statements to /dev/null
	debug_file = "/dev/null";
    }
}

# The idea here is to collect the job attributes as they are passed to
# us by the VM GAHP.  The GAHP then outputs a blank line to indicate
# that the entire classad has been sent to us.
{
    gsub(/\"/, "")
    attrs[$1] = $3
    bootloader = ""
    root = ""
    initrd = ""
    kernel = ""
    kern_parms = ""
    print "Received attribute: " $1 "=" attrs[$1] >debug_file
}

END {
    if(index(attrs["JobVMType"],"xen") != 0) 
    {
	print "<domain type='xen'>";
	os_type = "linux"
	if(index(attrs["VMPARAM_Xen_Kernel"],"included") != 0)
	{
	    bootloader = attrs["VMPARAM_Xen_Bootloader"]
	}
	else
	{
	    kernel = attrs["VMPARAM_Xen_Kernel"]
	    root = attrs["VMPARAM_Xen_Root"]
	    initrd = attrs["VMPARAM_Xen_Initrd"]
            kern_params = attrs["VMPARAM_Xen_Kernel_Params"]
        }
    }
    else if(index(attrs["JobVMType"],"kvm") != 0)
    {
	print "<domain type='kvm'>" ;
        os_type = "hvm"
    }
    else
    {
	print "Unknown VM type: " index(attrs["JobVMType"], "kvm") >debug_file;
	exit(-1);
    }
    print "<name>" attrs["VMPARAM_VM_NAME"] "</name>" ;
    print "<memory>" attrs["JobVMMemory"] * 1024 "</memory>" ;
    print "<vcpu>" attrs["JobVM_VCPUS"] "</vcpu>" ;
    print "<os><type>" os_type "</type>" ;
    if(kernel != "")
    {
	print "<kernel>" kernel "</kernel>"
    }
    if(initrd != "")
    {
	print "<initrd>" initrd "</initrd>"
    }
    if(root != "")
    {
	print "<root>" root "</root>"
    }
    if(kern_params != "")
    {
	print "<cmdline>" kern_params "</cmdline>"
    }
    print "</os>" ;
    if(bootloader != "")
    {
	print "<bootloader>" bootloader "</bootloader>"
    }
    print "<devices>" ;
    if(attrs["JobVMNetworking"] == "TRUE")
    {
	if(index(attrs["JobVMNetworkingType"],"nat") != 0)
	{
	    print "<interface type='network'><source network='default'/></interface>" ;
	}
	else
	{
	    print "<interface type='bridge'><mac address='" attrs["JobVM_MACADDR"] "'/></interface>" ;
	}
    }
    print "<disk type='file'>" ;
	if(index(attrs["JobVMType"],"xen") != 0) 
    {
	 split(attrs["VMPARAM_Xen_Disk"], disk_string, ":");
    }
    else if(index(attrs["JobVMType"],"kvm") != 0)
    {
	split(attrs["VMPARAM_Kvm_Disk"], disk_string, ":");
    }
    print "<source file='" disk_string[1] "'/>" ;
    print "<target dev='" disk_string[2] "'/>" ;
    print "</disk></devices></domain>" ;
    exit(0);
}
