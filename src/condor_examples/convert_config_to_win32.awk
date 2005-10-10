##
## convert_config_to_win32.awk
##
## Todd Tannenbaum <tannenba@cs.wisc.edu>, 10/06
## Copyright Condor Team, Univ of Wisconsin-Madison, 2005
##

# This is an awk script to modify condor_config.generic into a form that 
# works with the InstallShield Windows installer.  The packager on Win32
# will pipe condor_config.generic through this awk script and save it as
# condor_config.orig, which is what the InstallShield script expects to 
# modify after unbundling.

BEGIN {
	FS = "="
}

# Add a .exe to the end of all condor daemon names
/BIN)\/condor_/ {
	printf "%s.exe\n",$0
	next
}

# Email Settings
# Use condor_mail.exe for MAIL, and put in a stub for SMTP_SERVER
/^MAIL/ {
	printf "MAIL = $(BIN)/condor_mail.exe\n\n"
	printf "## For Condor on Win32 we need to specify an SMTP server so\n"
	printf "## that Condor is able to send email.\n"
	printf "SMTP_SERVER =\n\n"
	next
}
/^SMTP_SERVER/ {
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
# Note the space after JAVA below, so we do not change other JAVA settings.
/^JAVA / {
	printf "JAVA =\n"
	next
}

# Win32 has a path separator of ';', while on Unix it is ':'
/^JAVA_CLASSPATH_SEPARATOR/ {
	printf "%s= ;\n", $1
	next
}

# If we made it here, print out the line unchanged
{print $0}

