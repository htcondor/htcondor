#include "condor_common.h"
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "condor_classad.h"
#include "dgram_io_handle.h"
#include "condor_attributes.h"
#include "condor_expressions.h"
#include "condor_collector.h"
#include "condor_io.h"
#include "condor_adtypes.h"

extern "C" {

int
send_classad_to_machine(char *host, int port, int sd, ClassAd* ad)
{
	SafeSock sock(host, port);

	ad->SetMyTypeName (STARTD_ADTYPE);
	ad->SetTargetTypeName (JOB_ADTYPE);
	
	sock.attach_to_file_desc(sd);
	sock.encode();
	sock.put(UPDATE_STARTD_AD);
	ad->put(sock);
	sock.end_of_message();
	
	return 0;
}

}
