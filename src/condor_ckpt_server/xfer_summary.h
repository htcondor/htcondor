#if !defined(_XFER_SUMMARY_H)
#define _XFER_SUMMARY_H

#include "xferstat2.h"

#include <stdio.h>

class XferSummary {
public:
	XferSummary() { log_file = 0; }
	init();
	Result(transferinfo *);
	void time_out(time_t);

private:
	time_t	start_time;
	int		num_sends;
	unsigned long	bytes_sent;
	int		tot_send_bandwidth;
	unsigned int		time_sending;

	int		num_recvs;
	unsigned long	bytes_recv;
	int		tot_recv_bandwidth;
	unsigned int		time_recving;
	FILE	*log_file;

	void log_transfer(time_t, transferinfo *);
};

#define XFER_SUMMARY_INTERVAL 60 * 60 /* Really, an hour */

unsigned long get_file_size(char *);

#endif
