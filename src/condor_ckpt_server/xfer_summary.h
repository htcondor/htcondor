/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#if !defined(_XFER_SUMMARY_H)
#define _XFER_SUMMARY_H

#include "condor_io.h"
#include "xferstat2.h"


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

	char* CollectorHost;
	char* CondorViewHost;

	void log_transfer(time_t, transferinfo *, bool success_flag,
					  struct in_addr peer, int xfer_size);
};

#define XFER_SUMMARY_INTERVAL 60 * 60 /* Really, an hour */

unsigned long get_file_size(char *);

#endif
