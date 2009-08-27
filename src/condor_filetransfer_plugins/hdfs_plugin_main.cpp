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
