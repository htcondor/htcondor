#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_commands.h"

#include "ce-audit-plugin.h"
#include "condor_regex.h"

#include <algorithm>
#include <iterator>

static CEAuditPlugin instance;

CEAuditPlugin::CEAuditPlugin() : CollectorPlugin(), maxJobSeconds(-1) {
}

CEAuditPlugin::~CEAuditPlugin() {
}

void
CEAuditPlugin::initialize() {
    int maxJobHours = param_integer( "AUDIT_PAYLOAD_MAX_HOURS", 3 * 24 );
    dprintf( D_AUDIT, "Audit payload maximum job hours: %d\n", maxJobHours );
    this->maxJobSeconds = maxJobHours * 60 * 60;
}

void
CEAuditPlugin::shutdown() {
}

void
CEAuditPlugin::stopJob(const ClassAd& ad) {
    int integerSlotID;
    std::string name, slotID, globalJobID;
    if(! ad.LookupString("Name", name)) { return; }
    if(! ad.LookupInteger("SlotID", integerSlotID)) { return; }
    formatstr(slotID, "%d", integerSlotID);

    int errcode;
    int erroffset;
    std::string matchRE;
    std::string indexName = name;
    ad.LookupString("GLIDEIN_MASTER_NAME", indexName);
    if( indexName == name ) {
        // stop all jobs under this master
        matchRE = ".*";
    } else {
        // names of form "slotN@" stop that name and all "slotN_M@" names
        Regex re;
        bool success = re.compile( "^(slot[0-9]*)@.*'", &errcode, &erroffset);
        if (!success) {
            EXCEPT("Programmer error:  unable to compile regexp: ^(slot[0-9]*)@.*");
        }
        std::vector<std::string> groups; // HTCONDOR-322
        if( re.match( name,  &groups ) ) {
            formatstr( matchRE, "^%s[@_]", groups[1].c_str() );
        }
    }
    auto index = std::make_pair( indexName, slotID );

    auto iterM = runningMasters.find(index);
    if( iterM == runningMasters.end() ) { return; }

    auto & runningJobs = iterM->second.second;
    std::vector<std::pair<std::string, std::string>> stopJobs;
    if( matchRE.empty() ) {
        // no match expression, just stop one
        if( runningJobs.find(name) == runningJobs.end() ) { return; }
        stopJobs.push_back(std::make_pair(name, ""));
    } else {
        if( matchRE == ".*" ) {
            std::copy( runningJobs.begin(), runningJobs.end(),
                       std::back_inserter(stopJobs) );
        } else {
            Regex re;
            bool success = re.compile(matchRE.c_str(), &errcode, &erroffset);
            if (!success) {
                dprintf(D_AUDIT, "Unable to compile regexp: %s", matchRE.c_str());
            } else {
               std::copy_if( runningJobs.begin(), runningJobs.end(),
                          std::back_inserter(stopJobs),
                          [& re](const std::pair<std::string, std::string> p){ return re.match(p.first); }
                        );
           }
        }
    }

    if( !stopJobs.empty() ) {
        std::string jobList;
        for( const auto& p : stopJobs ) {
            formatstr_cat(jobList, "%s ", p.first.c_str() );
        }
        // dprintf( D_AUDIT, "CEAuditPlugin::stopJob(): stopping jobs %s\n", jobList.c_str() );
    }

    for( const auto & p : stopJobs ) {
        dprintf( D_AUDIT, "Job stop: {'Name': '%s', 'SlotID': '%s', 'GlobalJobID': '%s'}\n",
                 name.c_str(), slotID.c_str(), p.second.c_str() );
        runningJobs.erase(p.first);
    }
}

void
CEAuditPlugin::invalidate(int command, const ClassAd& ad) {
    if( command != INVALIDATE_STARTD_ADS ) { return; }
    // dprintf( D_AUDIT, "CEAuditPlugin::invalidate()\n" );
    stopJob(ad);
}

void
CEAuditPlugin::startJob(const ClassAd& ad) {
    int integerSlotID;
    std::string name, slotID, globalJobID;
    if(! ad.LookupString("Name", name)) { return; }
    if(! ad.LookupInteger("SlotID", integerSlotID)) { return; }
    if(! ad.LookupString("GlobalJobId", globalJobID)) { return; }
    // dprintf( D_AUDIT, "CEAuditPlugin::startJob(): found ad with job.\n" );

    std::string indexName = name;
    ad.LookupString("GLIDEIN_MASTER_NAME", indexName);
    formatstr(slotID, "%d", integerSlotID);
    auto index = std::make_pair( indexName, slotID );

    auto iterM = runningMasters.find(index);
    if( iterM != runningMasters.end() ) {
        // dprintf( D_AUDIT, "CEAuditPlugin::startJob(): Master (%s, %s) found in list.\n", indexName.c_str(), slotID.c_str() );
        auto runningJob = iterM->second.second.find(name);
        if( runningJob != iterM->second.second.end() ) {
            if( runningJob->second == globalJobID ) {
                // just an update to a running job, ignore
                // dprintf( D_AUDIT, "CEAuditPlugin::startJob(): Ignoring update to running job %s\n", name.c_str() );
                return;
            } else {
                // First stop the existing job, the slot is being reused
                stopJob(ad);
                // this may have remove the last job in this master, check again
            }
        }
    }

    time_t now = 0;
    iterM = runningMasters.find(index);
    if( iterM == runningMasters.end() ) {
        // dprintf( D_AUDIT, "CEAuditPlugin::startJob(): Master (%s, %s) is new.\n", indexName.c_str(), slotID.c_str() );
        std::map<std::string, std::string> jobs;
        now = time(NULL);
        runningMasters[index] = std::make_pair( now, jobs );
    }

    iterM = runningMasters.find(index);
    iterM->second.second[name] = globalJobID;

    // Everything _but_ SlotID is a string.
    std::vector<std::string> keys = { // "Name", "SlotID", "GlobalJobId",
        "RemoteOwner", "ClientMachine", "ProjectName", "Group",
        "x509UserProxyVOName", "x509userproxysubject", "x509UserProxyEmail" };

    std::string logline = "Job start: {";
    formatstr_cat( logline,
        "'Name': '%s', 'SlotID': '%s', 'GlobalJobId': '%s'",
        name.c_str(), slotID.c_str(), globalJobID.c_str() );
    for( const auto & key : keys ) {
        std::string value;
        if(ad.LookupString( key, value )) {
            formatstr_cat( logline, ", '%s': '%s'", key.c_str(), value.c_str() );
        }
    }
    logline += "}";

    dprintf( D_AUDIT, "%s\n", logline.c_str() );


    if( now == 0 ) { return; }

    // also look for expired jobs at the beginning of the list and stop them
    std::vector< std::pair< std::string, std::string > > mastersToErase;
    for( const auto & m : runningMasters ) {
        const auto &thisMaster = m.second;
        time_t deltasecs = now - thisMaster.first;
        if( deltasecs <= this->maxJobSeconds ) { break; }

        auto runningJobs = thisMaster.second;
        for( const auto &j : runningJobs ) {
            dprintf( D_AUDIT, "Cleaning up %ld-second expired job: {'Name': '%s', 'GlobalJobId': '%s'}\n",
                deltasecs, j.first.c_str(), j.second.c_str() );
        }

        mastersToErase.push_back(m.first);
    }

    for( const auto & m : mastersToErase ) {
        runningMasters.erase(m);
    }
}

void
CEAuditPlugin::update(int command, const ClassAd& ad) {
    if( command != UPDATE_STARTD_AD ) { return; }

    std::string state;
    ad.LookupString("State", state);
    if( state == "Unclaimed" ) {
        stopJob(ad); // stop previous job on this slot if any
        return;
    }

    startJob(ad);
}

