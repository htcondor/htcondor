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
#include <condor_daemon_core.h>
#include "my_popen.h"
#include "directory.h"
#include "gangliad.h"

char const *
GangliaMetric::gangliaMetricType() const {
	switch( type ) {
    case AUTO: return "auto";
    case FLOAT: return "float";
	case DOUBLE: return "double";
	case STRING: return "string";
	case BOOLEAN: return "int8";
    case INT8: return "int8";
    case UINT8: return "uint8";
    case INT16: return "int16";
    case UINT16: return "uint16";
    case INT32: return "int32";
    case UINT32: return "uint32";
	}
	EXCEPT("Unexpected metric type: %d",type);
	return NULL;
}

int
GangliaMetric::gangliaSlope() const {
	return derivative ? GANGLIA_SLOPE_DERIVATIVE : GANGLIA_SLOPE_BOTH;
}

GangliaD::GangliaD():
	m_tmax(600),
	m_dmax(86400),
	m_ganglia_context(NULL),
	m_ganglia_config(NULL),
	m_ganglia_channels(NULL),
	m_ganglia_noop(0),
    m_gstat_argv(NULL),
    m_send_data_for_all_hosts(false),
    m_ganglia_metrics_sent(0)
{
}

GangliaD::~GangliaD()
{
	ganglia_config_destroy(&m_ganglia_context,&m_ganglia_config,&m_ganglia_channels);
	deleteStringArray(m_gstat_argv);
}

Metric *
GangliaD::newMetric(Metric const *copy_me) {
	if( copy_me ) {
		return new GangliaMetric(*static_cast<GangliaMetric const *>(copy_me));
	}
	return new GangliaMetric();
}

static bool
locateSharedLib(const std::string& libpath,std::string libname,std::string &result)
{
	for (const auto& path: StringTokenIterator(libpath)) {
		Directory d(path.c_str());
		d.Rewind();
		char const *fname;
		while( (fname=d.Next()) ) {
			if( d.IsDirectory() ) continue;
			if( strncmp(fname,libname.c_str(),libname.size())==0 ) {
				size_t l = strlen(fname);
				if( l >= 2 && strcmp(&(fname[l-2]),".a")==0 ) {
					continue; // ignore this; it is a static lib
				}
				result = d.GetFullPath();
				return true;
			}
		}
	}
	return false;
}

void
GangliaD::initAndReconfig(const char * /*unused */)
{
	std::string libname;
	std::string gmetric_path;
	param(libname,"GANGLIA_LIB");
	param(gmetric_path,"GANGLIA_GMETRIC");
	if( libname.empty() && gmetric_path.empty()) {
		std::string libpath;
		char const *libpath_param = "GANGLIA_LIB_PATH";
		if( __WORDSIZE == 64 ) {
			libpath_param = "GANGLIA_LIB64_PATH";
		}
		param(libpath,libpath_param);

		dprintf(D_FULLDEBUG,"Searching for libganglia in %s=%s\n",libpath_param,libpath.c_str());
		locateSharedLib(libpath,"libganglia",libname);

		gmetric_path = "gmetric";

		if( libname.empty() && gmetric_path.empty()) {
			EXCEPT("libganglia was not found via %s, and GANGLIA_LIB is not configured, "
				   "and GANGLIA_GMETRIC is not configured.  "
				   "Ensure that libganglia is installed in a location included in %s "
				   "or configure GANGLIA_LIB and/or GANGLIA_GMETRIC.",
				   libpath_param, libpath_param);
		}
	}
	m_ganglia_noop = false;
	if( libname == "NOOP" ) {
		dprintf(D_ALWAYS,"GANGLIA_LIB=NOOP, so we will go through the motions, but not actually interact with ganglia\n");
		m_ganglia_noop = true;
	}

	if( !m_ganglia_noop ) {
		bool gmetric_initialized = false;
		bool libganglia_initialized = false;
		if( !gmetric_path.empty() ) {
			dprintf(D_ALWAYS,"Testing %s\n",gmetric_path.c_str());
			if( ganglia_init_gmetric(gmetric_path.c_str()) ) {
				gmetric_initialized = true;
			}
		}
		if( !libname.empty() ) {
			if( libname == m_ganglia_libname ) {
				libganglia_initialized = true;
				dprintf(D_ALWAYS,"Already loaded libganglia %s\n",libname.c_str());
				// I have observed instabilities when reloading the library, so
				// it is best to not do that unless it is really necessary.
			}
			else {
				dprintf(D_ALWAYS,"Loading libganglia %s\n",libname.c_str());
				ganglia_config_destroy(&m_ganglia_context,&m_ganglia_config,&m_ganglia_channels);
				if( ganglia_load_library(libname.c_str()) ) {
					libganglia_initialized = true;
					m_ganglia_libname = libname;
				}
				else if( gmetric_initialized ) {
					dprintf(D_ALWAYS,"WARNING: failed to load %s, so gmetric (which is slower) will be used instead.\n",libname.c_str());
				}
			}
		}

		if( libganglia_initialized ) {
			dprintf(D_ALWAYS,"Will use libganglia to interact with ganglia.\n");
		}
		else if( gmetric_initialized ) {
			dprintf(D_ALWAYS,"Will use gmetric to interact with ganglia.\n");
		}
		else {
			EXCEPT("Neither gmetric nor libganglia were successfully initialized.  Aborting");
		}

		std::string ganglia_conf_location;
		param(ganglia_conf_location, "GANGLIA_CONFIG", "/etc/ganglia/gmond.conf");
		int fd;
		if ((fd = safe_open_wrapper_follow(ganglia_conf_location.c_str(), O_RDONLY)) < 0)
		{
			EXCEPT("Cannot open Ganglia configuration file GANGLIA_CONFIG=%s.", ganglia_conf_location.c_str());
			return;
		}
		close(fd);

		if( !ganglia_reconfig(ganglia_conf_location.c_str(),&m_ganglia_context,&m_ganglia_config,&m_ganglia_channels) ) {
			EXCEPT("Failed to configure ganglia library.");
		}
	}

	param(m_gstat_command, "GANGLIA_GSTAT_COMMAND");
	deleteStringArray(m_gstat_argv);
    split_args(m_gstat_command.c_str(), &m_gstat_argv);

    m_send_data_for_all_hosts = param_boolean("GANGLIA_SEND_DATA_FOR_ALL_HOSTS", false);

	StatsD::initAndReconfig("GANGLIAD");

	// the interval we tell ganglia is the max time between updates
	m_tmax = m_stats_pub_interval*2;
	// the minimum dmax can be
	int min_dmax = param_integer("GANGLIAD_MIN_METRIC_LIFETIME", 86400);
	if(min_dmax < 0) { min_dmax = 86400; }
	dprintf(D_ALWAYS,"Setting minimum calculated DMAX value to %d. Specified metric lifetimes with override this value.\n", min_dmax);
	// the interval we tell ganglia is the lifetime of the metric
	if( m_tmax*3 < (unsigned int)min_dmax ) {
		m_dmax = min_dmax;
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
GangliaD::initializeHostList()
{
    m_monitored_hosts.clear();
    m_need_heartbeat.clear();
    m_ganglia_metrics_sent = 0;

	if (m_send_data_for_all_hosts) {
		// If we are sending data for all hosts, might as well bail out here, since
		// we don't need the list of monitored hosts in this case.
		return;
	}

	FILE *fp = my_popenv(m_gstat_argv,"r",MY_POPEN_OPT_WANT_STDERR);
	if( !fp ) {
		dprintf(D_ALWAYS,"Failed to execute %s: %s\n",m_gstat_command.c_str(),strerror(errno));
		return;
	}

	char line[1024];
	while( fgets(line,sizeof(line),fp) ) {
        if (char *colon = strchr(line, ':')) {
            *colon = 0;
            // if number of CPUs > 0, this host is monitored by ganglia
            if (atoi(colon + 1) > 0) {
                m_monitored_hosts.insert(line);
            }
        }
    }
    my_pclose(fp);
    dprintf(D_ALWAYS, "Ganglia is monitoring %zu hosts\n", m_monitored_hosts.size());
}

void
GangliaD::sendHeartbeats()
{
    if (m_ganglia_metrics_sent) {
        dprintf(D_ALWAYS, "Ganglia metrics sent: %d\n", m_ganglia_metrics_sent);
        m_ganglia_metrics_sent = 0;
    }

    int heartbeats_sent = 0;
	for( std::set< std::string >::iterator itr = m_need_heartbeat.begin();
		 itr != m_need_heartbeat.end();
		 itr++ )
	{
		if (!m_ganglia_noop) {
        	ganglia_send_heartbeat(m_ganglia_context, m_ganglia_channels, itr->c_str());
		}
        heartbeats_sent++;
	}
    dprintf(D_ALWAYS, "Heartbeats sent: %d\n", heartbeats_sent);
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
        
        if (m_monitored_hosts.find(metric.machine) == m_monitored_hosts.end()) {
            if (m_send_data_for_all_hosts) {
                m_need_heartbeat.insert(spoof_host);
            } else {
                return;
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

    m_ganglia_metrics_sent++;
	dprintf(D_FULLDEBUG,"%spublishing %s=%s, machine=%s <%s>, group=%s, units=%s, derivative=%d, type=%s, title=%s, desc=%s, cluster=%s, spoof_host=%s, lifetime=%d\n",
			m_ganglia_noop ? "noop mode: " : "",
			metric.name.c_str(), value.c_str(), metric.machine.c_str(), metric.ip.c_str(), metric.group.c_str(),  metric.units.c_str(), metric.derivative, metric.gangliaMetricType(), metric.title.c_str(),
			metric.desc.c_str(), metric.cluster.c_str(), spoof_host.c_str(), metric.lifetime < 0 ? m_dmax : metric.lifetime);
	if( !m_ganglia_noop ) {
		bool ok = ganglia_send(
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
						  metric.cluster.c_str(),
						  m_tmax,
						  metric.lifetime < 0 ? m_dmax : metric.lifetime);

		if( !ok ) {
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
}
