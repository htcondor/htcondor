/***************************************************************
*
* Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "get_daemon_name.h"
#include "daemon.h"
#include "subsystem_info.h"
#include "CondorError.h"
#include "string_list.h"

#include "hdfs_plugin.h"

DECL_SUBSYSTEM( "TOOL", SUBSYSTEM_TYPE_TOOL );

#define PLUGIN_TYPE "FileTransfer"
#define PLUGIN_METHOD "hdfs"

int main(int argc, char *argv[]) {
        config();
        if (argc < 2) {  
                return 1;
        }

        if (strcmp(argv[1], "-classad") == 0) {
                printf("PluginType = %s\n", PLUGIN_TYPE);
                printf("SupportedMethods = %s\n", PLUGIN_METHOD);
                return 1;
        }        

        //Not a classad request, should contain two arguments
        if (argc < 3 ) {
                return 1;
        }

        HdfsIO hdfsIO;
        CondorError error;

        int ret = hdfsIO.get_file(argv[1], argv[2], error);

        do {
                printf ("\t\tmsg = %s\n\n", error.message());
        } while (error.pop());

        return ret;
}
