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

#ifndef __GANGLIAD_H__
#define __GANGLIAD_H__

#include "statsd.h"
#include "ganglia_interaction.h"

class GangliaMetric: public Metric {
 public:
	char const *gangliaMetricType() const;
	int gangliaSlope() const;
};

class GangliaD: public StatsD {
 public:
	GangliaD();
	~GangliaD();

	virtual void initAndReconfig();

	// create a GangliaMetric
	virtual Metric *newMetric(Metric const *copy_me=NULL);

	// publish a metric to Ganglia
	virtual void publishMetric(Metric const &metric);

	// apply ganglia-specific logic relating to the IP address in the spoof host
	virtual bool getDaemonIP(std::string const &machine,std::string &result) const;

 private:
	unsigned m_tmax; // max time between updates
	unsigned m_dmax; // max time to deletion of metrics that are not updated
	std::string m_ganglia_libname;
	Ganglia_pool m_ganglia_context;
	Ganglia_gmond_config m_ganglia_config;
	Ganglia_udp_send_channels m_ganglia_channels;
	bool m_ganglia_noop;
};

#endif
