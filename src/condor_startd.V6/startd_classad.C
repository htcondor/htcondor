#include "condor_common.h"
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "condor_classad.h"
#include "proc_obj.h"
#include "dgram_io_handle.h"
#include "condor_attributes.h"
#include "condor_expressions.h"
#include "condor_collector.h"

int send_classad_to_machine(DGRAM_IO_HANDLE *UdpHandle, int cmd, const ClassAd* ad);

extern "C" {

int
send_classad_to_negotiator(DGRAM_IO_HANDLE *handle, CONTEXT *cp)
{
	int ret;
	ClassAd *ad;

	ad = new ClassAd(cp);
	ad->SetMyTypeName ("Machine");
	ad->SetTargetTypeName ("Job");
	
	ret = send_classad_to_machine(handle, UPDATE_STARTD_AD, ad);
	delete ad;
	return ret;
}

}
