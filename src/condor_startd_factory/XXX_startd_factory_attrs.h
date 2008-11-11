/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
