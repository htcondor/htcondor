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
#include "metricd.h"

#include <set>

MetricD::MetricD() :
	m_ganglia(true)
{
}

void
MetricD::initAndReconfig(const char * /*unused*/)
{
	StatsD::initAndReconfig("METRICD", true);

	m_ganglia.initAndReconfig();
	m_prometheus.initAndReconfig();

	// Merge the backends' collector target types into our own so the unified
	// collector query covers everything every backend needs.
	struct caselt {
		bool operator()(const std::string &l, const std::string &r) const {
			return strcasecmp(l.c_str(), r.c_str()) < 0;
		}
	};
	std::set<std::string,caselt> all;
	for (const auto &t : m_target_types) all.insert(t);
	for (const auto &t : m_ganglia.getTargetTypes()) all.insert(t);
	for (const auto &t : m_prometheus.getTargetTypes()) all.insert(t);
	m_target_types.assign(all.begin(), all.end());
}

Metric *
MetricD::newMetric(Metric const *copy_me)
{
	if (copy_me) {
		return new Metric(*copy_me);
	}
	return new Metric();
}

void
MetricD::publishMetricsFromAds(std::vector<ClassAd> &daemon_ads)
{
	m_ganglia.publishMetricsFromAds(daemon_ads);
	m_prometheus.publishMetricsFromAds(daemon_ads);
}

void
MetricD::postPublishMetrics()
{
	// Ganglia has no per-cycle flush; Prometheus writes its file here.
	m_prometheus.postPublishMetrics();
}

void
MetricD::initializeHostList()
{
	m_ganglia.initializeHostList();
}

void
MetricD::sendHeartbeats()
{
	m_ganglia.sendHeartbeats();
}
