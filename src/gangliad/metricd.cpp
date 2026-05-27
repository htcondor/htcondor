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
#include "condor_debug.h"
#include "condor_classad.h"
#include "metricd.h"

MetricD::MetricD() :
	m_ganglia(true)		// we want to use GangliaD as a backend, as the MetricD will be doing the work of running the publication cycle
{
}

void
MetricD::initAndReconfig(const char * /*unused*/)
{
	StatsD::initAndReconfig("METRICD", false);

	m_ganglia.initAndReconfig();
	m_prometheus.initAndReconfig();

	// MetricD is the only instance that queries the collector, and it parses
	// every metric definition itself (it applies no ExportMetric filter), so
	// its own m_target_types and projection already cover everything every
	// backend needs.  We only need to fold in each backend's extra projection
	// requirements (e.g. PrometheusD wants ATTR_LAST_HEARD_FROM for timestamps),
	// which are advertised separately rather than derived from m_metrics.
	if (m_want_projection) {
		m_ganglia.extraProjectionRefs(m_projection_references);
		m_prometheus.extraProjectionRefs(m_projection_references);
	}
}

Metric *
MetricD::newMetric(Metric const * /*unused*/ )
{
	// We should never get here, since MetricD itself doesn't publish any metrics; it just delegates to its backends (ganglia, prometheus, etc).
	// So if we do get get here, EXCEPT to alert the developer that they forgot to override this function in a backend.
	EXCEPT("MetricD::newMetric() called, but MetricD doesn't define any metrics. This should have been overridden by a backend (e.g. GangliaD or PrometheusD).");
	return nullptr;
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
