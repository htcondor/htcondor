#include <string.h>
#include "HoldReasons.h"
#include "param_info.h"

typedef struct HoldReasonEntry_t {
	const char * key;
	const char * value;
} HoldReasonEntry;

HoldReasonEntry holdReasonTable[] = {
	{ "VMGAHP_ERR_JOBCLASSAD_KVM_INVALID_DISK_PARAM",
		"The attribute vm_disk was invalid.  Please consult the manual, "
		"or the condor_submit man page, for information about the syntax of "
		"vm_disk.  A syntactically correct value may be invalid if the "
		"on-disk permissions of a file specified in it do not match the requested "
		"permissions.  Presently, files not transferred to the root of the working "
		"directory must be specified with full paths." },
	{ "VMGAHP_ERR_JOBCLASSAD_KVM_MISMATCHED_CHECKPOINT",
		"KVM jobs can not presently checkpoint if any of their disk files are not "
		"on a shared filesystem.  Files on a shared filesystem must be specified in "
		"vm_disk with full paths." },
	{ "VMGAHP_ERR_JOBCLASSAD_KVM_NO_DISK_PARAM",
		"The attribute VMPARAM_vm_Disk was not set in the job ad sent to the "
		"VM GAHP.  HTCondor will usually set this attribute when you submit a valid "
		"KVM job (it is derived from vm_disk).  Examine your job and verify that "
		"VMPARAM_vm_Disk is set.  If it is, please contact your administrator." },
	{ "VMGAHP_ERR_JOBCLASSAD_MISMATCHED_HARDWARE_VT",
		"Don't use 'vmx' as the name of your kernel image.  Pick something else and "
		"change xen_kernel to match." },
	{ "VMGAHP_ERR_JOBCLASSAD_NO_VM_MEMORY_PARAM",
		"The attribute JobVMMemory was not set in the job ad sent to the "
		"VM GAHP.  HTCondor will usually prevent you from submitting a VM universe job "
		"without JobVMMemory set.  Examine your job and verify that JobVMMemory is set.  "
		"If it is, please contact your administrator." },
	{ "VMGAHP_ERR_JOBCLASSAD_NO_VMWARE_VMX_PARAM",
		"The attribute VMPARAM_VMware_Dir was not set in the job ad sent to the "
		"VM GAHP.  HTCondor will usually set this attribute when you submit a valid "
		"VMWare job (it is derived from vmware_dir).  If you used condor_submit to "
		"submit this job, contact your administrator.  Otherwise, examine your job "
		"and verify that VMPARAM_VMware_Dir is set.  If it is, contact your "
		"administrator." },
	{ "VMGAHP_ERR_JOBCLASSAD_XEN_INITRD_NOT_FOUND",
		"HTCondor could not read from the file specified by xen_initrd.  "
		"Check the path and the file's permissions.  If it's on a shared filesystem, "
		"you may need to alter your job's requirements expression to ensure the "
		"filesystem's availability." },
	{ "VMGAHP_ERR_JOBCLASSAD_XEN_INVALID_DISK_PARAM",
		"The attribute vm_disk was invalid.  Please consult the manual, "
		"or the condor_submit man page, for information about the syntax of "
		"vm_disk.  A syntactically correct value may be invalid if the "
		"on-disk permissions of a file specified in it do not match the requested "
		"permissions.  Presently, files not transferred to the root of the working "
		"directory must be specified with full paths." },
	{ "VMGAHP_ERR_JOBCLASSAD_XEN_KERNEL_NOT_FOUND",
		"HTCondor could not read from the file specified by xen_kernel.  "
		"Check the path and the file's permissions.  If it's on a shared filesystem, "
		"you may need to alter your job's requirements expression to ensure the "
		"filesystem's availability." },
	{ "VMGAHP_ERR_JOBCLASSAD_XEN_MISMATCHED_CHECKPOINT",
		"Xen jobs can not presently checkpoint if any of their disk files are not "
		"on a shared filesystem.  Files on a shared filesystem must be specified in "
		"vm_disk with full paths." },
	{ "VMGAHP_ERR_JOBCLASSAD_XEN_NO_DISK_PARAM",
		"The attribute VMPARAM_vm_Disk was not set in the job ad sent to the "
		"VM GAHP.  HTCondor will usually set this attribute when you submit a valid "
		"Xen job (it is derived from vm_disk).  Examine your job and verify that "
		"VMPARAM_vm_Disk is set.  If it is, please contact your administrator." },
	{ "VMGAHP_ERR_JOBCLASSAD_XEN_NO_KERNEL_PARAM",
		"The attribute VMPARAM_Xen_Kernel was not set in the job ad sent to the "
		"VM GAHP.  HTCondor will usually set this attribute when you submit a valid "
		"Xen job (it is derived from xen_kernel).  Examine your job and verify that "
		"VMPARAM_Xen_Kernel is set.  If it is, please contact your administrator." },
	{ "VMGAHP_ERR_JOBCLASSAD_XEN_NO_ROOT_DEVICE_PARAM",
		"The attribute VMPARAM_Xen_Root was not set in the job ad sent to the "
		"VM GAHP.  HTCondor will usually set this attribute when you submit a valid "
		"Xen job (it is derived from xen_root).  Examine your job and verify that "
		"VMPARAM_Xen_Root is set.  If it is, please contact your administrator." },
};

const char * explainHoldReason( const char * holdReason ) {
	const char * holdReasonPtr = holdReason;
	if( strstr( holdReason, "Error from " ) == holdReason ) {
		holdReasonPtr = strchr( holdReason, ':' );
		if( holdReasonPtr != NULL ) { ++holdReasonPtr; ++holdReasonPtr; }
	}

	if( holdReasonPtr == NULL ) { return NULL; }
	const HoldReasonEntry * hre = BinaryLookup<HoldReasonEntry>( holdReasonTable, sizeof( holdReasonTable ) / sizeof( HoldReasonEntry ), holdReasonPtr, strcmp );
	if( hre == NULL ) { return holdReason; }
	return hre->value;
}

