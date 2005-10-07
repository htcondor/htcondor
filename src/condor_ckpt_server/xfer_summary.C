/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "xferstat2.h"
#include "xfer_summary.h"
#include "condor_network.h"
#include "condor_collector.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "my_hostname.h"


extern "C" {
	char* calc_subnet_name(char*);
	char* getwd( char* );
}



XferSummary::XferSummary() 
{
	subnet = 0;
	log_file = 0;
	Collectors = NULL;
}



XferSummary::~XferSummary() 
{
	if( log_file ) { fclose(log_file); }
	if( subnet ) { free(subnet); }
	if( Collectors ) { delete Collectors; }

}

void
XferSummary::init()
{
	start_time = time(0);

	num_sends = 0;
	bytes_sent = 0;
	tot_send_bandwidth = 0;
	time_sending = 0;

	num_recvs = 0;
	bytes_recv = 0;
	tot_recv_bandwidth = 0;
	time_recving = 0;

	if( ! Collectors ) {
		Collectors = CollectorList::create();
	}

	if( subnet ) { free( subnet ); }
	subnet = (char *)calc_subnet_name(NULL);

	/* pwd is an array defined for this class to be of path_max size */
	if( ! getcwd( pwd, _POSIX_PATH_MAX ) ) {
		EXCEPT( "Can't get working directory." );
	}
}

void
XferSummary::Result(transferinfo *tinfo, bool success_flag,
					struct in_addr peer, int bytes_transferred)
{
	time_t	now;
	int		xfer_len;
	int		xfer_bandwidth = 0;
	xfer_type type = tinfo->status;
	time_t xfer_begin = tinfo->start_time;
	u_lint xfer_size = tinfo->file_size;

	now = time(0);

	// only include statistics on successful transfers
	if (success_flag) {

		xfer_len = now - xfer_begin;
		
		if (xfer_len != 0) {
			xfer_bandwidth = xfer_size / xfer_len;
		}

		if (type == RECV) {
			dprintf(D_ALWAYS, "RECV ");
			num_recvs++;
			bytes_recv += xfer_size;
			tot_recv_bandwidth += xfer_bandwidth;
			time_recving += xfer_len;
		} else if (type == XMIT) {
			dprintf(D_ALWAYS, "SEND ");
			num_sends++;
			bytes_sent += xfer_size;
			tot_send_bandwidth += xfer_bandwidth;
			time_sending += xfer_len;
		}
		dprintf(D_ALWAYS | D_NOHEADER,
				"transferred %lu bytes in %d seconds (%d bytes / sec)\n", 
				(unsigned long) xfer_size, xfer_len, xfer_bandwidth);
	}

	log_transfer(now, tinfo, success_flag, peer, bytes_transferred);
}


void
XferSummary::time_out(time_t now, char *hostaddr)
{
	ClassAd	   	info;
	char		line[128], *tmp;

	info.SetMyTypeName("CkptServer");
	info.SetTargetTypeName("CkptFile");

	sprintf(line, "%s = \"%s\"", ATTR_NAME, my_full_hostname() );
	info.Insert(line);
    sprintf(line, "%s = \"%s\"", ATTR_MACHINE, hostaddr );
	info.Insert(line);
	sprintf(line, "%s = \"%s\"", ATTR_VERSION, CondorVersion() );
	info.Insert(line);
	sprintf(line, "%s = \"%s\"", ATTR_PLATFORM, CondorPlatform() );
	info.Insert(line);
	sprintf(line, "Subnet = \"%s\"", subnet);
	info.Insert(line);
	sprintf(line, "NumSends = %d", num_sends);
	info.Insert(line);
	sprintf(line, "BytesSent = %d", (int) bytes_sent);
	info.Insert(line);
	sprintf(line, "TimeSending = %d", time_sending);
	info.Insert(line);
	sprintf(line, "AvgSendBandwidth = %f", num_sends ?
			tot_send_bandwidth / num_sends : 0.0);
	info.Insert(line);
	sprintf(line, "NumRecvs = %d", num_recvs);
	info.Insert(line);
	sprintf(line, "BytesReceived = %d", (int) bytes_recv);
	info.Insert(line);
	sprintf(line, "TimeReceiving = %d", time_recving);
	info.Insert(line);
	sprintf(line, "AvgReceiveBandwidth = %f", num_recvs ?
			tot_recv_bandwidth / num_recvs : 0.0);
	info.Insert(line);
	sprintf(line, "CkptServerIntervalStart = \"%s\"", ctime(&start_time));
	tmp = strchr( line, '\n' );
	strcpy( tmp, "\"" );
	info.Insert(line);
	sprintf(line, "CkptServerIntervalEnd = \"%s\"", ctime(&now));
	tmp = strchr( line, '\n' );
	strcpy( tmp, "\"" );
	info.Insert(line);
	sprintf(line, "Disk = %d", sysapi_disk_space(pwd) );
	info.Insert(line);
	
	// Send to collector
	if ( Collectors ) {
        dprintf(D_NETWORK, "Sending CkptServer ClassAd:\n");
        info.dPrint(D_NETWORK);
		Collectors->sendUpdates (UPDATE_CKPT_SRVR_AD, &info, NULL);
	}

	init();
}

void
XferSummary::log_transfer(time_t now, transferinfo *tinfo, bool success_flag,
						  struct in_addr peer, int xfer_size)
{
	struct tm *tm;
	char peer_IP[20];

	if (log_file == 0) {
		log_file = fopen("TransferLog", "a");
	}
	if (log_file == 0) {
		return;
	}

	tm = localtime( &now );
	// make local copy of peer address to avoid calling inet_ntoa twice in
	// fprintf below
	strcpy(peer_IP, inet_ntoa(peer));
	fprintf( log_file, "%d/%d %02d:%02d:%02d ", tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec );
	fprintf( log_file, "%s %s %d bytes %d sec %s %s@%s\n",
			(tinfo->status == RECV ? "R" : "S"),
			(success_flag ? "S" : "F"),
			xfer_size,
			(int) (now - tinfo->start_time),
			peer_IP,
			tinfo->owner,
			inet_ntoa(tinfo->shadow_addr));
	fflush( log_file );
}


unsigned long
get_file_size(char *path)
{
	int		fd;
	unsigned long answer;

	fd = open(path, O_RDONLY, 0);
	if (fd < 0) {
		return 0;
	}

	answer=lseek(fd,0,2);
	close(fd);
	return answer;
}


