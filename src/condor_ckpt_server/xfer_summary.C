#include <fcntl.h>
#include "condor_debug.h"
#include "expr.h"
#include "dgram_io_handle.h"
#include "xferstat2.h"
#include "xfer_summary.h"


#define COLLECTOR_UDP_PORT	9613	/* From sched.h which confilicts w/ local
									   headers :( */
#define SCHED_VERS			400
#define SCHEDD_INFO			(SCHED_VERS+15)

extern "C" {
	int udp_unconnect();
	int send_context_to_machine();
	int display_context();
	char *param();
	int fill_dgram_io_handle();
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
	CONTEXT	*my_context;
	ELEM	tmp;
	int		sock_fd;
	DGRAM_IO_HANDLE	CollectorIOHandle;
	char	*CollectorHost;

	if (now - start_time > XFER_SUMMARY_INTERVAL) {
		// Do something with this data
		my_context = create_context();
		tmp.type = INT;
		tmp.i_val = num_sends;
		store_stmt( build_expr("NumSends", &tmp), my_context);
		tmp.i_val = bytes_sent;
		store_stmt( build_expr("BytesSent", &tmp), my_context);
		tmp.i_val = time_sending;
		store_stmt( build_expr("TimeSending", &tmp), my_context);
		if (num_sends != 0) {
			tmp.i_val = tot_send_bandwidth / num_sends;
		} else {
			tmp.i_val = 0;
		}
		store_stmt( build_expr("AvgSendBandwidth", &tmp), my_context);

		tmp.i_val = num_recvs;
		store_stmt( build_expr("NumRecvs", &tmp), my_context);
		tmp.i_val = bytes_recv;
		store_stmt( build_expr("BytesReceived", &tmp), my_context);
		tmp.i_val = time_recving;
		store_stmt( build_expr("TimeReceving", &tmp), my_context);
		if (num_recvs != 0) {
			tmp.i_val = tot_recv_bandwidth / num_recvs;
		} else {
			tmp.i_val = 0;
		}
		store_stmt( build_expr("AvgReceiveBandwidth", &tmp), my_context);

		tmp.type = STRING;
		tmp.s_val = ctime(&start_time);
		store_stmt( build_expr("CKPT_SERVER_INTERVAL_START", &tmp), 
				   my_context);
		tmp.s_val = ctime(&now);
		store_stmt( build_expr("CKPT_SERVER_INTERVAL_END", &tmp), 
				   my_context);

		display_context( my_context );
		sock_fd = udp_unconnect();
		if (sock_fd >= 0) {
			CollectorHost = param("COLLECTOR_HOST");
			if (CollectorHost != 0) {
				fill_dgram_io_handle(&CollectorIOHandle, CollectorHost, 
									 sock_fd, COLLECTOR_UDP_PORT);
				/* NOT REALLY SCHEDD_INFO */
				send_context_to_machine(&CollectorIOHandle, SCHEDD_INFO,
										my_context);
			}
			CollectorHost = param("ALTERNATE_COLLECTOR_HOST");
			if (CollectorHost != 0) {
				fill_dgram_io_handle(&CollectorIOHandle, CollectorHost, 
									 sock_fd, COLLECTOR_UDP_PORT);
				/* NOT REALLY SCHEDD_INFO */
				send_context_to_machine(&CollectorIOHandle, SCHEDD_INFO,
										my_context);
			}
			close(sock_fd);
		}

		free_context(my_context);
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


