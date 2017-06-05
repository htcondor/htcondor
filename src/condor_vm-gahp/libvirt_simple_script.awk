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
    key = $1
    # Matching value should be $3-$NR
    $1 = ""
    $2 = ""
    sub(/^  /, "")
    attrs[key] = $0
    bootloader = ""
    root = ""
    initrd = ""
    kernel = ""
    kern_params = ""
    print "Received attribute: " key "=" attrs[key] >>debug_file
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
	print "Unknown VM type: " index(attrs["JobVMType"], "kvm") >>debug_file;
	exit(-1);
    }
    print "<name>" attrs["VMPARAM_VM_NAME"] "</name>" ;

    if( attrs["JobVMMemory"] != "" && attrs["JobVMMemory"] != "undefined" ) {
    	print "<memory>" attrs["JobVMMemory"] * 1024 "</memory>" ;
    } else {
	    print "<memory>" attrs["RequestMemory"] * 1024 "</memory>" ;
    }

    if( attrs["JobVM_VCPUS"] != "" && attrs["JobVM_VCPUS"] != "undefined" ) {
	    print "<vcpu>" attrs["JobVM_VCPUS"] "</vcpu>" ;
    } else {
	    print "<vcpu>" attrs["RequestCpus"] "</vcpu>" ;
    }

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

    # Make sure guests destroy on power off
    print "<features><acpi/><apic/><pae/></features>" ;
    print "<on_poweroff>destroy</on_poweroff><on_reboot>restart</on_reboot><on_crash>restart</on_crash>" ;

    print "<devices>" ;
    print "<console type='pty'><source path='/dev/ptmx'/></console>" ;

    # Check VNC settings 
    if(attrs["JobVMVNCConsole"] == "true")
    {
      print "<graphics type='vnc' port='-1' autoport='yes' keymap='en-us'/>" ;
    }

    # To see full input ad to script set D_FULLDEBUG
    if(attrs["JobVMNetworking"] == "true")
    {
	if(index(attrs["JobVMNetworkingType"],"nat") != 0)
	{
	    print "<interface type='network'><source network='default'/>" ;
            if(attrs["JobVM_MACADDR"] != "")
            {
		print"<mac address='" attrs["JobVM_MACADDR"] "'/>" ;
            }
	    print "</interface>" ;
	}
	else
	{
	    print "<interface type='bridge'>" ;
            if(attrs["JobVM_MACADDR"] != "")
            {
		print"<mac address='" attrs["JobVM_MACADDR"] "'/>" ;
            }
	    if(attrs["VMPARAM_Bridge_Interface"] != "")
	    {
		print "<source bridge='" attrs["VMPARAM_Bridge_Interface"] "'/>" ;
	    }
	    print "</interface>" ;
	}
    }

    
    n=split(attrs["VMPARAM_vm_Disk"], full_disk, " *, *");
    for ( i=1; i<=n; i++ )
    {
        # count is used to determine if format is passed.
        p = split(full_disk[i], disk_string, ":");

        if (index( disk_string[1], "iso") )
        {
            print "<disk type='file' device='cdrom'>";
        }
        else
        {
            print "<disk type='file'>" ;
        }
        
        if ( p == 4 )
        {
            # only an issue w/qemu
            if (attrs["JobVMType"] == "kvm")
            {
                print "<driver name='qemu' type='" disk_string[4] "'/>";
            }
        }

        if ( index( disk_string[1], "/" ) == 1 )
        {
            print "<source file='" disk_string[1] "'/>" ;
        }
        else
        {
            print "<source file='" attrs["VM_WORKING_DIR"] "/" disk_string[1] "'/>" ;
        }
        print "<target dev='" disk_string[2] "'/>" ;
        
        if ( disk_string[3] == "r" )
        {
            print "<readonly/>"
        }

        print "</disk>" ;
    }

     print "</devices></domain>"

    
    exit(0);
}
