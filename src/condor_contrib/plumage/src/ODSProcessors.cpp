/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "stl_string_utils.h"

// local includes
#include "ODSProcessors.h"

using namespace std;
using namespace compat_classad;
using namespace mongo;
using namespace plumage::etl;

// helpers, note expected bob & p vars
#define STRING(X,Y) \
const char* X = p.getStringField(Y); \
if (strcmp(X,"")) bob.append(#X,X);

#define INTEGER(X,Y) bob.append(#X,p.getIntField(Y));
#define DOUBLE(X,Y) bob.appendAsNumber(#X,formatReal(p.getField(Y).Double()));
#define DATE(X,Y) bob.appendDate(#X,Y);

// TODO: for now, insert accountant quota, etc. with 
// the precision we appear to see from userprio
// TODO: needs a common home
// utility to manage float precision to
// ClassAd serialization standard
string formatter;
template<typename T>
const char* formatReal(T real) {
    if (real == 0.0 || real == 1.0) {
        formatstr(formatter, "%.1G", real);
    }
    else {
        formatstr(formatter, "%.6G", real);
    }
    return formatter.c_str();
}

void
plumage::etl::processSubmitterStats(ODSMongodbOps* ops, Date_t& ts) {
    dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processSubmitterStats called...\n");
    DBClientConnection* conn =  ops->m_db_conn;
    conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
    auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Submitter" ) );
    conn->ensureIndex(DB_STATS_SAMPLES_SUB, BSON( "ts" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_SUB, BSON( "sn" << 1 ));
    while( cursor->more() ) {
        BSONObj p = cursor->next();
        // write record to submitter samples
        BSONObjBuilder bob;
        DATE(ts,ts);
        STRING(sn,ATTR_NAME);
        STRING(ma,ATTR_MACHINE);
        INTEGER(jr,ATTR_RUNNING_JOBS);
        // TODO: weird...HeldJobs isn't always there in the raw submitter ad
        int h = p.getIntField(ATTR_HELD_JOBS); h = (h>0) ? h : 0;
        bob.append("jh",h);
        INTEGER(ji,ATTR_IDLE_JOBS);
        conn->insert(DB_STATS_SAMPLES_SUB,bob.obj());
    }
}

void
plumage::etl::processMachineStats(ODSMongodbOps* ops, Date_t& ts) {
    dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processMachineStats() called...\n");
    DBClientConnection* conn =  ops->m_db_conn;
    conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
    auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Machine" ) );
    conn->ensureIndex(DB_STATS_SAMPLES_MACH, BSON( "ts" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_MACH, BSON( "m" << 1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_MACH, BSON( "n" << 1 ));
    while( cursor->more() ) {
        BSONObj p = cursor->next();
        // write record to machine samples
        BSONObjBuilder bob;
        DATE(ts,ts);
        STRING(m,ATTR_MACHINE);
        STRING(n,ATTR_NAME);
        STRING(ar,ATTR_ARCH);
        STRING(os,ATTR_OPSYS);
        STRING(req,ATTR_REQUIREMENTS);
        INTEGER(ki,ATTR_KEYBOARD_IDLE);
        DOUBLE(la,ATTR_LOAD_AVG);
        STRING(st,ATTR_STATE);
        INTEGER(cpu,ATTR_CPUS);
        INTEGER(mem,ATTR_MEMORY);
        // TODO: these might be moved to another collection
//         STRING(gjid,ATTR_GLOBAL_JOB_ID);
//         STRING(ru,ATTR_REMOTE_USER);
//         STRING(ag,ATTR_ACCOUNTING_GROUP);
        conn->insert(DB_STATS_SAMPLES_MACH,bob.obj());
    }
}

void
plumage::etl::processSchedulerStats(ODSMongodbOps* ops, Date_t& ts) {
    dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processSchedulerStats() called...\n");
    DBClientConnection* conn =  ops->m_db_conn;
    conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
    auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Scheduler" ) );
    conn->ensureIndex(DB_STATS_SAMPLES_SCHED, BSON( "ts" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_SCHED, BSON( "n" << 1 ));
    while( cursor->more() ) {
        BSONObj p = cursor->next();
        // write record to scheduler samples
        BSONObjBuilder bob;
        DATE(ts,ts);
        STRING(n,ATTR_NAME);
        INTEGER(mjr,ATTR_MAX_JOBS_RUNNING);
        INTEGER(nu,ATTR_NUM_USERS);
        INTEGER(tja,ATTR_TOTAL_JOB_ADS);
        INTEGER(trun,ATTR_TOTAL_RUNNING_JOBS);
        INTEGER(thj,ATTR_TOTAL_HELD_JOBS);
        INTEGER(tij,ATTR_TOTAL_IDLE_JOBS);
        INTEGER(trem,ATTR_TOTAL_REMOVED_JOBS);
        INTEGER(tsr,ATTR_TOTAL_SCHEDULER_RUNNING_JOBS);
        INTEGER(tsi,ATTR_TOTAL_SCHEDULER_IDLE_JOBS);
        INTEGER(tlr,ATTR_TOTAL_LOCAL_RUNNING_JOBS);
        INTEGER(tli,ATTR_TOTAL_LOCAL_IDLE_JOBS);
        INTEGER(tfj,ATTR_TOTAL_FLOCKED_JOBS);
        conn->insert(DB_STATS_SAMPLES_SCHED,bob.obj());
    }
}

// liberally cribbed from user_prio.cpp
void 
plumage::etl::processAccountantStats(ClassAd* ad, ODSMongodbOps* ops, Date_t& ts)
{
    // attr%d holders
    string  attrName, attrPrio, attrResUsed, attrWtResUsed, attrFactor, attrBeginUsage, attrAccUsage;
    string  attrLastUsage, attrAcctGroup, attrIsAcctGroup;
    string  attrConfigQuota, attrEffectiveQuota, attrSubtreeQuota, attrSurplusPolicy;
    
    // values
    string  name, acctGroup, surplusPolicy;
    float priority, factor, wtResUsed, configQuota, effectiveQuota, subtreeQuota, accUsage = -1;
    int   resUsed, beginUsage, lastUsage;
    resUsed = beginUsage = lastUsage = 0;
    bool isAcctGroup;

    DBClientConnection* conn = ops->m_db_conn;
    conn->ensureIndex(DB_STATS_SAMPLES_ACCOUNTANT, BSON( "ts" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_ACCOUNTANT, BSON( "lu" << -1 ));
    conn->ensureIndex(DB_STATS_SAMPLES_ACCOUNTANT, BSON( "n" << 1 ));
    unsigned long long acct_count = conn->count(DB_STATS_SAMPLES_ACCOUNTANT);

    // eventhough the Accountant doesn't forget
    // we don't care about stale submitters (default: last 24 hours)
    int cfg_last_usage = param_integer("ODS_ACCOUNTANT_LAST_USAGE", 60*60*24);
    int minLastUsageTime = time(0)-cfg_last_usage;
    int numElem = -1;
    ad->LookupInteger( "NumSubmittors", numElem );

    for( int i=1; i<=numElem; i++) {
        priority=0;
        isAcctGroup = false;

        // skip stale records unless we have none
        formatstr( attrLastUsage , "LastUsageTime%d", i );
        ad->LookupInteger  ( attrLastUsage.c_str(), lastUsage );
        if (lastUsage < minLastUsageTime && acct_count > 0)
            continue;

        // parse the horrid classad
        formatstr( attrName , "Name%d", i );
        formatstr( attrPrio , "Priority%d", i );
        formatstr( attrResUsed , "ResourcesUsed%d", i );
        formatstr( attrWtResUsed , "WeightedResourcesUsed%d", i );
        formatstr( attrFactor , "PriorityFactor%d", i );
        formatstr( attrBeginUsage , "BeginUsageTime%d", i );
        formatstr( attrAccUsage , "WeightedAccumulatedUsage%d", i );
        formatstr( attrAcctGroup, "AccountingGroup%d", i);
        formatstr( attrIsAcctGroup, "IsAccountingGroup%d", i);
        formatstr( attrConfigQuota, "ConfigQuota%d", i);
        formatstr( attrEffectiveQuota, "EffectiveQuota%d", i);
        formatstr( attrSubtreeQuota, "SubtreeQuota%d", i);
        formatstr( attrSurplusPolicy, "SurplusPolicy%d", i);

        ad->LookupString   ( attrName.c_str(), name );
        ad->LookupFloat    ( attrPrio.c_str(), priority );
        ad->LookupFloat    ( attrFactor.c_str(), factor );
        ad->LookupFloat    ( attrAccUsage.c_str(), accUsage );
        ad->LookupInteger  ( attrBeginUsage.c_str(), beginUsage );
        ad->LookupInteger  ( attrResUsed.c_str(), resUsed );
        ad->LookupBool     ( attrIsAcctGroup.c_str(), isAcctGroup);
        ad->LookupFloat    ( attrConfigQuota.c_str(), configQuota );
        ad->LookupFloat    ( attrEffectiveQuota.c_str(), effectiveQuota );
        ad->LookupFloat    ( attrSubtreeQuota.c_str(), subtreeQuota );
        ad->LookupString   ( attrSurplusPolicy.c_str(), surplusPolicy );
        
        if( !ad->LookupFloat( attrWtResUsed.c_str(), wtResUsed ) ) {
            wtResUsed = resUsed;
        }
        if (!ad->LookupString(attrAcctGroup.c_str(), acctGroup)) {
            acctGroup = "<none>";
        }

        BSONObjBuilder bob;
        bob.appendDate("ts",ts);
        bob.append("n",name);
        bob.append("ag",acctGroup);
        bob.appendAsNumber("prio",formatReal(priority));
        bob.appendAsNumber("fac",formatReal(factor));
        bob.append("ru",resUsed);
        bob.append("wru",wtResUsed);
        // condor timestamps need massaging when going in the db
        bob.appendDate("bu",static_cast<unsigned long long>(beginUsage)*1000);
        bob.appendDate("lu",static_cast<unsigned long long>(lastUsage)*1000);
        bob.appendAsNumber("au",formatReal(accUsage));
        bob.appendAsNumber("cq",formatReal(configQuota));
        bob.appendAsNumber("eq",formatReal(effectiveQuota));
        bob.appendAsNumber("sq",formatReal(subtreeQuota));
        if (!surplusPolicy.empty()) bob.append("sp",surplusPolicy);
        
        conn->insert(DB_STATS_SAMPLES_ACCOUNTANT,bob.obj());
    }
    
}
