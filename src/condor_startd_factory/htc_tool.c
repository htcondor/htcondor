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


/* Here is an example of how to compile this file:

gcc xxx.c -w -Wall -g -m64 -pthread -I/bgsys/drivers/ppcfloor/include -L/bgsys/drivers/ppcfloor/lib64 -lbgpbridge -Wl,--rpath /bgsys/drivers/ppcfloor/lib64 -pthread -o xxx.c

*/


#include <stdio.h>
#include <stdlib.h>
#include "rm_api.h"

#define BGP_MLOAD_IMAGE "/bgsys/drivers/ppcfloor/boot/uloader"
#define BGP_CNOAD_IMAGE "/bgsys/drivers/ppcfloor/boot/cns,/bgsys/drivers/ppcfloor/boot/cnk"
#define BGP_IOLOAD_IMAGE "/bgsys/drivers/ppcfloor/boot/cns,/bgsys/drivers/ppcfloor/boot/linux,/bgsys/drivers/ppcfloor/boot/ramdisk"

char *xjobstate[] = {
"RM_JOB_IDLE",
"RM_JOB_STARTING",
"RM_JOB_RUNNING",
"RM_JOB_TERMINATED",
"RM_JOB_KILLED",
"RM_JOB_ERROR",
"RM_JOB_DYING",
"RM_JOB_DEBUG",
"RM_JOB_LOAD",
"RM_JOB_LOADED",
"RM_JOB_BEGIN",
"RM_JOB_ATTACH",
"RM_JOB_NAV"
};

char *xjobmode[] = {
"RM_SMP_MODE",
"RM_DUAL_MODE",
"RM_VIRTUAL_NODE_MODE",
"RM_MODE_NAV"
};

void emit_partition_info(void);

int main(int argc, char *argv[])
{
	rm_set_serial("BGP");

	emit_partition_info();

	return EXIT_SUCCESS;
}

void emit_partition_info(void)
{
	rm_partition_list_t *rmplt = NULL;
	int num_parts;
	enum rm_specification getOption;
	status_t ret;
	int i;
	rm_partition_t *part;
	/* fields of a partition object */
	pm_partition_id_t id;
	rm_partition_state_t state;
	int size;
	char *options;

	/* grab all of the partitions */
/*	ret = rm_get_partitions_info(PARTITION_ALL_FLAG, &rmplt);*/
	ret = rm_get_partitions_info(RM_PARTITION_FREE | RM_PARTITION_READY, &rmplt);
	if (ret != STATUS_OK) {
		printf("partition_test() failed\n");
		return;
	}

	rm_get_data(rmplt, RM_PartListSize, &num_parts);

	printf("Processing %d partitions\n", num_parts);

	getOption = RM_PartListFirstPart;
	for (i = 0; i < num_parts; ++i) {
		rm_get_data(rmplt, getOption, &part);

		rm_get_data(part, RM_PartitionID, &id); // free me
		printf("partition=%s,", id);
		
		rm_get_data(part, RM_PartitionState, &state);
		printf("state=%s,",
			state == RM_PARTITION_FREE ? "FREE" :
			state == RM_PARTITION_CONFIGURING ? "CONFIGURING" :
			state == RM_PARTITION_REBOOTING ? "REBOOTING" :
			state == RM_PARTITION_READY ? "READY" :
			state == RM_PARTITION_DEALLOCATING ? "DEALLOCATING" :
			state == RM_PARTITION_ERROR ? "ERROR" :
			"NAV");

		rm_get_data(part, RM_PartitionOptions, &options); // free me
		switch(options[0]) {
			case 's':
				printf("htc=smp,");
				break;
			case 'd':
				printf("htc=dual,");
				break;
			case 'v':
				printf("htc=vn,");
				break;
			default:
				printf("htc=n/a,");
				;
		}
		free(options);

		rm_get_data(part, RM_PartitionSize, &size);
		printf("cores=%d\n", size);

		free(id);
		getOption = RM_PartListNextPart;
	}

	rm_free_partition_list(rmplt);
}
