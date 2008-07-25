#ifndef STARTD_FACTORY_ATTRS_H
#define STARTD_FACTORY_ATTRS_H

// Attrs to represent a partition
#define ATTR_PARTITION_NAME		"Partition"
#define ATTR_PARTITION_BACKER	"Backer"
#define ATTR_PARTITION_KIND		"Kind"
#define ATTR_PARTITION_SIZE		"Size"
#define ATTR_PARTITION_STATE	"State"

// attrs to represent a workload (a schedd at this time)
#define ATTR_WKLD_NAME						"WorkloadName"
#define ATTR_WKLD_SMP_RUNNING				"SMPRunning"
#define ATTR_WKLD_SMP_IDLE					"SMPIdle"
#define ATTR_WKLD_DUAL_RUNNING				"DUALRunning"
#define ATTR_WKLD_DUAL_IDLE					"DUALIdle"
#define ATTR_WKLD_VN_RUNNING				"VNRunning"
#define ATTR_WKLD_VN_IDLE					"VNIdle"

#endif
