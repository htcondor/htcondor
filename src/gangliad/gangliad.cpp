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
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "gangliad.h"

char const *
GangliaMetric::gangliaMetricType() const {
	switch( type ) {
	case DOUBLE: return "double";
	case STRING: return "string";
	case BOOLEAN: return "double";
	}
	EXCEPT("Unexpected metric type: %d",type);
	return NULL;
}

int
GangliaMetric::gangliaSlope() const {
	return derivative ? GANGLIA_SLOPE_DERIVATIVE : GANGLIA_SLOPE_UNSPECIFIED;
}

GangliaD::GangliaD():
	m_tmax(600),
	m_dmax(86400),
	m_ganglia_context(NULL),
	m_ganglia_config(NULL),
	m_ganglia_channels(NULL)
{
}

GangliaD::~GangliaD()
{
}

Metric *
GangliaD::newMetric(Metric const *copy_me) {
	if( copy_me ) {
		return new GangliaMetric(*static_cast<GangliaMetric const *>(copy_me));
	}
	return new GangliaMetric();
}

void
GangliaD::initAndReconfig()
{
	std::string ganglia_conf_location;
	param(ganglia_conf_location, "GANGLIA_CONFIG", "/etc/ganglia/gmond.conf");
	int fd;
	if ((fd = safe_open_wrapper_follow(ganglia_conf_location.c_str(), O_RDONLY)) < 0)
	{
		EXCEPT("Cannot open Ganglia configuration file GANGLIA_CONFIG=%s.\n", ganglia_conf_location.c_str());
		return;
	}
	close(fd);

	int rc = ganglia_reconfig(ganglia_conf_location.c_str(),&m_ganglia_context,&m_ganglia_config,&m_ganglia_channels);
	if( rc != 0 ) {
		EXCEPT("Failed to configure ganglia library.");
	}

	StatsD::initAndReconfig("GANGLIA");

	// the interval we tell ganglia is the max time between updates
	m_tmax = m_stats_pub_interval*2;
	// the interval we tell ganglia is the lifetime of the metric
	if( m_tmax*3 < 86400 ) {
		m_dmax = 86400;
	}
	else {
		m_dmax = m_tmax*3;
	}
}

bool
GangliaD::getDaemonIP(std::string const &machine,std::string &result) const
{
	if( machine.find("@")!=std::string::npos ) {

		// The machine name being used for publishing purposes is a
		// daemon name containing '@', so there may be multiple
		// daemons of the same type on the same machine.  We need the
		// IP in the ganglia spoof host string to be unique to each
		// daemon.  Therefore, return the daemon name here rather than
		// the actual IP.  In all other cases, we return the actual
		// IP, because we would like the condor metrics to show up in
		// the same host entry as other ganglia metrics.

		result = machine;
		return true;
	}
	return StatsD::getDaemonIP(machine,result);
}


void
GangliaD::publishMetric(Metric const &m)
{
	GangliaMetric const &metric = *static_cast<GangliaMetric const *>(&m);

	if( metric.derivative &&
		m_derivative_publication_failed &&
		!m_derivative_publication_succeeded &&
		m_non_derivative_publication_succeeded )
	{
		// it appears that this version of ganglia does not handle the derivative option
		if( !m_warned_about_derivative ) {
			m_warned_about_derivative = true;
			dprintf(D_ALWAYS,"Not publishing further metrics that require derivatives, because this version of ganglia must not support it.\n");
		}
		return;
	}

	std::string spoof_host;
	if( !metric.machine.empty() ) {
		// Tell ganglia which host this metric belongs to.
		// The format is IP:host.  By experimenting, I have determined that the IP
		// is used as a key to look up the host in ganglia's existing host entries,
		// so even if a new host name is specified, if the IP is already associated
		// with some other host, the metrics will be associated with the other host.
		// Ganglia does not seem to care if the IP is an actual IP address.

		if( !metric.ip.empty() ) {
			spoof_host = metric.ip;
		}
		else {
			spoof_host = metric.machine;
		}
		spoof_host += ":";

		// The web frontend gets messed up if the host part contains
		// unexpected characters such as '@', so we sanitize it here.
		char const *ch;
		for(ch = metric.machine.c_str(); *ch; ch++) {
			if( isalnum(*ch) || *ch == '-' || *ch == '_' ) {
				spoof_host += *ch;
			}
			else {
				spoof_host += ".";
			}
		}
	}

	std::string value;
	if( !metric.getValueString(value) ) {
		return;
	}
	if( value.empty() ) {
		dprintf(D_FULLDEBUG,"No value for %s\n",metric.whichMetric().c_str());
		return;
	}

	int slope = metric.gangliaSlope();

	dprintf(D_FULLDEBUG,"publishing %s=%s, group=%s, units=%s, derivative=%d, type=%s, title=%s, desc=%s, spoof_host=%s\n",
			metric.name.c_str(), value.c_str(), metric.group.c_str(),  metric.units.c_str(), metric.derivative, metric.gangliaMetricType(), metric.title.c_str(), metric.desc.c_str(), spoof_host.c_str());

	int rc = ganglia_send(
						  m_ganglia_context,
						  m_ganglia_channels,
						  metric.group.c_str(),
						  metric.name.c_str(),
						  value.c_str(),
						  metric.gangliaMetricType(),
						  metric.units.c_str(),
						  slope,
						  metric.title.c_str(),
						  metric.desc.c_str(),
						  spoof_host.c_str(),
						  m_tmax,
						  m_dmax);

	if( rc != 0 ) {
		dprintf(D_ALWAYS,"Failed to publish %s%s\n",
				metric.derivative ? "derivative of " : "",
				metric.whichMetric().c_str());
		if( metric.derivative ) {
			m_derivative_publication_failed += 1;
		}
		else {
			m_non_derivative_publication_failed += 1;
		}
	}
	else {
		if( metric.derivative ) {
			m_derivative_publication_succeeded += 1;
		}
		else {
			m_non_derivative_publication_succeeded += 1;
		}
	}
}
