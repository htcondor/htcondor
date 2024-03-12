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
#include "directory.h"
#include "dc_collector.h"
#include "statsd.h"

#include <memory>
#include <fstream>
#include <sstream>

#define ATTR_REGEX  "Regex"
#define ATTR_VERBOSITY "Verbosity"
#define ATTR_TITLE  "Title"
#define ATTR_GROUP  "Group"
#define ATTR_CLUSTER "Cluster"
#define ATTR_DESC   "Desc"
#define ATTR_UNITS  "Units"
#define ATTR_DERIVATIVE "Derivative"
#define ATTR_VALUE  "Value"
#define ATTR_TYPE   "Type"
#define ATTR_AGGREGATE "Aggregate"
#define ATTR_AGGREGATE_GROUP "AggregateGroup"
#define ATTR_IP "IP"
#define ATTR_SCALE "Scale"
#define ATTR_LIFETIME "Lifetime"

Metric::Metric():
	derivative(false),
	verbosity(0),
	lifetime(-1),
    scale(1.0),
	type(AUTO),
	aggregate(NO_AGGREGATE),
	sum(0),
	count(0),
	restrict_slot1(false)
{
}

std::string
Metric::whichMetric() const {
	std::string result;
	formatstr(result,"metric %s",name.c_str());
	if( !machine.empty() ) {
		result += " for ";
		result += machine;
	}
	return result;
}

bool
Metric::evaluate(char const *attr_name,classad::Value &result,classad::ClassAd &metric_ad,classad::ClassAd const &daemon_ad,MetricTypeEnum type,std::vector<std::string> *regex_groups,char const *regex_attr) const
{
	bool retval = true;
	ExprTree *expr = NULL;
	if( !(expr=metric_ad.Lookup(attr_name)) ) {
		return true;
	}
	classad::ClassAd const *ad = &daemon_ad;
	ClassAd daemon_ad_copy;
	if( regex_attr ) {
		// make a modifiable copy of daemon_ad and insert regex_attr = regex_attr, so the user-defined expression can refer to it
		daemon_ad_copy = daemon_ad;
		ad = &daemon_ad_copy;
		daemon_ad_copy.AssignExpr(ATTR_REGEX,regex_attr);
	}
	expr->SetParentScope(ad);
	if( !ad->EvaluateExpr(expr,result) ||
		(type == STRING && !result.IsStringValue()) ||
		(type == DOUBLE && !result.IsNumber()) ||
        (type == FLOAT && !result.IsNumber()) ||
        (type == INT8 && !result.IsIntegerValue()) ||
        (type == UINT8 && !result.IsIntegerValue()) ||
        (type == INT16 && !result.IsIntegerValue()) ||
        (type == UINT16 && !result.IsIntegerValue()) ||
        (type == INT32 && !result.IsIntegerValue()) ||
        (type == UINT32 && !result.IsIntegerValue()) ||
		(type == BOOLEAN && !result.IsBooleanValue()) )
	{
		retval = false;
		classad::ClassAdUnParser unparser;
		std::string expr_str;
		unparser.Unparse(expr_str,expr);
		dprintf(D_FULLDEBUG,"Failed to evaluate the following%s%s: %s=%s\n",
				name.empty() ? "" : " attribute of metric ",
				name.c_str(),
				attr_name,
				expr_str.c_str());
	}
	expr->SetParentScope(&metric_ad);

	// do regex macro substitutions
	if( regex_groups && regex_groups->size() > 0 ) {
		std::string str_value;
		if( result.IsStringValue(str_value) && str_value.find("\\")!=std::string::npos ) {
			std::string new_str_value;
			const char *ch = str_value.c_str();
			while( *ch ) {
				if( *ch == '\\' ) {
					ch++;
					if( !isdigit(*ch) ) {
						new_str_value += *(ch++);
					}
					else {
						char *endptr = NULL;
						long index = strtol(ch,&endptr,10);
						ch = endptr;
						if( index < (ssize_t) regex_groups->size() ) {
							new_str_value += (*regex_groups)[index];
						}
					}
				}
				else {
					new_str_value += *(ch++);
				}
			}
			result.SetStringValue(new_str_value);
		}
	}

	return retval;
}

bool
Metric::evaluateOptionalString(char const *attr_name,std::string &result,classad::ClassAd &metric_ad,classad::ClassAd const &daemon_ad,std::vector<std::string> *regex_groups)
{
	classad::Value val;
	if( !evaluate(attr_name,val,metric_ad,daemon_ad,STRING,regex_groups) ) {
		return false;
	}
	if( val.IsUndefinedValue() ) {
		return true;
	}
	ASSERT(val.IsStringValue(result) );
	return true;
}

bool
Metric::evaluateDaemonAd(classad::ClassAd &metric_ad,classad::ClassAd const &daemon_ad,int max_verbosity,StatsD *statsd,std::vector<std::string> *regex_groups,char const *regex_attr)
{
	if( regex_attr ) {
		name = regex_attr;
	}
	if( !evaluateOptionalString(ATTR_NAME,name,metric_ad,daemon_ad,regex_groups) ) return false;

	metric_ad.EvaluateAttrInt(ATTR_VERBOSITY,verbosity);
	if( verbosity > max_verbosity ) {
		// avoid doing more work; this metric requires higher verbosity
		return false;
	}

	metric_ad.EvaluateAttrInt(ATTR_LIFETIME, lifetime);

	std::string target_type_str;
	if( !evaluateOptionalString(ATTR_TARGET_TYPE,target_type_str,metric_ad,daemon_ad,regex_groups) ) return false;
	target_type.initializeFromString(target_type_str.c_str());
	if( target_type.contains_anycase("machine_slot1") ) {
		restrict_slot1 = true;
	}

	std::string my_type;
	daemon_ad.EvaluateAttrString(ATTR_MY_TYPE,my_type);
	if( !target_type.isEmpty() && !target_type.contains_anycase("any") ) {
		if( restrict_slot1 && !strcasecmp(my_type.c_str(),"machine") ) {
			int slotid = 1;
			daemon_ad.EvaluateAttrInt(ATTR_SLOT_ID,slotid);
			if( slotid != 1 ) {
				return false;
			}
			bool dynamic_slot = false;
			(void) daemon_ad.EvaluateAttrBool(ATTR_SLOT_DYNAMIC,dynamic_slot);
			if( dynamic_slot ) {
				return false;
			}
		}
		else if( !target_type.contains_anycase(my_type.c_str()) ) {
			// avoid doing more work; this is not the right type of daemon ad
			return false;
		}
	}

	classad::Value requirements_val;
	requirements_val.SetBooleanValue(true);
	if( !evaluate(ATTR_REQUIREMENTS,requirements_val,metric_ad,daemon_ad,BOOLEAN,regex_groups,regex_attr) ) {
		return false;
	}
	bool requirements = true;
	if( !requirements_val.IsBooleanValue(requirements) || requirements!=true ) {
		return false;
	}

	if( !regex_attr ) {
		std::string regex;
		if( !evaluateOptionalString(ATTR_REGEX,regex,metric_ad,daemon_ad,NULL) ) return false;
		if( !regex.empty() ) {
			Regex re;
			int errcode;
			int erroffset=0;
			if( !re.compile(regex.c_str(),&errcode,&erroffset,PCRE2_ANCHORED) ) {
				EXCEPT("Invalid regex %s",regex.c_str());
			}
			for( classad::ClassAd::const_iterator itr = daemon_ad.begin();
				 itr != daemon_ad.end();
				 itr++ )
			{
				std::vector<std::string> the_regex_groups;
				if( re.match(itr->first.c_str(),&the_regex_groups) ) {
					// make a new Metric for this attribute that matched the regex
					std::shared_ptr<Metric> metric(statsd->newMetric());
					metric->evaluateDaemonAd(metric_ad,daemon_ad,max_verbosity,statsd,&the_regex_groups,itr->first.c_str());
				}
			}
			return false;
		}
	}

	std::string aggregate_str;
	if( !evaluateOptionalString(ATTR_AGGREGATE,aggregate_str,metric_ad,daemon_ad,regex_groups) ) return false;
	aggregate = NO_AGGREGATE;
	if( strcasecmp(aggregate_str.c_str(),"sum")==0 ) {
		aggregate = SUM;
	}
	else if( strcasecmp(aggregate_str.c_str(),"avg")==0 ) {
		aggregate = AVG;
	}
	else if( strcasecmp(aggregate_str.c_str(),"min")==0 ) {
		aggregate = MIN;
	}
	else if( strcasecmp(aggregate_str.c_str(),"max")==0 ) {
		aggregate = MAX;
	}
	else if( !aggregate_str.empty() ) {
		EXCEPT("Invalid aggregate function %s",aggregate_str.c_str());
	}

	// set default stats grouping
	if( isAggregateMetric() ) {
		group = "HTCondor Pool";
	}
	else if( !strcasecmp(my_type.c_str(),"scheduler") ) {
		group = "HTCondor Schedd";
	}
	else if( !strcasecmp(my_type.c_str(),"machine") ) {
		group = "HTCondor Startd";
		if( !statsd->publishPerExecuteNodeMetrics() ) {
			return false;
		}
	}
	else if( !strcasecmp(my_type.c_str(),"daemonmaster") ) {
		group = "HTCondor Master";

		if( !statsd->publishPerExecuteNodeMetrics() ) {
			std::string machine_name;
			daemon_ad.EvaluateAttrString(ATTR_MACHINE,machine_name);
			if( statsd->isExecuteOnlyNode(machine_name) ) {
				return false;
			}
		}
	}
	else {
		formatstr(group,"HTCondor %s",my_type.c_str());
	}

	if( isAggregateMetric() ) {
		// This is like GROUP BY in SQL.
		// By default, group by the name of the metric.
		aggregate_group = name;
		if( !evaluateOptionalString(ATTR_AGGREGATE_GROUP,aggregate_group,metric_ad,daemon_ad,regex_groups) ) return false;
	}

	if( !evaluateOptionalString(ATTR_GROUP,group,metric_ad,daemon_ad,regex_groups) ) return false;

	if( !evaluateOptionalString(ATTR_TITLE,title,metric_ad,daemon_ad,regex_groups) ) return false;
	if( !evaluateOptionalString(ATTR_DESC,desc,metric_ad,daemon_ad,regex_groups) ) return false;
	if( !evaluateOptionalString(ATTR_UNITS,units,metric_ad,daemon_ad,regex_groups) ) return false;
	if( !evaluateOptionalString(ATTR_CLUSTER,cluster,metric_ad,daemon_ad,regex_groups) ) return false;

	metric_ad.EvaluateAttrBool(ATTR_DERIVATIVE,derivative);
    metric_ad.EvaluateAttrNumber(ATTR_SCALE,scale);

	std::string type_str;
	if( !evaluateOptionalString(ATTR_TYPE,type_str,metric_ad,daemon_ad,regex_groups) ) return false;
	if( type_str.empty() ) {
		type = AUTO;
	}
	else if( strcasecmp(type_str.c_str(),"string")==0 ) {
		type = STRING;
	}
    else if( strcasecmp(type_str.c_str(),"int8")==0 ) {
		type = INT8;
	}
    else if( strcasecmp(type_str.c_str(),"uint8")==0 ) {
		type = UINT8;
	}
    else if( strcasecmp(type_str.c_str(),"int16")==0 ) {
		type = INT16;
	}
    else if( strcasecmp(type_str.c_str(),"uint16")==0 ) {
		type = UINT16;
	}
    else if( strcasecmp(type_str.c_str(),"int32")==0 ) {
		type = INT32;
	}
    else if( strcasecmp(type_str.c_str(),"uint32")==0 ) {
		type = UINT32;
	}
    else if( strcasecmp(type_str.c_str(),"float")==0 ) {
		type = FLOAT;
	}
    else if( strcasecmp(type_str.c_str(),"double")==0 ) {
		type = DOUBLE;
	}
	else if( strcasecmp(type_str.c_str(),"boolean")==0 ) {
		type = BOOLEAN;
	}
	else {
		EXCEPT("Invalid metric attribute type=%s for %s",type_str.c_str(),whichMetric().c_str());
		return false;
	}

	value.SetUndefinedValue();
	if( !evaluate(ATTR_VALUE,value,metric_ad,daemon_ad,type,regex_groups,regex_attr) ) {
		return false;
	}
	if( value.IsUndefinedValue() ) {
		if( regex_attr ) {
			daemon_ad.EvaluateAttr(regex_attr,value);
		}
		else {
			daemon_ad.EvaluateAttr(name,value);
		}
	}
	if( value.IsUndefinedValue() ) {
		return false;
	}
    if ( type == AUTO ) {
        if (value.IsBooleanValue() ) {
            type = BOOLEAN;
        } else if ( value.IsIntegerValue() )  {
            type = INT32;
        } else if ( value.IsNumber() ) {
            type = FLOAT;
        } else if ( value.IsStringValue() ) {
            type = STRING;
        }
    }

	if( isAggregateMetric() ) {
		machine = statsd->getDefaultAggregateHost();
	}
	else {
		if( (!strcasecmp(my_type.c_str(),"machine") && restrict_slot1) || !strcasecmp(my_type.c_str(),"collector") ) {
			// for STARTD_SLOT1 metrics, advertise by default as host.name, not slot1.host.name
			// ditto for the collector, which typically has a daemon name != machine name, even though there is only one collector
			daemon_ad.EvaluateAttrString(ATTR_MACHINE,machine);
		}
		else if( !strcasecmp(my_type.c_str(),"submitter") ) {
			daemon_ad.EvaluateAttrString(ATTR_SCHEDD_NAME,machine);
		}
		else if( !strcasecmp(my_type.c_str(),"accounting") ) {
			daemon_ad.EvaluateAttrString(ATTR_NEGOTIATOR_NAME,machine);
		}
		else {
			// use the daemon name for the metric machine name
			daemon_ad.EvaluateAttrString(ATTR_NAME,machine);
		}
	}
	if( !evaluateOptionalString(ATTR_MACHINE,machine,metric_ad,daemon_ad,regex_groups) ) return false;

	statsd->getDaemonIP(machine,ip);
	if( !evaluateOptionalString(ATTR_IP,ip,metric_ad,daemon_ad,regex_groups) ) return false;

	if( isAggregateMetric() ) {
		statsd->addToAggregateValue(*this);
	}
	else {
		statsd->publishMetric(*this);
	}
	return true;
}

bool
Metric::getValueString(std::string &result) const {
	switch( type ) {
        case FLOAT:
		case DOUBLE: {
			double dbl = 0.0;
			if( value.IsNumber(dbl) ) {
                dbl *= scale;
				formatstr(result,"%g",dbl);
				return true;
			}
			break;
		}
		case STRING: {
			if( value.IsStringValue(result) ) {
				return true;
			}
			break;
		}
        case INT8:
        case UINT8:
        case INT16:
        case UINT16:
        case INT32:
        case UINT32: {
            int i = 0;
            if( value.IsIntegerValue(i) ) {
                i *= scale;
                formatstr(result,"%d",i);
                return true;
            }
            break;
        }
		case BOOLEAN: {
			bool v = false;
			if( value.IsBooleanValue(v) ) {
				formatstr(result,"%d",v==true);
				return true;
			}
			break;
		}
		case AUTO: {
			// Shouldn't happen
			break;
		}
	}
	std::stringstream value_str;
	classad::Value v = value;
	value_str << v;
	dprintf(D_ALWAYS,"Invalid value %s=%s\n",name.c_str(),value_str.str().c_str());
	return false;
}

void
Metric::addToAggregateValue(Metric const &datapoint) {

	switch(aggregate) {
		case NO_AGGREGATE:
			EXCEPT("addToAggregateValue() called when aggregate function was not defined");
			break;
		case SUM: {
			double n=0;
			if( datapoint.value.IsNumber(n) ) {
				sum += n;
			}
			break;
		}
		case AVG: {
			double n=0;
			if( datapoint.value.IsNumber(n) ) {
				count += 1;
				sum += n;
			}
			break;
		}
		case MIN: {
			double n=0;
			if( datapoint.value.IsNumber(n) ) {
				if( count == 0 || n < sum ) {
					sum = n;
				}
				count += 1;
			}
			break;
		}
		case MAX: {
			double n=0;
			if( datapoint.value.IsNumber(n) ) {
				if( count == 0 || n > sum ) {
					sum = n;
				}
				count += 1;
			}
			break;
		}
	}
}

void
Metric::convertToNonAggregateValue() {
	switch(aggregate) {
		case NO_AGGREGATE:
			break;
		case MIN:
		case MAX:
		case SUM: {
            if (type == FLOAT || type == DOUBLE) {
                value.SetRealValue(sum);
            } else {
                value.SetIntegerValue((int)sum);
            }
			break;
		}
		case AVG: {
			if( count != 0 ) {
                if (type == FLOAT || type == DOUBLE) {
                    value.SetRealValue(sum/count);
                } else {
                    value.SetIntegerValue((int)(sum/count));
                }
			}
			else {
				value.SetErrorValue();
			}
			break;
		}
	}
	aggregate = NO_AGGREGATE;
}

StatsD::StatsD():
	m_verbosity(0),
	m_per_execute_node_metrics(true),
	m_stats_pub_interval(0),
	m_stats_heartbeat_interval(20),
	m_stats_time_till_pub(0),
	m_stats_pub_timer(-1),
	m_derivative_publication_failed(0),
	m_non_derivative_publication_failed(0),
	m_derivative_publication_succeeded(0),
	m_non_derivative_publication_succeeded(0),
	m_warned_about_derivative(false)
{
}

StatsD::~StatsD()
{
	clearMetricDefinitions();
	clearAggregateMetrics();
}

void
StatsD::initAndReconfig(char const *service_name)
{
	std::string param_name;

	ASSERT( service_name );

	m_default_metric_ad.Clear();

	int old_stats_pub_interval = m_stats_pub_interval;
	formatstr(param_name,"%s_INTERVAL",service_name);
	m_stats_pub_interval = param_integer(param_name.c_str(),60);
	if( m_stats_pub_interval < 0 ) {
		dprintf(D_ALWAYS,
				"%s is less than 0, so no stats publications will be made.\n",
				param_name.c_str());
		if( m_stats_pub_timer != -1 ) {
			daemonCore->Cancel_Timer(m_stats_pub_timer);
			m_stats_pub_timer = -1;
		}
	}
	else if( m_stats_pub_timer >= 0 ) {
		if( old_stats_pub_interval != m_stats_pub_interval ) {
            m_stats_time_till_pub = m_stats_time_till_pub + (m_stats_pub_interval - old_stats_pub_interval );
		}
	}
	else {
		m_stats_heartbeat_interval = MIN(m_stats_pub_interval,m_stats_heartbeat_interval);
		m_stats_pub_timer = daemonCore->Register_Timer(
			m_stats_heartbeat_interval,
			m_stats_heartbeat_interval,
			(TimerHandlercpp)&StatsD::publishMetrics,
			"Statsd::publishMetrics",
			this );
	}
	if( old_stats_pub_interval != m_stats_pub_interval && m_stats_pub_interval > 0 )
	{
		dprintf(D_ALWAYS,
				"Will perform stats publication every %s=%d "
				"seconds.\n", param_name.c_str(),m_stats_pub_interval);
	}

	formatstr(param_name,"%s_VERBOSITY",service_name);
	m_verbosity = param_integer(param_name.c_str(),0);

	formatstr(param_name,"%s_REQUIREMENTS",service_name);
	param(m_requirements,param_name.c_str());

	formatstr(param_name,"%s_PER_EXECUTE_NODE_METRICS",service_name);
	m_per_execute_node_metrics = param_boolean(param_name.c_str(),true);

	formatstr(param_name,"%s_DEFAULT_CLUSTER",service_name);
	std::string default_cluster_expr;
	param(default_cluster_expr,param_name.c_str());

	if( !default_cluster_expr.empty() ) {
		classad::ClassAdParser parser;
		classad::ExprTree *expr=NULL;
		if( !parser.ParseExpression(default_cluster_expr,expr,true) || !expr ) {
			EXCEPT("Invalid %s=%s",param_name.c_str(),default_cluster_expr.c_str());
		}
		// The classad takes ownership of expr
		m_default_metric_ad.Insert(ATTR_CLUSTER,expr);
	}

	formatstr(param_name,"%s_DEFAULT_MACHINE",service_name);
	std::string default_machine_expr;
	param(default_machine_expr,param_name.c_str());

	if( !default_machine_expr.empty() ) {
		classad::ClassAdParser parser;
		classad::ExprTree *expr=NULL;
		if( !parser.ParseExpression(default_machine_expr,expr,true) || !expr ) {
			EXCEPT("Invalid %s=%s",param_name.c_str(),default_machine_expr.c_str());
		}
		// The classad takes ownership of expr
		m_default_metric_ad.Insert(ATTR_MACHINE,expr);
	}

	formatstr(param_name,"%s_DEFAULT_IP",service_name);
	std::string default_ip_expr;
	param(default_ip_expr,param_name.c_str());

	if( !default_ip_expr.empty() ) {
		classad::ClassAdParser parser;
		classad::ExprTree *expr=NULL;
		if( !parser.ParseExpression(default_ip_expr,expr,true) || !expr ) {
			EXCEPT("Invalid %s=%s",param_name.c_str(),default_ip_expr.c_str());
		}
		// The classad takes ownership of expr
		m_default_metric_ad.Insert(ATTR_IP,expr);
	}

	clearMetricDefinitions();
	std::string config_dir;
	formatstr(param_name,"%s_METRICS_CONFIG_DIR",service_name);
	param(config_dir,param_name.c_str());
	if( !config_dir.empty() ) {
		std::vector<std::string> file_list;
		if( !get_config_dir_file_list( config_dir.c_str(), file_list ) ) {
			EXCEPT("Failed to read metric configuration from %s",config_dir.c_str());
		}

		for (auto& fname: file_list) {
			dprintf(D_ALWAYS,"Reading metric definitions from %s\n",fname.c_str());
			ParseMetricsFromFile(fname.c_str());
		}
	}
}

void
StatsD::ParseMetricsFromFile( std::string const &fname )
{
	std::ifstream F(fname.c_str(), std::ios::in | std::ios::binary);
	if(!F) {
		EXCEPT("Failed to open %s: %s",fname.c_str(),strerror(errno));
	}

	std::string contents;
	F.seekg(0, std::ios::end);
	contents.resize(F.tellg());
	F.seekg(0, std::ios::beg);
	F.read(&contents[0], contents.size());
	if(!F) {
		EXCEPT("Failed to read %s: %s",fname.c_str(),strerror(errno));
	}
	F.close();

	ParseMetrics( contents, fname.c_str(), m_metrics );
}

void
StatsD::ParseMetrics( std::string const &stats_metrics_string, char const *param_name, std::list< classad::ClassAd * > &stats_metrics ) {

		// Parse a list of metrics.  The expected syntax is
		// a list of ClassAds, optionally delimited by commas and or
		// whitespace.

	int offset = 0;
	while(1) {
		if(offset >= (int)stats_metrics_string.size()) break;

		int this_offset = offset; //save offset before eating an ad.

		std::string error_msg;

		classad::ClassAdParser parser;
		classad::ClassAd *ad = new classad::ClassAd;
		bool failed = false;
		if(!parser.ParseClassAd(stats_metrics_string,*ad,offset)) {
			int final_offset = this_offset;
			std::string final_stats_metrics_string = stats_metrics_string;
			final_stats_metrics_string += "\n[]"; // add an empty ClassAd

			if(parser.ParseClassAd(final_stats_metrics_string,*ad,final_offset)) {
					// There must have been some trailing whitespace or
					// comments after the last ClassAd, so the only reason
					// ParseClassAd() failed was because there was no ad.
					// Therefore, we are done.
				delete ad;
				break;
			}
			failed = true;
		}

		if( failed ) {
			EXCEPT("CONFIGURATION ERROR: error in metrics defined in %s: %s, for entry starting here: %.80s",
				   param_name,error_msg.c_str(),stats_metrics_string.c_str() + this_offset);
		}

		classad::ClassAd *ad2 = new ClassAd(m_default_metric_ad);
		ad2->Update(*ad);
		delete ad;
		ad = ad2;

		int verbosity = 0;
		ad->EvaluateAttrInt(ATTR_VERBOSITY,verbosity);
		if( verbosity > m_verbosity ) {
			delete ad;
			continue;
		}

		// for efficient queries to the collector, keep track of
		// which type of ads we need
		std::string target_type;
		ad->EvaluateAttrString(ATTR_TARGET_TYPE,target_type);
		if( target_type.empty() ) {
			classad::ClassAdUnParser unparser;
			std::string ad_str;
			unparser.Unparse(ad_str,ad);
			EXCEPT("CONFIGURATION ERROR: no target type specified for metric defined in %s: %s",
				   param_name,
				   ad_str.c_str());
		}
		StringList target_types(target_type.c_str());
		m_target_types.create_union(target_types,true);

		stats_metrics.push_back(ad);
	}
}

void
StatsD::publishMetrics( int /* timerID */ )
{
	dprintf(D_ALWAYS,"Starting update...\n");

    double start_time = condor_gettimestamp_double();

    m_stats_time_till_pub -= m_stats_heartbeat_interval;

    if (m_stats_time_till_pub > 0) {
        sendHeartbeats();
        return;
    }

    m_stats_time_till_pub = m_stats_pub_interval;

    initializeHostList();

	// reset all aggregate sums, counts, etc.
	clearAggregateMetrics();

	ClassAdList daemon_ads;
	CondorQuery query(ANY_AD);

	if( !m_target_types.contains_anycase("any") ) {
		if( !m_requirements.empty() ) {
			query.addANDConstraint(m_requirements.c_str());
		}
	}
	else {
		char const *target_type;
		while( (target_type=m_target_types.next()) ) {
			std::string constraint;
			if( !strcasecmp(target_type,"machine_slot1") ) {
				formatstr(constraint,"MyType == \"Machine\" && SlotID==1 && DynamicSlot =!= True");
			}
			else {
				formatstr(constraint,"MyType == \"%s\"",target_type);
			}
			if( !m_requirements.empty() ) {
				constraint += " && (";
				constraint += m_requirements;
				constraint += ")";
			}
			query.addORConstraint(constraint.c_str());
		}
	}

	CollectorList* collectors = daemonCore->getCollectorList();
	ASSERT( collectors );

	QueryResult result;
	result = collectors->query(query,daemon_ads);
	if( result != Q_OK ) {
		dprintf(D_ALWAYS,
				"Couldn't fetch schedd ads: %s\n",
				getStrQueryResult(result));
		return;
	}

	dprintf(D_ALWAYS,"Got %d daemon ads\n",daemon_ads.MyLength());

	mapDaemonIPs(daemon_ads,*collectors);

	if( !m_per_execute_node_metrics ) {
		determineExecuteNodes(daemon_ads);
	}

	daemon_ads.Open();
	ClassAd *daemon;
	while( (daemon=daemon_ads.Next()) ) {
		publishDaemonMetrics(daemon);
	}
	daemon_ads.Close();

	publishAggregateMetrics();

    sendHeartbeats();

    // Did we take longer than a heartbeat period?
    int heartbeats_missed = (int)(condor_gettimestamp_double() - start_time) /
                            m_stats_heartbeat_interval;
    if (heartbeats_missed) {
        dprintf(D_ALWAYS, "Skipping %d heartbeats\n", heartbeats_missed);
        m_stats_time_till_pub -= (heartbeats_missed * m_stats_heartbeat_interval);
    }
}

void
StatsD::determineExecuteNodes(ClassAdList &daemon_ads) {
	std::set< std::string > submit_nodes;
	std::set< std::string > execute_nodes;
	std::set< std::string > cm_nodes;

	daemon_ads.Open();
	ClassAd *daemon;
	while( (daemon=daemon_ads.Next()) ) {
		std::string machine,my_type;
		daemon->EvaluateAttrString(ATTR_MACHINE,machine);
		daemon->EvaluateAttrString(ATTR_MY_TYPE,my_type);
		if( strcasecmp(my_type.c_str(),"machine")==0 ) {
			execute_nodes.insert( std::set< std::string >::value_type(machine) );
		}
		else if( strcasecmp(my_type.c_str(),"scheduler")==0 ) {
			submit_nodes.insert( std::set< std::string >::value_type(machine) );
		}
		else if( strcasecmp(my_type.c_str(),"negotiator")==0 || strcasecmp(my_type.c_str(),"collector")==0 ) {
			cm_nodes.insert( std::set< std::string >::value_type(machine) );
		}
	}
	daemon_ads.Close();

	m_execute_only_nodes.clear();
	for( std::set< std::string >::iterator itr = execute_nodes.begin();
		 itr != execute_nodes.end();
		 itr++ )
	{
		if( !submit_nodes.count(*itr) && !cm_nodes.count(*itr) ) {
			m_execute_only_nodes.insert(*itr);
		}
	}
	if( !m_per_execute_node_metrics && m_execute_only_nodes.size()>0 ) {
		dprintf(D_FULLDEBUG,"Filtering out metrics for %d execute nodes because PER_EXECUTE_NODE_METRICS=False.\n",
				(int)m_execute_only_nodes.size());
	}
}

void
StatsD::publishDaemonMetrics(ClassAd *daemon_ad)
{
	for( std::list< classad::ClassAd * >::iterator itr = m_metrics.begin();
		 itr != m_metrics.end();
		 itr++ )
	{
		std::shared_ptr<Metric> metric(newMetric());
		// This calls publishMetric() (possibly multiple times) or addToAggregateValue()
		metric->evaluateDaemonAd(**itr,*daemon_ad,m_verbosity,this);
	}
}

void
StatsD::publishAggregateMetrics()
{
	for( AggregateMetricList::iterator itr = m_aggregate_metrics.begin();
		 itr != m_aggregate_metrics.end();
		 itr++ )
	{
		Metric *metric = newMetric(itr->second);
		metric->convertToNonAggregateValue();
		publishMetric(*metric);
		delete metric;
	}
}

void
StatsD::clearMetricDefinitions()
{
	for( std::list< classad::ClassAd * >::iterator itr = m_metrics.begin();
		 itr != m_metrics.end();
		 itr++ )
	{
		delete *itr;
	}
	m_metrics.clear();
}

void
StatsD::clearAggregateMetrics()
{
	for( AggregateMetricList::iterator itr = m_aggregate_metrics.begin();
		 itr != m_aggregate_metrics.end();
		 itr++ )
	{
		delete itr->second;
	}
	m_aggregate_metrics.clear();
}

void
StatsD::addToAggregateValue(Metric const &metric) {
	AggregateMetricList::iterator map_item = m_aggregate_metrics.find(metric.aggregate_group);
	if( map_item == m_aggregate_metrics.end() ) {
		map_item = m_aggregate_metrics.insert(AggregateMetricList::value_type(metric.aggregate_group,newMetric(&metric))).first;
		ASSERT( map_item != m_aggregate_metrics.end() );
	}
	map_item->second->addToAggregateValue(metric);
}


void
StatsD::mapDaemonIPs(ClassAdList &daemon_ads,CollectorList &collectors) {
	// The map of machines to IPs is used when directing ganglia to
	// associate specific metrics with specific hosts (host spoofing)

	m_daemon_ips.clear();

	daemon_ads.Open();
	ClassAd *daemon;
	while( (daemon=daemon_ads.Next()) ) {
		std::string machine,name,my_address;
		daemon->EvaluateAttrString(ATTR_MACHINE,machine);
		daemon->EvaluateAttrString(ATTR_MACHINE,name);
		daemon->EvaluateAttrString(ATTR_MY_ADDRESS,my_address);
		Sinful s(my_address.c_str());
		if( !s.getHost() ) {
			continue;
		}
		std::string ip = s.getHost();
		if( !machine.empty() ) {
			m_daemon_ips.insert( std::map< std::string,std::string >::value_type(machine,ip) );
		}
		if( !name.empty() ) {
			m_daemon_ips.insert( std::map< std::string,std::string >::value_type(name,ip) );
		}
	}
	daemon_ads.Close();

	// Also add a mapping of collector hosts to IPs, and determine the
	// collector host to use as the default machine name for aggregate
	// metrics.

	m_default_aggregate_host = "";

	for (auto& collector : collectors.getList()) {
		char const *collector_host = collector->fullHostname();
		char const *collector_addr = collector->addr();
		if( collector_host && m_default_aggregate_host.empty() ) {
			m_default_aggregate_host = collector_host;
		}
		if( collector_host && collector_addr ) {
			Sinful s(collector_addr);
			if( s.getHost() ) {
				char const *ip = s.getHost();
				m_daemon_ips.insert( std::map< std::string,std::string >::value_type(collector_host,ip) );
			}
		}
	}
}

bool
StatsD::getDaemonIP(std::string const &machine,std::string &result) const {
	std::map< std::string,std::string >::const_iterator found = m_daemon_ips.find(machine);
	if( found == m_daemon_ips.end() ) {
		return false;
	}
	result = found->second;
	return true;
}
