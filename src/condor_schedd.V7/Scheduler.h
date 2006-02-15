/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"

#include "JobLogReader.h"

class Scheduler: public Service {
public:
	Scheduler();
	virtual ~Scheduler();
	void init();
	void config();
	void stop();

	classad::ClassAdCollection *GetClassAds() {return &ad_collection;}
	char const *Name() const {return m_name.c_str();}

private:
	std::string m_name;

	ClassAd m_public_ad;
	int m_public_ad_update_interval;
	int m_public_ad_update_timer;

	classad::ClassAdCollection ad_collection;
	JobLogReader job_log_reader;
	class CollectorList *m_collectors;

	int log_reader_polling_timer;
	int log_reader_polling_period;

	void InitPublicAd();
	void TimerHandler_UpdateCollector();
	void InvalidatePublicAd();

	void TimerHandler_JobLogPolling();
};

#endif
