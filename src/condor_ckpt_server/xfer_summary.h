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
    void time_out(time_t, char *);

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
	MyString pwd;

	CollectorList * Collectors;

	void log_transfer(time_t, transferinfo *, bool success_flag,
					  struct in_addr peer, int xfer_size);
};

#define XFER_SUMMARY_INTERVAL 60 * 60 /* Really, an hour */

unsigned long get_file_size(char *);

#endif
