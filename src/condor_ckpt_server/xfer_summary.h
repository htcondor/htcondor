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
#if !defined(_XFER_SUMMARY_H)
#define _XFER_SUMMARY_H

#include "condor_io.h"
#include "xferstat2.h"
#include "dc_collector.h"

class XferSummary {
public:
	XferSummary();
	~XferSummary();
	void init();
	void Result( transferinfo *, bool success_flag, struct in_addr peer,
				 int xfer_size);
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

	char	*subnet;
	char	pwd[_POSIX_PATH_MAX];

	CollectorList * Collectors;

	void log_transfer(time_t, transferinfo *, bool success_flag,
					  struct in_addr peer, int xfer_size);
};

#define XFER_SUMMARY_INTERVAL 60 * 60 /* Really, an hour */

unsigned long get_file_size(char *);

#endif
