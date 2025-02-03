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

#ifndef __STATSD_H__
#define __STATSD_H__

/*
 * This file defines base classes for gathering information from the
 * collector and publishing to some other monitoring system.
 */

#include <list>
#include <vector>
#include <string>

// Base class defining a metric to be evaluated against ads in the collector
class Metric {
public:
	Metric();
	virtual ~Metric() {}

	// Serialize state in this metric to a string
	std::string serialize() const;

	// Deserialize state from a string or a deserializer stream into this metric
	bool deserialize(const std::string &buf);
	bool deserialize(YourStringDeserializer &in);

	// Given a metric definition ad and an ad to monitor,
	// evaluate the monitored value and other properties such as
	// name, description, and so on.
	virtual bool evaluateDaemonAd(classad::ClassAd &metric_ad,classad::ClassAd const &daemon_ad,int max_verbosity,class StatsD *statsd,std::vector<std::string> *regex_groups=NULL,char const *regex_attr=NULL);

	// Sets result to a string representation of the value to publish.
	// Returns false on failure.
	bool getValueString(std::string &result) const;

	// Returns "metric {name} for {machine}"
	std::string whichMetric() const;

	bool isAggregateMetric() const { return aggregate != NO_AGGREGATE; }

	// This is called to contribute another datapoint to an aggregate
	// metric (e.g. SUM, AVG, MIN, MAX)
	void addToAggregateValue(Metric const &datapoint);

	// This is called after all ads have been processed.  It finalizes
	// the aggregate value so it is ready to be published.
	void convertToNonAggregateValue();

	std::string name;
	std::string title;
	std::string desc;
	std::string units;
	std::string group;
	std::string machine;
	std::string ip;
	std::string cluster;
	bool derivative;
	bool zero_value;
	int verbosity;
	int lifetime;
    double scale;

	enum MetricTypeEnum {
        AUTO,
		STRING,
        INT8,
        UINT8,
        INT16,
        UINT16,
        INT32,
        UINT32,
        FLOAT,
		DOUBLE,
		BOOLEAN
	};
	MetricTypeEnum type;

	enum AggregateFunc {
		NO_AGGREGATE,
		SUM,
		AVG,
		MIN,
		MAX
	};
	AggregateFunc aggregate;
	std::string aggregate_group;
	double sum;
	unsigned long count;

	std::vector<std::string> target_type; // type of condor daemons this metric applies to

	// True if this metric only looks at slot 1 of the startd
	// (we do this in lieu of a true machine ad)
	bool restrict_slot1;

	classad::Value value; // value to be published for this metric

private:

	// evaluate an expression in the daemon ad.
	// If this is a regex metric, performs substitutions of regex groups
	// into strings that reference them.
	bool evaluate(char const *attr_name,classad::Value &result,classad::ClassAd &metric_ad,classad::ClassAd const &daemon_ad,MetricTypeEnum type,std::vector<std::string> *regex_groups,char const *regex_attr=NULL) const;
	bool evaluateOptionalString(char const *attr_name,std::string &result,classad::ClassAd &metric_ad,classad::ClassAd const &daemon_ad,std::vector<std::string> *regex_groups);
};

/* StatsD: base class for gathering and publishing condor statistics
 */

class StatsD: Service {
 public:
	StatsD();
	~StatsD();

	virtual void initAndReconfig(char const *service_name);

	// Allocate a new Metric object.
	// This is done via a virtual function so the class derived from StatsD
	// can create a type derived from Metric.
	virtual Metric *newMetric(Metric const *copy_me=NULL) = 0;

	// Publish a metric.
	virtual void publishMetric(Metric const &metric) = 0;

	// Collect ads from the collector and evaluate all metrics.
	void publishMetrics( int timerID = -1 );

	// Given a machine name or daemon name, return the IP address of it,
	// using information gathered from the collector.
	virtual bool getDaemonIP(std::string const &machine,std::string &result) const;

    // initialize monitored host list
    virtual void initializeHostList() = 0;

    // send heartbeats to hosts
    virtual void sendHeartbeats() = 0;

	// Returns the collector host name.
	std::string const &getDefaultAggregateHost() { return m_default_aggregate_host; }

	// Apply an aggregate function to a data point.
	void addToAggregateValue(Metric const &metric);

	bool publishPerExecuteNodeMetrics() const {return m_per_execute_node_metrics; }
	bool isExecuteOnlyNode(std::string &machine) {return m_execute_only_nodes.count(machine)!=0;}

 protected:
	int m_verbosity;
	std::string m_requirements;
	bool m_per_execute_node_metrics;
	std::set< std::string > m_execute_only_nodes;
	int m_stats_pub_interval;
	int m_stats_heartbeat_interval;
	int m_stats_time_till_pub;
	int m_stats_pub_timer;
	std::list< classad::ClassAd * > m_metrics;
	typedef std::map< std::string, Metric *> AggregateMetricList;
	AggregateMetricList m_aggregate_metrics;
	AggregateMetricList m_previous_aggregate_metrics;
	std::unordered_set< std::string > m_metrics_to_reset_at_startup;
	std::map< std::string,std::string > m_daemon_ips; // map of daemon machine (and name) to IP address
	std::string m_default_aggregate_host;
	classad::ClassAd m_default_metric_ad;
	std::vector<std::string> m_target_types;
	bool m_want_projection;
	classad::References m_projection_references;


	unsigned m_derivative_publication_failed;
	unsigned m_non_derivative_publication_failed;
	unsigned m_derivative_publication_succeeded;
	unsigned m_non_derivative_publication_succeeded;
	bool m_warned_about_derivative;

	std::string m_reset_metrics_filename;  // empty if reset metrics not desired

	std::string m_param_monitor_multiple_collectors;
	std::unordered_set< std::string > m_unresponsive_collectors;

	// Write out file of metrics to reset to zero at startup. Return true on success.
	bool WriteMetricsToReset();

	// Read in file of metrics to reset to zero at daemon startup. Return true on success.
	bool ReadMetricsToReset();

	// Read metric definition ads.  Add them to the list of metric ads.
	void ParseMetrics( std::string const &stats_metrics_string, char const *param_name, std::list< classad::ClassAd * > &stats_metrics );

	// Read metric definitions from a file.
	void ParseMetricsFromFile( std::string const &fname );

	// Evaluate metrics against the provided daemon ad.
	void publishDaemonMetrics(ClassAd *ad);

	// Publish the final value of all aggreate metrics.
	void publishAggregateMetrics();

	// Initialize aggregate metrics.
	void clearAggregateMetrics();

	// Remove all previously parsed metric definitions
	void clearMetricDefinitions();

	// Extract IP addresses from daemon ads.
	void mapDaemonIPs(ClassAdList &daemon_ads);

	// Extract IP addresses of collectors, and set a default aggregate host
	void mapCollectorIPs(CollectorList &collectors, bool reset_mappings);

	// Determine which machines are execute-only nodes
	void determineExecuteNodes(ClassAdList &daemon_ads);

	// Fetch daemon ads from collector(s) - invoked from publishMetrics()
	void getDaemonAds(ClassAdList &daemon_ads);

	// Query a collector to set MONITOR_MULTIPLE_COLLECTORS
	bool getCollectorsToMonitor();

};

#endif
