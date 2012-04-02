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
#define STRING(X,Y) bob.append(X,p.getStringField(Y));
#define INTEGER(X,Y) bob.append(X,p.getIntField(Y));
#define DOUBLE(X,Y) bob.append(X,p.getField(Y).Double());
#define DATE(X,Y) bob.appendDate(X,Y);

void
plumage::etl::processSubmitterStats(ODSMongodbOps* ops, Date_t& ts) {
    dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processSubmitterStats called...\n");
    DBClientConnection* conn =  ops->m_db_conn;
    conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
    auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Submitter" ) );
    while( cursor->more() ) {
        BSONObj p = cursor->next();
        // write record to submitter samples
        conn->ensureIndex(DB_STATS_SAMPLES_SUB, BSON( "ts" << 1 << "sn" << 1 ));
        BSONObjBuilder bob;
        DATE("ts",ts);
        STRING("sn",ATTR_NAME);
        STRING("ma",ATTR_MACHINE);
        INTEGER("jr",ATTR_RUNNING_JOBS);
        // TODO: weird...HeldJobs isn't always there in the raw submitter ad
        int h = p.getIntField(ATTR_HELD_JOBS); h = (h>0) ? h : 0;
        bob.append("jh",h);
        INTEGER("ji",ATTR_IDLE_JOBS);
        conn->insert(DB_STATS_SAMPLES_SUB,bob.obj());
    }
}

void
plumage::etl::processMachineStats(ODSMongodbOps* ops, Date_t& ts) {
    dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processMachineStats() called...\n");
    DBClientConnection* conn =  ops->m_db_conn;
    conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
    auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Machine" ) );
    while( cursor->more() ) {
        BSONObj p = cursor->next();
        // write record to machine samples
        conn->ensureIndex(DB_STATS_SAMPLES_MACH, BSON( "ts" << 1 << "m" << 1 ));
        BSONObjBuilder bob;
        DATE("ts",ts);
        STRING("m",ATTR_MACHINE);
        STRING("n",ATTR_NAME);
        STRING("ar",ATTR_ARCH);
        STRING("os",ATTR_OPSYS);
        STRING("req",ATTR_REQUIREMENTS);
        INTEGER("ki",ATTR_KEYBOARD_IDLE);
        DOUBLE("la",ATTR_LOAD_AVG);
        STRING("st",ATTR_STATE);
        INTEGER("cpu",ATTR_CPUS);
        INTEGER("mem",ATTR_MEMORY);
        const char* gjid = p.getStringField(ATTR_GLOBAL_JOB_ID);
        const char* ru = p.getStringField(ATTR_REMOTE_USER);
        const char* ag = p.getStringField(ATTR_ACCOUNTING_GROUP);
        strcmp(gjid,"")?bob.append("gjid",gjid):bob.appendNull("gjid");
        strcmp(ru,"")?bob.append("ru",ru):bob.appendNull("ru");
        strcmp(ag,"")?bob.append("ag",ag):bob.appendNull("ag");
        conn->insert(DB_STATS_SAMPLES_MACH,bob.obj());
    }
}

void
plumage::etl::processSchedulerStats(ODSMongodbOps* ops, Date_t& ts) {
    dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processSchedulerStats() called...\n");
    DBClientConnection* conn =  ops->m_db_conn;
    conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
    auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Scheduler" ) );
    while( cursor->more() ) {
        BSONObj p = cursor->next();
        // write record to scheduler samples
        conn->ensureIndex(DB_STATS_SAMPLES_SCHED, BSON( "ts" << 1 << "n" << 1 ));
        BSONObjBuilder bob;
        STRING("n",ATTR_NAME);
        INTEGER("mjr",ATTR_MAX_JOBS_RUNNING);
        INTEGER("nu",ATTR_NUM_USERS);
        INTEGER("tja",ATTR_TOTAL_JOB_ADS);
        INTEGER("trun",ATTR_TOTAL_RUNNING_JOBS);
        INTEGER("thj",ATTR_TOTAL_HELD_JOBS);
        INTEGER("thi",ATTR_TOTAL_IDLE_JOBS);
        INTEGER("trem",ATTR_TOTAL_REMOVED_JOBS);
        INTEGER("tsr",ATTR_TOTAL_SCHEDULER_RUNNING_JOBS);
        INTEGER("tsi",ATTR_TOTAL_SCHEDULER_IDLE_JOBS);
        INTEGER("tlr",ATTR_TOTAL_LOCAL_RUNNING_JOBS);
        INTEGER("tli",ATTR_TOTAL_LOCAL_IDLE_JOBS);
        INTEGER("tfj",ATTR_TOTAL_FLOCKED_JOBS);
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
    string attrConfigQuota, attrEffectiveQuota, attrSubtreeQuota, attrSurplusPolicy;
    
    // values
    string  name, acctGroup, configQuota, effectiveQuota, subtreeQuota, surplusPolicy;
    float priority, factor, wtResUsed, accUsage = -1;
    int   resUsed, beginUsage, lastUsage;
    resUsed = beginUsage = lastUsage = 0;
    bool isAcctGroup;

    int MinLastUsageTime = time(0)-60*60*24;
    int numElem = -1;
    ad->LookupInteger( "NumSubmittors", numElem );
    
    DBClientConnection* conn = ops->m_db_conn;
    conn->ensureIndex(DB_STATS_SAMPLES_SCHED, BSON( "ts" << 1 << "n" << 1 ));

    for( int i=1; i<=numElem; i++) {
        priority=0;
        lastUsage=MinLastUsageTime;
        isAcctGroup = false;

        // parse the horrid classad
        sprintf( attrName , "Name%d", i );
        sprintf( attrPrio , "Priority%d", i );
        sprintf( attrResUsed , "ResourcesUsed%d", i );
        sprintf( attrWtResUsed , "WeightedResourcesUsed%d", i );
        sprintf( attrFactor , "PriorityFactor%d", i );
        sprintf( attrBeginUsage , "BeginUsageTime%d", i );
        sprintf( attrLastUsage , "LastUsageTime%d", i );
        sprintf( attrAccUsage , "WeightedAccumulatedUsage%d", i );
        sprintf( attrAcctGroup, "AccountingGroup%d", i);
        sprintf( attrIsAcctGroup, "IsAccountingGroup%d", i);
        sprintf( attrConfigQuota, "ConfigQuota%d", i);
        sprintf( attrEffectiveQuota, "EffectiveQuota%d", i);
        sprintf( attrSubtreeQuota, "SubtreeQuota%d", i);
        sprintf( attrSurplusPolicy, "SurplusPolicy%d", i);

        ad->LookupString   ( attrName.c_str(), name );
        ad->LookupFloat    ( attrPrio.c_str(), priority );
        ad->LookupFloat    ( attrFactor.c_str(), factor );
        ad->LookupFloat    ( attrAccUsage.c_str(), accUsage );
        ad->LookupInteger  ( attrBeginUsage.c_str(), beginUsage );
        ad->LookupInteger  ( attrLastUsage.c_str(), lastUsage );
        ad->LookupInteger  ( attrResUsed.c_str(), resUsed );
        ad->LookupBool     ( attrIsAcctGroup.c_str(), isAcctGroup);
        
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
        bob.append("prio",priority);
        bob.append("fac",factor);
        bob.append("ru",resUsed);
        bob.append("wru",wtResUsed);
        bob.appendDate("bu",beginUsage);
        bob.appendDate("lu",lastUsage);
        bob.append("au",accUsage);
        bob.append("cq",configQuota);
        bob.append("eq",effectiveQuota);
        bob.append("sq",subtreeQuota);
        bob.append("sp",surplusPolicy);
        
        conn->insert(DB_STATS_SAMPLES_ACCOUNTANT,bob.obj());
    }
    
}