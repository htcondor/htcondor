#include <fcntl.h>
#include "condor_debug.h"
#include "expr.h"
#include "dgram_io_handle.h"
#include "xferstat2.h"
#include "xfer_summary.h"
#include "safe_sock.h"
#include "condor_network.h"
#include "condor_classad.h"
#include "condor_collector.h"

extern "C" {
	char *param();
#if defined(LINUX) || defined(HPUX9)	
	int gethostname(char *name, unsigned int namelen);
#else
	int gethostname(char *name, int namelen);
#endif
	char *calc_subnet_name();
	char *getwd(char *path);
}


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

	gethostname(machine, 256);
	struct hostent *hostptr = gethostbyname(machine);
	sprintf(name, hostptr->h_name);
	subnet = (char *)calc_subnet_name(NULL);
}


XferSummary::Result(transferinfo *tinfo)
{
	time_t	now;
	int		xfer_len;
	int		xfer_bandwidth = 0;
	xfer_type type = tinfo->status;
	time_t xfer_begin = tinfo->start_time;
	u_lint xfer_size = tinfo->file_size;

	now = time(0);

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
			"transferred %d bytes in %d seconds (%d bytes / sec)\n", 
			xfer_size, xfer_len, xfer_bandwidth);
	log_transfer(now, tinfo);
}

extern "C" {
	CONTEXT *create_context();
	EXPR *build_expr();
	int store_stmt(EXPR *, CONTEXT *);
	void free_context( CONTEXT *);
}

void
XferSummary::time_out(time_t now)
{
	SafeSock	s(param("COLLECTOR_HOST"), COLLECTOR_UDP_COMM_PORT);
	ClassAd	   	info;
	char		line[128];
	int			command = UPDATE_CKPT_SRVR_AD;
	struct in_addr addr;

	info.SetMyTypeName("CkptServer");
	info.SetTargetTypeName("CkptFile");

	sprintf(line, "Name = \"%s\"", name);
	info.Insert(line);
	sprintf(line, "Machine = \"%s\"", machine);
	info.Insert(line);
	sprintf(line, "Subnet = \"%s\"", subnet);
	info.Insert(line);
	sprintf(line, "NumSends = %d", num_sends);
	info.Insert(line);
	sprintf(line, "BytesSent = %d", bytes_sent);
	info.Insert(line);
	sprintf(line, "TimeSending = %d", time_sending);
	info.Insert(line);
	sprintf(line, "AvgSendBandwidth = %f", num_sends ?
			tot_send_bandwidth / num_sends : 0.0);
	info.Insert(line);
	sprintf(line, "NumRecvs = %d", num_recvs);
	info.Insert(line);
	sprintf(line, "BytesReceived = %d", bytes_recv);
	info.Insert(line);
	sprintf(line, "TimeReceiving = %d", time_recving);
	info.Insert(line);
	sprintf(line, "AvgReceiveBandwidth = %f", num_recvs ?
			tot_recv_bandwidth / num_recvs : 0.0);
	info.Insert(line);
	sprintf(line, "CKPT_SERVER_INTERVAL_START = %s", ctime(&start_time));
	info.Insert(line);
	sprintf(line, "CKPT_SERVER_INTERVAL_END = %s", ctime(&now));
	info.Insert(line);
	sprintf(line, "Disk = %d", free_fs_blocks(getwd(NULL)));
	info.Insert(line);
	
	s.encode();
	s.code(command);
	info.put(s);
	s.eom();

	if (now - start_time > XFER_SUMMARY_INTERVAL) {
		init();
	}
}

void
XferSummary::log_transfer(time_t now, transferinfo *tinfo)
{
	struct tm *tm;


	if (log_file == 0) {
		log_file = fopen("TransferLog", "a");
	}
	if (log_file == 0) {
		return;
	}

	tm = localtime( &now );
	fprintf( log_file, "%d/%d %02d:%02d ", tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min );
	fprintf( log_file, "%s %u bytes in %d secs. for %s@%s\n",
			(tinfo->status == RECV ? "RECV" : "SENT"),
			tinfo->file_size,
			now - tinfo->start_time,
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


