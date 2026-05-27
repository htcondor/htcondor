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

#ifndef __METRICD_H__
#define __METRICD_H__

#include "statsd.h"
#include "gangliad.h"
#include "prometheusd.h"

class ClassAd;  // forward declaration to avoid including classad headers in this file

class MetricD: public StatsD {
 public:
	MetricD();

	virtual void initAndReconfig(const char *unused = nullptr);

	virtual Metric *newMetric(Metric const *copy_me = NULL);
	virtual void publishMetric(Metric const &) {}
	virtual void publishMetricsFromAds(std::vector<ClassAd> &daemon_ads);

	virtual void initializeHostList();
	virtual void sendHeartbeats();
	virtual void postPublishMetrics();

 private:
	GangliaD    m_ganglia;
	PrometheusD m_prometheus;
};

#endif
