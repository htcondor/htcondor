##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

##
## convert_config_to_win32.awk
##
## Todd Tannenbaum <tannenba@cs.wisc.edu>, 10/06
## John Knoeller <johnkn@cs.wisc.edu>, 12/10
##

# This is an awk script to modify condor_config.generic into a form that 
# works with Windows.  
# Pipe condor_config.generic through this awk script and save it as
# condor_config.win, which is what the Installer expects to modify after unbundling.

BEGIN {
	FS = "="
}

# Make it so we can fill in the VM_GAHP_SERVER option if we choose to in the
# installer. This is done before the .exe append bellow, because that match 
# seems to block any other matches for that line.
/^#VM_GAHP_SERVER/ {
	print "#VM_GAHP_SERVER = $(BIN)\\condor_vm-gahp.exe"
	print "VM_GAHP_SERVER = "
	next
}
# We DONT want to add a .exe extension to the vmware script, it's a perl script.
/VMWARE_SCRIPT[ \t]*=/ {
	gsub(/\//, "\\", $0)
	printf "%s\n",$0
	next
}

# Remove SHADOW_STANDARD (the standard universe shadow)
# 
/^SHADOW_STANDARD/ {
	next
}
/^SHADOW_LIST/ {
    printf "SHADOW_LIST = SHADOW\n"
    next
}

# Remove STARTER_STANDARD (the standard universe starter)
#
/^STARTER_STANDARD/ {
    next
}
/^STARTER_LIST/ {
    printf "STARTER_LIST = STARTER\n"
    next
}

/^FILETRANSFER_PLUGINS[ \t]*=/ {
	gsub(/\//, "\\", $0)
	gsub(/_plugin,/, "_plugin.exe,", $0)
	gsub(/_plugin$/, "_plugin.exe", $0)
	printf "%s\n",$0
	next
}

# Email Settings
# Use condor_mail.exe for MAIL, and put in a stub for SMTP_SERVER
/^MAIL/ {
	printf "MAIL = $(BIN)\\condor_mail.exe\n\n"
	printf "## For Condor on Win32 we need to specify an SMTP server so\n"
	printf "## that Condor is able to send email.\n"
	printf "SMTP_SERVER =\n\n"
	printf "## For Condor on Win32 we may need to specify a \"from\"\n"
	printf "## address so that a valid address is used in the from\n"
	printf "## field.\n"
	printf "MAIL_FROM =\n\n"
	next
}
/^SMTP_SERVER/ {
	next
}

# Choose a Windows-reasonable value for RELEASE_DIR and LOCAL_DIR
#
/^RELEASE_DIR[ \t]*=/ {
	printf "RELEASE_DIR = C:\\Condor\n"
	next
}
/^LOCAL_DIR[ \t]*=/ {
    printf "LOCAL_DIR = $(RELEASE_DIR)\n"
    next
}

# Don't require a local config file.
#
/^\#REQUIRE_LOCAL_CONFIG_FILE \=/ {
   printf "REQUIRE_LOCAL_CONFIG_FILE = FALSE\n"
   next
}


# Specify $(FULL_HOSTNAME) as the sample commented-out entry for
# FILESYSTEM_DOMAIN and UID_DOMAIN, as this is what the installer expects.
/^UID_DOMAIN/ || /^FILESYSTEM_DOMAIN/ {
	printf "#%s= $(FULL_HOSTNAME)\n", $1
	next
}

# Have $(SBIN) point to $(BIN)
/^SBIN/ {
	printf "%s= $(BIN)\n", $1
	next
}

# Have $(LIBEXEC) point to $(BIN)
/^LIBEXEC/ {
	printf "%s= $(BIN)\n", $1
	next
}

# QUEUE_SUPER_USERS should be condor and SYSTEM, not root! :)
/^QUEUE_SUPER_USERS/ {
	printf "%s= condor, SYSTEM\n", $1
	next
}

# JOB_RENICE_INCREMENT must be 10 or load average will be messed up
# Be careful, it may be commented out in condor_config.generic.
/^.*JOB_RENICE_INCREMENT/ {
	printf "JOB_RENICE_INCREMENT = 10\n"
	next
}

# Clean out the initial guess of /usr/bin/java for JAVA... ;)
# wherever it is on Windows, it's not there.
/^JAVA[ \t]*=/ {
	printf "JAVA =\n"
	next
}

# Win32 has a path separator of ';', while on Unix it is ':'
/^JAVA_CLASSPATH_SEPARATOR/ {
	printf "JAVA_CLASSPATH_SEPARATOR = ;\n", $1
	next
}

# There's no reasonable default place to put log files for
# daemons that run as a user (on Unix we use /tmp). Use NUL.
/(^C_GAHP_LOG)|(^C_GAHP_LOCK)|(^C_GAHP_WORKER_THREAD_LOG)|(^C_GAHP_WORKER_THREAD_LOCK)|(^VM_GAHP_LOG)/ {
	printf "%s = NUL\n", $1
	next
}

# PROCD_ADDRESS needs to refer to a valid name for a Windows named
# pipe
/^PROCD_ADDRESS/ {
	print "PROCD_ADDRESS = \\\\.\\pipe\\condor_procd_pipe"
	next
}

# Make it so we can fill in the VM GAHP options if we choose 
# to in the installer. 
/^#VM_TYPE/ {
	print "VM_TYPE ="
	next
}
/^#VM_MAX_NUMBER/ {
	print "VM_MAX_NUMBER = $(NUM_CPUS)"
	next
}
/^#VM_MEMORY/ {
	print "VM_MEMORY = 128"
	next
}
/^#VMWARE_LOCAL_SETTINGS_FILE/ {
	print "#VMWARE_LOCAL_SETTINGS_FILE = $(RELEASE_DIR)/condor_vmware_local_settings"
	next
}

# Set it up so we can change the VMWARE_SCRIPT option if we choose to
# do so in the installer
#/^VMWARE_SCRIPT/ {
#	printf "VMWARE_SCRIPT = $(BIN)/condor_vm_vmware\n"
#	next
#}

# Change the configuration such that we can modify the VMware 
# networking types
/^#VM_NETWORKING_TYPE/ {
	print "VM_NETWORKING_TYPE = nat"
	next
}
/^#VM_NETWORKING_DEFAULT_TYPE/ {
	print "VM_NETWORKING_DEFAULT_TYPE = nat"
	next
}

# We use the equals sign so that it will not match the names above
# and trash the resulting configuration file
/^#VM_NETWORKING =/ {
	print "VM_NETWORKING = FALSE"
	next
}
/^#VMWARE_NAT_NETWORKING_TYPE/ {
	printf "VMWARE_NAT_NETWORKING_TYPE = nat\n"
	next
}
/^#VMWARE_BRIDGE_NETWORKING_TYPE/ {
	printf "VMWARE_BRIDGE_NETWORKING_TYPE = bridged\n"
	next
}

# Add a .exe to the end of all condor daemon names in $(BIN) and $(SBIN)
/BIN)\/condor_/ {
	gsub(/\//, "\\", $0)
	printf "%s.exe\n",$0
	next
}

# Add a .exe to the end of all condor daemon names in $(LIBEXEC)
/LIBEXEC)\/condor_/ {
	gsub(/\//, "\\", $0)
	printf "%s.exe\n",$0
	next
}

# convert paths relative to LOCAL_DIR, RELEASE_DIR and LOG to windows path separators
/[ \t]*=[ \t]*\$\(LOCAL_DIR\)\// {
	gsub(/\//, "\\", $0)
	printf "%s\n",$0
	next
}
/[ \t]*=[ \t]*\$\(RELEASE_DIR\)\// {
	gsub(/\//, "\\", $0)
	printf "%s\n",$0
	next
}
/[ \t]*=[ \t]*\$\(LOG\)\// {
	gsub(/\//, "\\", $0)
	printf "%s\n",$0
	next
}

# If we made it here, print out the line unchanged
{print $0}
