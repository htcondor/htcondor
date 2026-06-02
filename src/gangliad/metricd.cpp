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

	// Only initialize a backend if at least one parsed metric could publish
	// to it. This keeps a Prometheus-only deployment from ever loading
	// libganglia (whose init EXCEPTs if not installed), and vice versa.
	// Note: we recompute these on every reconfig, so adding a ganglia or
	// prometheus metric to metrics.d and reconfiguring will start invoking
	// the corresponding backend. Once a backend has been activated we leave
	// it activated for the life of the process; removing the last metric
	// for an already-loaded backend won't tear it back down.
	m_ganglia_active    = m_ganglia_active    || hasMetricsForBackend("ganglia");
	m_prometheus_active = m_prometheus_active || hasMetricsForBackend("prometheus");

	if (m_ganglia_active) {
		m_ganglia.initAndReconfig();
	}
	if (m_prometheus_active) {
		m_prometheus.initAndReconfig();
	}

	// MetricD is the only instance that queries the collector, and it parses
	// every metric definition itself (it applies no ExportMetric filter), so
	// its own m_target_types and projection already cover everything every
	// backend needs.  We only need to fold in each backend's extra projection
	// requirements (e.g. PrometheusD wants ATTR_LAST_HEARD_FROM for timestamps),
	// which are advertised separately rather than derived from m_metrics.
	if (m_want_projection) {
		if (m_ganglia_active)    m_ganglia.extraProjectionRefs(m_projection_references);
		if (m_prometheus_active) m_prometheus.extraProjectionRefs(m_projection_references);
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
	if (m_ganglia_active)    m_ganglia.publishMetricsFromAds(daemon_ads);
	if (m_prometheus_active) m_prometheus.publishMetricsFromAds(daemon_ads);
}

void
MetricD::postPublishMetrics()
{
	// Ganglia has no per-cycle flush; Prometheus writes its file here.
	if (m_prometheus_active) m_prometheus.postPublishMetrics();
}

void
MetricD::cleanupOldPreviousValues()
{
	// MetricD itself processes no metrics; the backends do, and each keeps its
	// own previous-value map for computing aggregate-metric derivatives.  Forward
	// the end-of-cycle rotation to them so those maps are populated for the next
	// cycle (otherwise aggregate derivative metrics never see a previous value).
	if (m_ganglia_active)    m_ganglia.cleanupOldPreviousValues();
	if (m_prometheus_active) m_prometheus.cleanupOldPreviousValues();
}

void
MetricD::initializeHostList()
{
	if (m_ganglia_active) m_ganglia.initializeHostList();
}

void
MetricD::sendHeartbeats()
{
	if (m_ganglia_active) m_ganglia.sendHeartbeats();
}
