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

#include <list>

class Metric {
public:
	Metric();
	virtual ~Metric() {}

	// Given a metric definition ad and an ad to monitor,
	// evaluate the monitored value and other properties such as
	// name, description, and so on.
	virtual bool evaluateDaemonAd(classad::ClassAd &metric_ad,classad::ClassAd const &daemon_ad,AdTypes daemon_ad_type,int max_verbosity,class StatsD *statsd,ExtArray<MyString> *regex_groups=NULL,char const *regex_attr=NULL);

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
	bool derivative;
	int verbosity;

	enum MetricTypeEnum {
		DOUBLE,
		STRING,
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

	AdTypes daemon; // type of condor daemon this metric applies to

	// True if this metric only looks at slot 1 of the startd
	// (we do this in lieu of a true machine ad)
	bool restrict_slot1;

	classad::Value value; // value to be published for this metric

private:

	// evaluate an expression in the daemon ad.
	// If this is a regex metric, performs substitutions of regex groups
	// into strings that reference them.
	bool evaluate(char const *attr_name,classad::Value &result,classad::ClassAd &metric_ad,classad::ClassAd const &daemon_ad,MetricTypeEnum type,ExtArray<MyString> *regex_groups,char const *regex_attr=NULL);
	bool evaluateOptionalString(char const *attr_name,std::string &result,classad::ClassAd &metric_ad,classad::ClassAd const &daemon_ad,ExtArray<MyString> *regex_groups);
};

/* StatsD: base class for gathering and publishing condor statistics
 */

class StatsD: Service {
 public:
	StatsD();
	~StatsD();

	virtual void initAndReconfig(char const *service_name);

	virtual Metric *newMetric(Metric const *copy_me=NULL) = 0;

	void publishMetrics();

	virtual void publishMetric(Metric const &metric) = 0;

	bool getDaemonIP(std::string const &machine,std::string &result) const;

	std::string const &getDefaultAggregateHost() { return m_default_aggregate_host; }

 protected:
	int m_verbosity;
	int m_stats_pub_interval;
	int m_stats_pub_timer;
	std::list< classad::ClassAd * > m_metrics;
	typedef std::map< std::string, Metric *> AggregateMetricList;
	AggregateMetricList m_aggregate_metrics;
	std::map< std::string,std::string > m_daemon_ips; // map of daemon machine (and name) to IP address
	std::string m_default_aggregate_host;
	unsigned m_schedd_metric_count;
	unsigned m_negotiator_metric_count;
	unsigned m_collector_metric_count;
	unsigned m_startd_metric_count;
	unsigned m_startd_slot1_metric_count;

	unsigned m_derivative_publication_failed;
	unsigned m_non_derivative_publication_failed;
	unsigned m_derivative_publication_succeeded;
	unsigned m_non_derivative_publication_succeeded;
	bool m_warned_about_derivative;

	void ParseMetrics( std::string const &stats_metrics_string, char const *param_name, std::list< classad::ClassAd * > &stats_metrics );
	void ParseMetricsFromFile( std::string const &fname );
	void publishDaemonMetrics(ClassAd *ad);
	void publishAggregateMetrics();
	void addToAggregateValue(Metric const &metric);
	void clearAggregateMetrics();
	void mapDaemonIPs(ClassAdList &daemon_ads,CollectorList &collectors);
};

#endif
