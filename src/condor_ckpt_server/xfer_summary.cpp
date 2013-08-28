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
#include "internet.h"
#include "condor_getcwd.h"



XferSummary::XferSummary() 
{
	log_file = 0;
	Collectors = NULL;
	bytes_recv = 0;
	bytes_sent = 0;
	num_recvs = 0;
	num_sends = 0;
	start_time = time(0);
	time_recving = 0;
	time_sending = 0;
	tot_recv_bandwidth = 0;
	tot_send_bandwidth = 0;

}



XferSummary::~XferSummary() 
{
	if( log_file ) { fclose(log_file); }
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

	if( ! condor_getcwd( pwd ) ) {
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
	char		*str = NULL;

	SetMyTypeName(info, "CkptServer");
	SetTargetTypeName(info, "CkptFile");

	sprintf(line, "%s = \"%s\"", ATTR_NAME, my_full_hostname() );
	info.Insert(line);
    sprintf(line, "%s = \"%s\"", ATTR_MACHINE, hostaddr );
	info.Insert(line);
	sprintf(line, "%s = \"%s\"", ATTR_VERSION, CondorVersion() );
	info.Insert(line);
	sprintf(line, "%s = \"%s\"", ATTR_PLATFORM, CondorPlatform() );
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

	/* ctime adds a newline at the end of the ascii conversion.... */
	str = ctime(&start_time);
	sprintf(line, "CkptServerIntervalStart = \"%s\"", str ? str : "Unknown\n");
	tmp = strchr( line, '\n' );
	if (tmp != NULL) {
		/* delete the newline */
		*tmp = '\"';
		tmp++;
		*tmp = '\0';
	}
	info.Insert(line);

	/* ctime adds a newline at the end of the ascii conversion.... */
	str = ctime(&now);
	sprintf(line, "CkptServerIntervalEnd = \"%s\"", str ? str : "Unknown\n");
	tmp = strchr( line, '\n' );
	if (tmp != NULL) {
		/* delete the newline */
		*tmp = '\"';
		tmp++;
		*tmp = '\0';
	}
	info.Insert(line);

	info.Assign("Disk", sysapi_disk_space(pwd.Value()));
	
	// Send to collector
	if ( Collectors ) {
        dprintf(D_NETWORK, "Sending CkptServer ClassAd:\n");
        dPrintAd(D_NETWORK, info);
		Collectors->sendUpdates (UPDATE_CKPT_SRVR_AD, &info, NULL, true);
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
		log_file = safe_fopen_wrapper("TransferLog", "a");
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

	fd = safe_open_wrapper_follow(path, O_RDONLY, 0);
	if (fd < 0) {
		return 0;
	}

	answer=lseek(fd,0,2);
	close(fd);
	return answer;
}


