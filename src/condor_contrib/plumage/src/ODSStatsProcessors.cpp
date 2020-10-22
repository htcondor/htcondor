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
#include "CondorError.h"

// platform includes
#include <libgen.h> // dirname
#include "directory.h"
#include "stat_wrapper.h"

// local includes
#include "ODSStatsProcessors.h"
#include "ODSDBNames.h"
#include "ODSUtils.h"
#include "ODSHistoryFile.h"

using namespace std;
using namespace mongo;
using namespace plumage::etl;
using namespace plumage::stats;
using namespace plumage::util;

// helpers, note expected bob & p vars
#define STRING(X,Y) \
const char* X = p.getStringField(Y); \
if (strcmp(X,"")) bob.append(#X,X);

#define INTEGER(X,Y) bob.append(#X,p.getIntField(Y));
#define DOUBLE(X,Y) bob.appendAsNumber(#X,formatReal(p.getField(Y).Double()));
#define DATE(X,Y) bob.appendDate(#X,Y);

void
plumage::stats::processSubmitterStats(ODSMongodbOps* ops, Date_t& ts) {
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
plumage::stats::processMachineStats(ODSMongodbOps* ops, Date_t& ts) {
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
plumage::stats::processSchedulerStats(ODSMongodbOps* ops, Date_t& ts) {
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
plumage::stats::processAccountantStats(ClassAd* ad, ODSMongodbOps* ops, Date_t& ts)
{
    // attr%d holders...sadly reverting back to MyString for convenience of formatstr
    MyString  attrName, attrPrio, attrResUsed, attrWtResUsed, attrFactor, attrBeginUsage, attrAccUsage;
    MyString  attrLastUsage, attrAcctGroup, attrIsAcctGroup;
    MyString  attrConfigQuota, attrEffectiveQuota, attrSubtreeQuota, attrSurplusPolicy;
    
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
        attrLastUsage.formatstr("LastUsageTime%d", i );
        ad->LookupInteger  ( attrLastUsage.Value(), lastUsage );
        if (lastUsage < minLastUsageTime && acct_count > 0)
            continue;

        // parse the horrid classad
        attrName.formatstr("Name%d", i );
        attrPrio.formatstr("Priority%d", i );
        attrResUsed.formatstr("ResourcesUsed%d", i );
        attrWtResUsed.formatstr("WeightedResourcesUsed%d", i );
        attrFactor.formatstr("PriorityFactor%d", i );
        attrBeginUsage.formatstr("BeginUsageTime%d", i );
        attrAccUsage.formatstr("WeightedAccumulatedUsage%d", i );
        attrAcctGroup.formatstr("AccountingGroup%d", i);
        attrIsAcctGroup.formatstr("IsAccountingGroup%d", i);
        attrConfigQuota.formatstr("ConfigQuota%d", i);
        attrEffectiveQuota.formatstr("EffectiveQuota%d", i);
        attrSubtreeQuota.formatstr("SubtreeQuota%d", i);
        attrSurplusPolicy.formatstr("SurplusPolicy%d", i);

        ad->LookupString   ( attrName.Value(), name );
        ad->LookupFloat    ( attrPrio.Value(), priority );
        ad->LookupFloat    ( attrFactor.Value(), factor );
        ad->LookupFloat    ( attrAccUsage.Value(), accUsage );
        ad->LookupInteger  ( attrBeginUsage.Value(), beginUsage );
        ad->LookupInteger  ( attrResUsed.Value(), resUsed );
        ad->LookupBool     ( attrIsAcctGroup.Value(), isAcctGroup);
        ad->LookupFloat    ( attrConfigQuota.Value(), configQuota );
        ad->LookupFloat    ( attrEffectiveQuota.Value(), effectiveQuota );
        ad->LookupFloat    ( attrSubtreeQuota.Value(), subtreeQuota );
        ad->LookupString   ( attrSurplusPolicy.Value(), surplusPolicy );
        
        if( !ad->LookupFloat( attrWtResUsed.Value(), wtResUsed ) ) {
            wtResUsed = resUsed;
        }
        if (!ad->LookupString(attrAcctGroup.Value(), acctGroup)) {
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
