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
## convert_vmgahp_config_to_win32.awk
##
## Besed on convert_config_to_win32.awk by Todd Tannenbaum 
## <tannenba@cs.wisc.edu>, 10/06
##
## Ben Burnett <burnett@cs.wisc.edu>, 12/07
##

# This is an awk script to modify condor_vmgahp_config.vmware into a form 
# that with the Windows installer.  The packager on Win32 will pipe 
# condor_vmgahp_config.vmware through this awk script and save it as
# condor_vmgahp_config.vmware, which is what is expected to build the 
# installer.

BEGIN {
	FS = "="
}

# Set it up so we can change the VMWARE_SCRIPT option if we choose to
# do so in the installer
/^VMWARE_SCRIPT/ {
	printf "VMWARE_SCRIPT = $(BIN)/condor_vm_vmware.pl\n"
	next
}

# If we made it here, print out the line unchanged
{print $0}

