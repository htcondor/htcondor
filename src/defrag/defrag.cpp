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
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "condor_query.h"
#include "util_lib_proto.h"
#include "dc_startd.h"
#include "get_daemon_name.h"
#include <algorithm>
#include <iterator>
#include "defrag.h"

#include <list>
#include <tuple>
#define LOG_LENGTH 10
class DefragLog {
	public:
		void record_drain( const std::string & name, const std::string & schedule );

		void record_cancel( const std::string & name );

		void write_to_ad( ClassAd * ad ) const;

	private:
		std::list< std::tuple< time_t, std::string, std::string > > drains;
		std::list< std::tuple< time_t, std::string > > cancels;
};

void
DefragLog::record_drain( const std::string & name, const std::string & schedule ) {
	drains.emplace_back( time(NULL), name, schedule );
	if( drains.size() > LOG_LENGTH ) { drains.pop_front(); }
}

void
DefragLog::record_cancel( const std::string & name ) {
	cancels.emplace_back( time(NULL), name );
	if( cancels.size() > LOG_LENGTH ) { cancels.pop_front(); }
}

void
DefragLog::write_to_ad(ClassAd * ad) const {
	std::string buffer;

	std::string list = "{ ";
	for( auto i = drains.crbegin(); i != drains.crend(); ++i ) {
		formatstr( buffer, "[ when = %ld; who = \"%s\"; what = \"%s\" ],",
			std::get<0>(*i), std::get<1>(*i).c_str(), std::get<2>(*i).c_str() );
		list += buffer;
	}
	list[list.length() - 1] = '}';
	ad->AssignExpr( ATTR_RECENT_DRAINS_LIST, list.c_str() );

	list = "{ ";
	for( auto i = cancels.crbegin(); i != cancels.crend(); ++i ) {
		formatstr( buffer, "[ when = %ld; who = \"%s\" ],",
			std::get<0>(*i), std::get<1>(*i).c_str() );
		list += buffer;
	}
	list[list.length() - 1] = '}';
	ad->AssignExpr( ATTR_RECENT_CANCELS_LIST, list.c_str() );
}

DefragLog defrag_log;

// The basic constraint for p-slots that are currently draining.
#define BASE_SLOT_CONSTRAINT "PartitionableSlot && Offline=!=True"
// alternate BASE_SLOT_CONSTRAINT "PartitionableSlot && Offline=!=True && " ATTR_SLOT_BACKFILL " =!= True"
#define BASE_DRAINING_CONSTRAINT BASE_SLOT_CONSTRAINT " && Draining"

Defrag::Defrag():
	m_polling_interval(-1),
	m_polling_timer(-1),
	m_draining_per_hour(0),
	m_draining_per_poll(0),
	m_draining_per_poll_hour(0),
	m_draining_per_poll_day(0),
	m_max_draining(-1),
	m_max_whole_machines(-1),
	m_draining_schedule(DRAIN_GRACEFUL),
	m_last_poll(0),
	m_public_ad_update_interval(-1),
	m_public_ad_update_timer(-1),
	m_whole_machines_arrived(0),
	m_last_whole_machine_arrival(0),
	m_whole_machine_arrival_sum(0),
	m_whole_machine_arrival_mean_squared(0)
	
{
}

Defrag::~Defrag()
{
	stop();
}

void Defrag::init()
{
	m_stats.Init();
	config();
}

void Defrag::config()
{
#ifdef USE_DEFRAG_STATE_FILE
	if( m_last_poll==0 ) {
		loadState();
	}
#endif

	if ( ! param(m_defrag_name,"DEFRAG_NAME")) {
		param(m_defrag_name, "LOCALNAME");
	}
	auto_free_ptr valid_name(build_valid_daemon_name(m_defrag_name.c_str()));
	ASSERT( valid_name );
	m_daemon_name = valid_name.ptr();

	int old_polling_interval = m_polling_interval;
	m_polling_interval = param_integer("DEFRAG_INTERVAL",600);
	if( m_polling_interval <= 0 ) {
		dprintf(D_ALWAYS,
				"DEFRAG_INTERVAL=%d, so no pool defragmentation "
				"will be done.\n", m_polling_interval);
		if( m_polling_timer != -1 ) {
			daemonCore->Cancel_Timer(m_polling_timer);
			m_polling_timer = -1;
		}
	}
	else if( m_polling_timer >= 0 ) {
		if( old_polling_interval != m_polling_interval ) {
			daemonCore->Reset_Timer_Period(
				m_polling_timer,
				m_polling_interval);
		}
	}
	else {
		time_t now = time(NULL);
		time_t first_time = 0;
		if( m_last_poll != 0 && now-m_last_poll < m_polling_interval && m_last_poll <= now ) {
			first_time = m_polling_interval - (now-m_last_poll);
		}
		m_polling_timer = daemonCore->Register_Timer(
			first_time,
			m_polling_interval,
			(TimerHandlercpp)&Defrag::poll,
			"Defrag::poll",
			this );
	}
	if( old_polling_interval != m_polling_interval && m_polling_interval > 0 )
	{
		dprintf(D_ALWAYS,
				"Will evaluate defragmentation policy every DEFRAG_INTERVAL="
				"%d seconds.\n", m_polling_interval);
	}

	m_draining_per_hour = param_double("DEFRAG_DRAINING_MACHINES_PER_HOUR",0,0);

	double rate = m_draining_per_hour/3600.0*m_polling_interval;
	m_draining_per_poll = (int)floor(rate + 0.00001);
	if( m_draining_per_poll < 0 ) m_draining_per_poll = 0;

	double error_per_hour = (rate - m_draining_per_poll)/m_polling_interval*3600.0;
	m_draining_per_poll_hour = (int)floor(error_per_hour + 0.00001);
	if( m_draining_per_hour < 0 || m_polling_interval > 3600 ) {
		m_draining_per_hour = 0;
	}

	double error_per_day = (error_per_hour - m_draining_per_poll_hour)*24.0;
	m_draining_per_poll_day = (int)floor(error_per_day + 0.5);
	if( m_draining_per_poll_day < 0 || m_polling_interval > 3600*24 ) {
		m_draining_per_poll_day = 0;
	}
	dprintf(D_ALWAYS,"polling interval %ds, DEFRAG_DRAINING_MACHINES_PER_HOUR = %f/hour = %d/interval + %d/hour + %d/day\n",
			m_polling_interval,m_draining_per_hour,m_draining_per_poll,
			m_draining_per_poll_hour,m_draining_per_poll_day);

	m_max_draining = param_integer("DEFRAG_MAX_CONCURRENT_DRAINING",-1,-1);
	m_max_whole_machines = param_integer("DEFRAG_MAX_WHOLE_MACHINES",-1,-1);

	param(m_defrag_requirements,"DEFRAG_REQUIREMENTS");
	if ( ! m_defrag_requirements.empty()) {
		validateExpr(m_defrag_requirements.c_str(), "DEFRAG_REQUIREMENTS");
	}

	formatstr(m_drain_by_me_expr, "(" ATTR_DRAIN_REASON " is undefined || " ATTR_DRAIN_REASON " == \"Defrag %s\")",
	          m_daemon_name.c_str());

	if (param(m_draining_start_expr, "DEFRAG_DRAINING_START_EXPR")) {
		validateExpr( m_draining_start_expr.c_str(), "DEFRAG_DRAINING_START_EXPR" );
	}

	ASSERT( param(m_whole_machine_expr,"DEFRAG_WHOLE_MACHINE_EXPR") );
	validateExpr( m_whole_machine_expr.c_str(), "DEFRAG_WHOLE_MACHINE_EXPR" );
	m_whole_machine_expr += " && " BASE_SLOT_CONSTRAINT " && ";
	m_whole_machine_expr += m_drain_by_me_expr;


	param(m_draining_schedule_str,"DEFRAG_DRAINING_SCHEDULE");
	if( m_draining_schedule_str.empty() ) {
		m_draining_schedule = DRAIN_GRACEFUL;
		m_draining_schedule_str = "graceful";
	}
	else {
		m_draining_schedule = getDrainingScheduleNum(m_draining_schedule_str.c_str());
		if( m_draining_schedule < 0 ) {
			EXCEPT("Invalid draining schedule: %s",m_draining_schedule_str.c_str());
		}
	}

	m_drain_attrs.clear();
	// we always need the attributes of a location lookup to send drain or cancel commands
	m_drain_attrs.insert(ATTR_NAME);
	m_drain_attrs.insert(ATTR_STATE);
	m_drain_attrs.insert(ATTR_MACHINE);
	m_drain_attrs.insert(ATTR_VERSION);
	m_drain_attrs.insert(ATTR_PLATFORM); // TODO: get rid if this after Daemon object is fixed to work without it.
	m_drain_attrs.insert(ATTR_MY_ADDRESS);
	m_drain_attrs.insert(ATTR_ADDRESS_V1);
	// also these special attributes to choose draining candidates
	m_drain_attrs.insert(ATTR_EXPECTED_MACHINE_GRACEFUL_DRAINING_COMPLETION);
	m_drain_attrs.insert(ATTR_EXPECTED_MACHINE_QUICK_DRAINING_COMPLETION);
	m_drain_attrs.insert(ATTR_EXPECTED_MACHINE_GRACEFUL_DRAINING_BADPUT);
	m_drain_attrs.insert(ATTR_EXPECTED_MACHINE_QUICK_DRAINING_BADPUT);
	m_drain_attrs.insert(ATTR_DRAIN_REASON);

	auto_free_ptr rank(param("DEFRAG_RANK"));
	m_rank_ad.Delete(ATTR_RANK);
	if (rank) {
		if( !m_rank_ad.AssignExpr(ATTR_RANK,rank) ) {
			EXCEPT("Invalid expression for DEFRAG_RANK: %s", rank.ptr());
		}
		// add rank references to the projection
		GetExprReferences(m_rank_ad.Lookup(ATTR_RANK), m_rank_ad, NULL, &m_drain_attrs);
	}

	int update_interval = param_integer("DEFRAG_UPDATE_INTERVAL", 300);
	if(m_public_ad_update_interval != update_interval) {
		m_public_ad_update_interval = update_interval;

		dprintf(D_FULLDEBUG, "Setting update interval to %d\n",
			m_public_ad_update_interval);

		if(m_public_ad_update_timer >= 0) {
			daemonCore->Reset_Timer_Period(
				m_public_ad_update_timer,
				m_public_ad_update_interval);
		}
		else {
			m_public_ad_update_timer = daemonCore->Register_Timer(
				0,
				m_public_ad_update_interval,
				(TimerHandlercpp)&Defrag::updateCollector,
				"Defrag::updateCollector",
				this);
		}
	}

	if (param(m_cancel_requirements, "DEFRAG_CANCEL_REQUIREMENTS")) {
		validateExpr( m_cancel_requirements.c_str(), "DEFRAG_CANCEL_REQUIREMENTS" );
	} else {
		m_cancel_requirements = "";
	}


	int stats_quantum = m_polling_interval;
	int stats_window = 10*stats_quantum;
	m_stats.SetWindowSize(stats_window,stats_quantum);
}

void Defrag::stop()
{
	if( m_polling_timer != -1 ) {
		daemonCore->Cancel_Timer(m_polling_timer);
		m_polling_timer = -1;
	}
}

static int StartdSortFunc(ClassAd *ad1,ClassAd *ad2,void *data)
{
	ClassAd *rank_ad = (ClassAd *)data;

	float rank1 = 0;
	float rank2 = 0;
	EvalFloat(ATTR_RANK,rank_ad,ad1,rank1);
	EvalFloat(ATTR_RANK,rank_ad,ad2,rank2);

	return rank1 > rank2;
}

void Defrag::validateExpr(char const *constraint,char const *constraint_source)
{
	ExprTree *requirements = NULL;

	if( ParseClassAdRvalExpr( constraint, requirements )!=0 || requirements==NULL )
	{
		EXCEPT("Invalid expression for %s: %s",
			   constraint_source,constraint);
	}
	delete requirements;
}

bool Defrag::queryMachines(char const *constraint,char const *constraint_source,ClassAdList &startdAds, classad::References * projection)
{
	CondorQuery startdQuery(STARTD_AD);

	validateExpr(constraint,constraint_source);
	startdQuery.addANDConstraint(constraint);
	if (projection) {
		startdQuery.setDesiredAttrs(*projection);
	} else {
		// if no projection supplied, just get the Name attribute
		startdQuery.setDesiredAttrs("Name");
	}
	startdQuery.addExtraAttribute(ATTR_SEND_PRIVATE_ATTRIBUTES, "true");

	CollectorList* collects = daemonCore->getCollectorList();
	ASSERT( collects );

	QueryResult result;
	result = collects->query(startdQuery,startdAds);
	if( result != Q_OK ) {
		dprintf(D_ALWAYS,
				"Couldn't fetch startd ads using constraint "
				"%s=%s: %s\n",
				constraint_source,constraint, getStrQueryResult(result));
		return false;
	}

	dprintf(D_FULLDEBUG,"Got %d startd ads matching %s=%s\n",
			startdAds.MyLength(), constraint_source, constraint);

	return true;
}

void
Defrag::queryDrainingCost()
{
	ClassAdList startdAds;
	CondorQuery startdQuery(STARTD_AD);
	char const *desired_attrs[6];
	desired_attrs[0] = ATTR_TOTAL_MACHINE_DRAINING_UNCLAIMED_TIME;
	desired_attrs[1] = ATTR_TOTAL_MACHINE_DRAINING_BADPUT;
	desired_attrs[2] = ATTR_DAEMON_START_TIME;
	desired_attrs[3] = ATTR_TOTAL_CPUS;
	desired_attrs[4] = ATTR_LAST_HEARD_FROM;
	desired_attrs[5] = NULL;

	startdQuery.setDesiredAttrs(desired_attrs);
	std::string query;
	// only want one ad per machine
	formatstr(query,"%s==1 && (%s =!= undefined || %s =!= undefined)",
			ATTR_SLOT_ID,
			ATTR_TOTAL_MACHINE_DRAINING_UNCLAIMED_TIME,
			ATTR_TOTAL_MACHINE_DRAINING_BADPUT);
	startdQuery.addANDConstraint(query.c_str());

	CollectorList* collects = daemonCore->getCollectorList();
	ASSERT( collects );

	QueryResult result;
	result = collects->query(startdQuery,startdAds);
	if( result != Q_OK ) {
		dprintf(D_ALWAYS,
				"Couldn't fetch startd ads: %s\n",
				getStrQueryResult(result));
		return;
	}

	double avg_badput = 0.0;
	double avg_unclaimed = 0.0;
	int total_cpus = 0;

	startdAds.Open();
	ClassAd *startd_ad;
	while( (startd_ad=startdAds.Next()) ) {
		int unclaimed = 0;
		time_t badput = 0;
		time_t start_time = 0;
		int cpus = 0;
		time_t last_heard_from = 0;
		startd_ad->LookupInteger(ATTR_TOTAL_MACHINE_DRAINING_UNCLAIMED_TIME,unclaimed);
		startd_ad->LookupInteger(ATTR_TOTAL_MACHINE_DRAINING_BADPUT,badput);
		startd_ad->LookupInteger(ATTR_DAEMON_START_TIME,start_time);
		startd_ad->LookupInteger(ATTR_LAST_HEARD_FROM,last_heard_from);
		startd_ad->LookupInteger(ATTR_TOTAL_CPUS,cpus);

		time_t age = last_heard_from - start_time;
		if( last_heard_from == 0 || start_time == 0 || age <= 0 ) {
			continue;
		}

		avg_badput += ((double)badput)/age;
		avg_unclaimed += ((double)unclaimed)/age;
		total_cpus += cpus;
	}
	startdAds.Close();

	if( total_cpus > 0 ) {
		avg_badput = avg_badput/total_cpus;
		avg_unclaimed = avg_unclaimed/total_cpus;
	}

	dprintf(D_ALWAYS,"Average pool draining badput = %.2f%%\n",
			avg_badput*100);

	dprintf(D_ALWAYS,"Average pool draining unclaimed = %.2f%%\n",
			avg_unclaimed*100);

	m_stats.AvgDrainingBadput = avg_badput;
	m_stats.AvgDrainingUnclaimed = avg_unclaimed;
}

int Defrag::countMachines(char const *constraint,char const *constraint_source,	MachineSet *machines)
{
	ClassAdList startdAds;
	int count = 0;

	if( !queryMachines(constraint,constraint_source,startdAds,NULL) ) {
		return -1;
	}

	MachineSet my_machines;
	if( !machines ) {
		machines = &my_machines;
	}

	startdAds.Open();
	ClassAd *startd_ad;
	while( (startd_ad=startdAds.Next()) ) {
		std::string machine;
		std::string name;
		startd_ad->LookupString(ATTR_NAME,name);
		slotNameToDaemonName(name,machine);

		if( machines->count(machine) ) {
			continue;
		}

		machines->insert(machine);
		count++;
	}
	startdAds.Close();

	dprintf(D_FULLDEBUG,"Counted %d machines matching %s=%s\n",
			count,constraint_source,constraint);
	return count;
}

#ifdef USE_DEFRAG_STATE_FILE

static char const * const ATTR_LAST_POLL = "LastPoll";
void Defrag::saveState()
{
	ClassAd ad;
	ad.Assign(ATTR_LAST_POLL, m_last_poll);

	std::string new_state_file;
	formatstr(new_state_file,"%s.new",m_state_file.c_str());
	FILE *fp;
	if( !(fp = safe_fopen_wrapper_follow(new_state_file.c_str(), "w")) ) {
		EXCEPT("failed to save state to %s",new_state_file.c_str());
	}
	else {
		fPrintAd(fp, ad);
		fclose( fp );
		if( rotate_file(new_state_file.c_str(),m_state_file.c_str())!=0 ) {
			EXCEPT("failed to save state to %s",m_state_file.c_str());
		}
	}
}

void Defrag::loadState()
{
	if (m_state_file.empty()) {
		if ( ! param(m_state_file,"DEFRAG_STATE_FILE")) {
			auto_free_ptr state_file(expand_param("$(LOCK)/$(LOCALNAME:defrag)_state"));
			m_state_file = state_file.ptr();
		}
	}

	FILE *fp;
	if( !(fp = safe_fopen_wrapper_follow(m_state_file.c_str(), "r")) ) {
		if( errno == ENOENT ) {
			dprintf(D_ALWAYS,"State file %s does not yet exist.\n",m_state_file.c_str());
		}
		else {
			EXCEPT("failed to load state from %s",m_state_file.c_str());
		}
	}
	else {
		int isEOF=0, errorReadingAd=0, adEmpty=0;
		ClassAd *ad = new ClassAd;
		InsertFromFile(fp, *ad, "...", isEOF, errorReadingAd, adEmpty);
		fclose( fp );

		if( errorReadingAd ) {
			dprintf(D_ALWAYS,"WARNING: failed to parse state from %s\n",m_state_file.c_str());
		}

		time_t timestamp = m_last_poll;
		ad->LookupInteger(ATTR_LAST_POLL,timestamp);
		m_last_poll = timestamp;

		dprintf(D_ALWAYS, "Last poll: %lld\n", (long long)m_last_poll);

		delete ad;
	}
}
#endif

void Defrag::slotNameToDaemonName(std::string const &name,std::string &machine)
{
	size_t pos = name.find('@');
	if( pos == std::string::npos ) {
		machine = name;
	}
	else {
		machine.append(name,pos+1,name.size()-pos-1);
	}
}

// n is a number per period.  If we are partly through
// the interval, make n be in proportion to how much
// is left.
static int prorate(int n,time_t period_elapsed,time_t period,time_t granularity, double* real_answer)
{
	time_t time_remaining = period-period_elapsed;
	double frac = ((double)time_remaining)/period;

		// Add in maximum time in this interval that could have been
		// missed due to polling interval (granularity).

	frac += ((double)granularity)/period;

	if (real_answer) { *real_answer = n*frac; }
	int answer = (int)floor(n*frac + 0.5);

	if( (n > 0 && answer > n) || (n < 0 && answer < n) ) {
		return n; // never exceed magnitude of n
	}
	if( answer*n < 0 ) { // never flip sign
		return 0;
	}
	return answer;
}

void Defrag::poll_cancel(MachineSet &cancelled_machines)
{
	if (!m_cancel_requirements.size())
	{
		return;
	}

	// build a constraint out of the basic contraint for matching a draining p-slot
	// that was draind by this defrag and the cancel_requirements expression
	// (which should include the whole-machine constraint)
	std::string draining_constraint(BASE_DRAINING_CONSTRAINT " && ");
	draining_constraint += m_drain_by_me_expr;
	if ( ! m_cancel_requirements.empty()) {
		formatstr_cat(draining_constraint, " && (%s)", m_cancel_requirements.c_str());
	}

	ClassAdList startdAds;
	if (!queryMachines(draining_constraint.c_str(), "CANCEL_REQUIREMENTS",startdAds,&m_drain_attrs))
	{
		return;
	}

	startdAds.Shuffle();
	startdAds.Sort(StartdSortFunc,&m_rank_ad);

	startdAds.Open();

	unsigned int cancel_count = 0;
	ClassAd *startd_ad_ptr;
	while ( (startd_ad_ptr=startdAds.Next()) )
	{
		ClassAd &startd_ad = *startd_ad_ptr;
		std::string machine;
		std::string name;
		startd_ad.LookupString(ATTR_NAME,name);
		slotNameToDaemonName(name,machine);

		if( !cancelled_machines.count(machine)) {
			cancel_this_drain(startd_ad);
			cancelled_machines.insert(machine);
			cancel_count ++;
		}
	}

	startdAds.Close();


	dprintf(D_ALWAYS, "Cancelled draining of %u whole machines.\n", cancel_count);
}

void
Defrag::dprintf_set(const char *message, Defrag::MachineSet *m) const {
	dprintf(D_ALWAYS, "%s\n", message);

	for (Defrag::MachineSet::iterator it = m->begin(); it != m->end(); it++) {
		dprintf(D_ALWAYS, "\t %s\n", (*it).c_str());
	}
	
	if (m->size() == 0) {
		dprintf(D_ALWAYS, "(no machines)\n");
	}	
	
}

void Defrag::poll( int /* timerID */ )
{
	dprintf(D_FULLDEBUG,"Evaluating defragmentation policy.\n");

		// If we crash during this polling cycle, we will have saved
		// the time of the last poll, so the next cycle will be
		// scheduled on the false assumption that a cycle ran now.  In
		// this way, we error on the side of draining too little
		// rather than too much.

	time_t now = time(NULL);
	time_t prev = m_last_poll;
	m_last_poll = now;
#ifdef USE_DEFRAG_STATE_FILE
	saveState();
#endif

	m_stats.Tick();

	int num_to_drain = m_draining_per_poll;
	double real_num_to_drain = num_to_drain;

	if (prev != 0) {
		time_t last_hour    = (prev / 3600)*3600;
		time_t current_hour = (now  / 3600)*3600;
		time_t last_day     = (prev / (3600*24))*3600*24;
		time_t current_day  = (now  / (3600*24))*3600*24;
		double real_frac = 0.0;

		if( current_hour != last_hour ) {
			num_to_drain += prorate(m_draining_per_poll_hour,now-current_hour,3600,m_polling_interval,&real_frac);
		}
		if( current_day != last_day ) {
			num_to_drain += prorate(m_draining_per_poll_day,now-current_day,3600*24,m_polling_interval,&real_frac);
		}
		real_num_to_drain += real_frac;
	}

	// build a constraint that matches machines that are draining where the
	// drain command came from me (or for an unknown reason).
	std::string draining_constraint(BASE_DRAINING_CONSTRAINT " && ");
	draining_constraint += m_drain_by_me_expr;

	MachineSet draining_machines;
	int num_draining = countMachines(draining_constraint.c_str(),"<Machines currently draining>", &draining_machines);
	m_stats.MachinesDraining = num_draining;

	MachineSet whole_machines;
	int num_whole_machines = countMachines(m_whole_machine_expr.c_str(),"DEFRAG_WHOLE_MACHINE_EXPR",&whole_machines);
	m_stats.WholeMachines = num_whole_machines;

	dprintf(D_ALWAYS,"There are currently %d draining and %d whole machines.\n",
			num_draining,num_whole_machines);

	// Calculate arrival rate of fully drained machines.  This is a bit tricky because we poll.

	// We count by finding the newly-arrived
	// fully drained machines, and add to that count machines which are no-longer draining.
	// This allows us to find machines that have fully drained, but were then claimed between
	// polling cycles.
	
	MachineSet new_machines;
	MachineSet no_longer_whole_machines;

	// Find newly-arrived machines
	std::set_difference(whole_machines.begin(), whole_machines.end(), 
						m_prev_whole_machines.begin(), m_prev_whole_machines.end(),
						std::inserter(new_machines, new_machines.begin()));
	
	// Now, newly-departed machines
	std::set_difference(m_prev_draining_machines.begin(), m_prev_draining_machines.end(),
					    draining_machines.begin(), draining_machines.end(),
						std::inserter(no_longer_whole_machines, no_longer_whole_machines.begin()));

	dprintf_set("Set of current whole machines is ", &whole_machines);
	dprintf_set("Set of current draining machines is ", &draining_machines);
	dprintf_set("Newly Arrived whole machines is ", &new_machines);
	dprintf_set("Newly departed draining machines is ", &no_longer_whole_machines);

	m_prev_draining_machines = draining_machines;
	m_prev_whole_machines   = whole_machines;

	int newly_drained = new_machines.size() + no_longer_whole_machines.size();
	double arrival_rate = 0.0;

	// If there is an arrival...
	if (newly_drained > 0) {
		time_t current = time(0);

		// And it isn't the first one since defrag boot...
		if (m_last_whole_machine_arrival > 0) {
			m_whole_machines_arrived += newly_drained;

			time_t arrival_time = current - m_last_whole_machine_arrival;
			if (arrival_time < 1) arrival_time = 1; // very unlikely, but just in case

			m_whole_machine_arrival_sum += newly_drained * arrival_time;

			arrival_rate = newly_drained / ((double)arrival_time);
			dprintf(D_ALWAYS, "Arrival rate is %g machines/hour\n", arrival_rate * 3600.0);
		}
		m_last_whole_machine_arrival = current;
	}

	dprintf(D_ALWAYS, "Lifetime whole machines arrived: %d\n", m_whole_machines_arrived);
	if (m_whole_machine_arrival_sum > 0) {
		double lifetime_mean = m_whole_machines_arrived / m_whole_machine_arrival_sum;
		dprintf(D_ALWAYS, "Lifetime mean arrival rate: %g machines / hour\n", 3600.0 * lifetime_mean);

		if (newly_drained > 0) {
			double diff = arrival_rate - lifetime_mean;
			m_whole_machine_arrival_mean_squared += diff * diff;
		}
		double sd = sqrt(m_whole_machine_arrival_mean_squared / m_whole_machines_arrived);
		dprintf(D_ALWAYS, "Lifetime mean arrival rate sd: %g\n", sd * 3600);

		m_stats.MeanDrainedArrival = lifetime_mean;
		m_stats.MeanDrainedArrivalSD = sd;
		m_stats.DrainedMachines = m_whole_machines_arrived;
	}

	queryDrainingCost();

	// If possible, cancel some drains.
	MachineSet cancelled_machines;
	poll_cancel(cancelled_machines);

	if( num_to_drain <= 0 ) {
		dprintf(D_ALWAYS,"Doing nothing, because number to drain in next %ds is calculated to be %.3f.\n",
				m_polling_interval, real_num_to_drain);
		return;
	}

	if( (int)ceil(m_draining_per_hour) <= 0 ) {
		dprintf(D_ALWAYS,"Doing nothing, because DEFRAG_DRAINING_MACHINES_PER_HOUR=%f\n",
				m_draining_per_hour);
		return;
	}

	if( m_max_draining == 0 ) {
		dprintf(D_ALWAYS,"Doing nothing, because DEFRAG_MAX_CONCURRENT_DRAINING=0\n");
		return;
	}

	if( m_max_whole_machines == 0 ) {
		dprintf(D_ALWAYS,"Doing nothing, because DEFRAG_MAX_WHOLE_MACHINES=0\n");
		return;
	}

	if( m_max_draining >= 0 ) {
		if( num_draining >= m_max_draining ) {
			dprintf(D_ALWAYS,"Doing nothing, because DEFRAG_MAX_CONCURRENT_DRAINING=%d and there are %d draining machines.\n",
					m_max_draining, num_draining);
			return;
		}
		else if( num_draining < 0 ) {
			dprintf(D_ALWAYS,"Doing nothing, because DEFRAG_MAX_CONCURRENT_DRAINING=%d and the query to count draining machines failed.\n",
					m_max_draining);
			return;
		}
	}

	if( m_max_whole_machines >= 0 ) {
		if( num_whole_machines >= m_max_whole_machines ) {
			dprintf(D_ALWAYS,"Doing nothing, because DEFRAG_MAX_WHOLE_MACHINES=%d and there are %d whole machines.\n",
					m_max_whole_machines, num_whole_machines);
			return;
		}
	}

		// Even if m_max_whole_machines is -1 (infinite), we still need
		// the list of whole machines in order to filter them out in
		// the draining selection algorithm, so abort now if the
		// whole machine query failed.
	if( num_whole_machines < 0 ) {
		dprintf(D_ALWAYS,"Doing nothing, because the query to find whole machines failed.\n");
		return;
	}

	dprintf(D_ALWAYS,"Looking for %d machines to drain.\n",num_to_drain);

	ClassAdList startdAds;
	std::string requirements(BASE_SLOT_CONSTRAINT " && Draining =!= true");
	if ( ! m_defrag_requirements.empty()) {
		formatstr_cat(requirements, " && (%s)", m_defrag_requirements.c_str());
	}
	if( !queryMachines(requirements.c_str(),"DEFRAG_REQUIREMENTS",startdAds,&m_drain_attrs) ) {
		dprintf(D_ALWAYS,"Doing nothing, because the query to select machines matching DEFRAG_REQUIREMENTS failed.\n");
		return;
	}

	startdAds.Shuffle();
	startdAds.Sort(StartdSortFunc,&m_rank_ad);

	startdAds.Open();
	int num_drained = 0;
	ClassAd *startd_ad_ptr;
	MachineSet machines_done;
	while( (startd_ad_ptr=startdAds.Next()) ) {

		ClassAd &startd_ad = *startd_ad_ptr;

		std::string machine;
		std::string name;
		startd_ad.LookupString(ATTR_NAME,name);
		slotNameToDaemonName(name,machine);

		// If we have already cancelled draining on this machine, ignore it for this cycle.
		if( cancelled_machines.count(machine) ) {
			dprintf(D_FULLDEBUG,
					"Skipping %s: already cancelled draining of %s in this cycle.\n",
					name.c_str(),machine.c_str());
			continue;
		}

		if( machines_done.count(machine) ) {
			dprintf(D_FULLDEBUG,
					"Skipping %s: already attempted to drain %s in this cycle.\n",
					name.c_str(),machine.c_str());
			continue;
		}

		if( whole_machines.count(machine) ) {
			dprintf(D_FULLDEBUG,
					"Skipping %s: because it is already running as a whole machine.\n",
					name.c_str());
			continue;
		}

		if( drain_this(startd_ad) ) {
			machines_done.insert(machine);

			if( ++num_drained >= num_to_drain ) {
				dprintf(D_ALWAYS,
						"Drained maximum number of machines allowed in this cycle (%d).\n",
						num_to_drain);
				break;
			}
		}
	}
	startdAds.Close();

	dprintf(D_ALWAYS,"Drained %d machines (wanted to drain %d machines).\n",
			num_drained,num_to_drain);

	dprintf(D_FULLDEBUG,"Done evaluating defragmentation policy.\n");
}

bool
Defrag::drain_this(const ClassAd &startd_ad)
{
	std::string name;
	startd_ad.LookupString(ATTR_NAME,name);

	dprintf(D_ALWAYS,"Initiating %s draining of %s.\n",
			m_draining_schedule_str.c_str(),name.c_str());

	DCStartd startd( &startd_ad );

	time_t graceful_completion = 0;
	startd_ad.LookupInteger(ATTR_EXPECTED_MACHINE_GRACEFUL_DRAINING_COMPLETION,graceful_completion);
	time_t quick_completion = 0;
	startd_ad.LookupInteger(ATTR_EXPECTED_MACHINE_QUICK_DRAINING_COMPLETION,quick_completion);
	time_t graceful_badput = 0;
	startd_ad.LookupInteger(ATTR_EXPECTED_MACHINE_GRACEFUL_DRAINING_BADPUT,graceful_badput);
	time_t quick_badput = 0;
	startd_ad.LookupInteger(ATTR_EXPECTED_MACHINE_QUICK_DRAINING_BADPUT,quick_badput);

	time_t now = time(NULL);
	std::string draining_check_expr;
	double badput_growth_tolerance = 1.25; // for now, this is hard-coded
	time_t negligible_badput = 1200;
	time_t negligible_deadline_slippage = 1200;
	if( m_draining_schedule <= DRAIN_GRACEFUL ) {
		dprintf(D_ALWAYS,"Expected draining completion time is %llds; expected draining badput is %lld cpu-seconds\n",
				(long long)(graceful_completion-now),(long long) graceful_badput);
		formatstr(draining_check_expr,"%s <= %lld && %s <= %lld",
				ATTR_EXPECTED_MACHINE_GRACEFUL_DRAINING_COMPLETION,
				(long long) graceful_completion + negligible_deadline_slippage,
				ATTR_EXPECTED_MACHINE_GRACEFUL_DRAINING_BADPUT,
				(long long)(badput_growth_tolerance*graceful_badput) + negligible_badput);
	}
	else { // DRAIN_FAST and DRAIN_QUICK are effectively equivalent here
		dprintf(D_ALWAYS,"Expected draining completion time is %llds; expected draining badput is %lld cpu-seconds\n",
				(long long)(quick_completion-now),(long long)quick_badput);
		formatstr(draining_check_expr,"%s <= %lld && %s <= %lld",
				ATTR_EXPECTED_MACHINE_QUICK_DRAINING_COMPLETION,
				(long long) quick_completion + negligible_deadline_slippage,
				ATTR_EXPECTED_MACHINE_QUICK_DRAINING_BADPUT,
				(long long) (badput_growth_tolerance*quick_badput) + negligible_badput);
	}

	std::string drain_reason;
	formatstr(drain_reason, "Defrag %s", m_daemon_name.c_str());
	std::string request_id;
	bool rval = startd.drainJobs( m_draining_schedule, drain_reason.c_str(), DRAIN_RESUME_ON_COMPLETION,
		draining_check_expr.c_str(), m_draining_start_expr.c_str(), request_id );

	if( !rval ) {
		dprintf(D_ALWAYS,"Failed to send request to drain %s: %s\n",startd.name(),startd.error());
		m_stats.DrainFailures += 1;
	} else {
		m_stats.DrainSuccesses += 1;
	}

	defrag_log.record_drain( name, m_draining_schedule_str );
	return rval ? true : false;
}

bool
Defrag::cancel_this_drain(const ClassAd &startd_ad)
{

	std::string name;
	startd_ad.LookupString(ATTR_NAME,name);

	dprintf(D_ALWAYS, "Cancel draining of %s.\n", name.c_str());

	DCStartd startd( &startd_ad );

	bool rval = startd.cancelDrainJobs( NULL );
	if ( rval ) {
		dprintf(D_FULLDEBUG, "Sent request to cancel draining on %s\n", startd.name());
	} else {
		dprintf(D_ALWAYS, "Unable to cancel draining on %s: %s\n", startd.name(), startd.error());
	}

	defrag_log.record_cancel( startd.name() );
	return rval;
}

void
Defrag::publish(ClassAd *ad)
{
	SetMyTypeName(*ad, DEFRAG_ADTYPE);

	ad->Assign(ATTR_NAME,m_daemon_name);

	defrag_log.write_to_ad(ad);

	m_stats.Tick();
	m_stats.Publish(*ad);
	daemonCore->publish(ad);
}

void
Defrag::updateCollector( int /* timerID */ ) {
	publish(&m_public_ad);
	daemonCore->sendUpdates(UPDATE_AD_GENERIC, &m_public_ad, nullptr, true);
}

void
Defrag::invalidatePublicAd() {
	ClassAd invalidate_ad;
	std::string line;

	SetMyTypeName(invalidate_ad, QUERY_ADTYPE);
	invalidate_ad.Assign(ATTR_TARGET_TYPE, DEFRAG_ADTYPE);

	formatstr(line,"%s == \"%s\"", ATTR_NAME, m_daemon_name.c_str());
	invalidate_ad.AssignExpr(ATTR_REQUIREMENTS, line.c_str());
	invalidate_ad.Assign(ATTR_NAME, m_daemon_name);
	daemonCore->sendUpdates(INVALIDATE_ADS_GENERIC, &invalidate_ad, NULL, false);
}
