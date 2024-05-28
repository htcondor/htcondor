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
#include "condor_classad.h"
#include "match_prefix.h"
#include "HashTable.h"
#include "condor_attributes.h"
#include "zip_view.hpp"

static size_t ClassAdPtrHash(ClassAd * const &ptr);

static void usage() {
	fprintf(stderr,"USAGE: condor_match_test [OPTIONS] [command]\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"OPTIONS:\n");
	fprintf(stderr,"  -machine-ads <filename>\n");
	fprintf(stderr,"  -job-ads <filename>\n");
	fprintf(stderr,"  -job-constraint <expression>\n");
	fprintf(stderr,"  -machine-constraint <expression>\n");
    fprintf(stderr,"  -claimed <expression>   (machines that should be counted as already matched)\n");
    fprintf(stderr,"  -show-unmatched-machines\n");
    fprintf(stderr,"  -show-unmatched-jobs\n");
    fprintf(stderr,"\nCOMMANDS:\n");
    fprintf(stderr,"demand    -- estimate how many of the machines could be used by the jobs\n");
	exit(2);
}

class MatchTest {
public:
    MatchTest();

    // prints out the estimated demand
    void ShowDemand(char const *jobads_fname,const std::vector<std::string> &job_constraints,char const *machineads_fname,char const *machine_constraint,const std::vector<std::string> &claimed);

    bool analyze_demand(char const *jobads_fname, char const *job_constraint, char const *machineads_fname, char const *machine_constraint, char const *claimed);

    unsigned int getMachinesMatched() const { return m_machines_matched; }
    unsigned int getMachinesUnmatched() const { return m_machines_unmatched; }

    float getMachinesMatchedFraction() const {
        if( m_machines_matched + m_machines_unmatched == 0 ) {
            return 0;
        }
        return ((float)m_machines_matched) /
            (m_machines_matched + m_machines_unmatched);
    }

    void setShowUnmatchedMachines() { m_show_unmatched_machines = true; }
    void setShowUnmatchedJobs() { m_show_unmatched_jobs = true; }

    void showJobAd(ClassAd *ad,char const *label);
    void showMachineAd(ClassAd *ad,char const *label);

private:
    bool m_show_unmatched_machines;
    bool m_show_unmatched_jobs;
    unsigned int m_machines_matched;
    unsigned int m_machines_unmatched;

    ClassAdList m_machineads;
    // hash of matched machines
    HashTable<ClassAd*,bool> m_matched_machines;

    char const *m_last_machineads_fname;
    char const *m_last_machine_constraint;

	bool read_classad_file(const char *filename, ExprTree *constraint_expr, bool (MatchTest::*work_func)(ClassAd *));

    void matchExpr(ExprTree *claimed);
    bool matchJobAd(ClassAd *jobad);
    bool addMachineAd(ClassAd *machinead);
    void unmatch();

	void OptimizeMachineAdForMatchmaking(ClassAd *ad);
	void OptimizeJobAdForMatchmaking(ClassAd *ad);
};

int main(int argc,char *argv[]) {
	int i;
	char const *machineads_fname = NULL;
	char const *jobads_fname = NULL;
	std::vector<std::string> job_constraints;
	std::vector<std::string> claims; // must be one-to-one with job_constraints
	char const *machine_constraint = NULL;
    MatchTest match_test;
    char const *command=NULL;

	for( i=1; i<argc; i++ ) {
		if( match_prefix( argv[i], "-help" ) ) {
			usage();
		}
		else if( match_prefix( argv[i], "-machine-ads" ) ||
                 match_prefix( argv[i], "-machineads" ) ) {
			if( i+1 >= argc ) {
				fprintf(stderr,"ERROR: missing argument to -machineads\n");
				exit(1);
			}
			machineads_fname = argv[++i];
		}
		else if( match_prefix( argv[i], "-job-ads" ) ||
                 match_prefix( argv[i], "-jobads" ) ) {
			if( i+1 >= argc ) {
				fprintf(stderr,"ERROR: missing argument to -jobads\n");
				exit(1);
			}
			jobads_fname = argv[++i];
		}
		else if( match_prefix( argv[i], "-job-constraint" ) ) {
			if( i+1 >= argc ) {
				fprintf(stderr,"ERROR: missing argument to -job-constraint\n");
				exit(1);
			}
            job_constraints.emplace_back( argv[++i] );
		}
		else if( match_prefix( argv[i], "-claimed" ) ) {
			if( i+1 >= argc ) {
				fprintf(stderr,"ERROR: missing argument to -claimed\n");
				exit(1);
			}
            claims.emplace_back( argv[++i] );
		}
		else if( match_prefix( argv[i], "-machine-constraint" ) ) {
			if( i+1 >= argc ) {
				fprintf(stderr,"ERROR: missing argument to -machine-constraint\n");
				exit(1);
			}
			machine_constraint = argv[++i];
		}
        else if( match_prefix( argv[i], "-show-unmatched-machines" ) ) {
            match_test.setShowUnmatchedMachines();
        }
        else if( match_prefix( argv[i], "-show-unmatched-jobs" ) ) {
            match_test.setShowUnmatchedJobs();
        }
        else if( argv[i][0] != '-' ) {
            break;
        }
		else {
			fprintf(stderr,"ERROR: unexpected argument: %s\n", argv[i]);
			exit(2);
		}
	}

    if( i != argc-1 ) {
        fprintf(stderr,"ERROR: unrecognized or missing command\n");
        exit(2);
    }

    command = argv[i++];
    if( !strcasecmp(command,"demand") ) {

        if( !jobads_fname || !machineads_fname ) {
            fprintf(stderr,"ERROR: you must specify both -job-ads and -machine-ads\n");
            exit(2);
        }

        if( job_constraints.size() == 0 ) {
            job_constraints.emplace_back("");
        }
        if( claims.size() != 0 && claims.size() != job_constraints.size() )
        {
            fprintf(stderr,"ERROR: must provide same number of -claimed and -job-constraint arguments\n");
            exit(2);
        }

        match_test.ShowDemand( jobads_fname,job_constraints,machineads_fname,machine_constraint, claims );
    }
    else {
        fprintf(stderr,"ERROR: unrecognized command %s\n",command);
        exit(2);
    }

    return 0;
}


size_t ClassAdPtrHash(ClassAd * const &ptr) {
	intptr_t i = (intptr_t)ptr;

	return (size_t) i;
}

MatchTest::MatchTest():
    m_show_unmatched_machines(false),
    m_show_unmatched_jobs(false),
    m_machines_matched(0),
    m_machines_unmatched(0),
	m_matched_machines(ClassAdPtrHash),
    m_last_machineads_fname(NULL),
    m_last_machine_constraint(NULL)
{
}

void
MatchTest::OptimizeMachineAdForMatchmaking(ClassAd *ad)
{
		// The machine ad will be passed as the RIGHT ad during
		// matchmaking (i.e. in the call to IsAMatch()), so
		// optimize it accordingly.
	std::string error_msg;
	if( !classad::MatchClassAd::OptimizeRightAdForMatchmaking( ad, &error_msg ) ) {
		std::string name;
		ad->LookupString(ATTR_NAME,name);
		dprintf(D_ALWAYS,
				"Failed to optimize machine ad %s for matchmaking: %s\n",	
			name.c_str(),
				error_msg.c_str());
	}
}

void
MatchTest::OptimizeJobAdForMatchmaking(ClassAd *ad)
{
		// The job ad will be passed as the LEFT ad during
		// matchmaking (i.e. in the call to IsAMatch()), so
		// optimize it accordingly.
	std::string error_msg;
	if( !classad::MatchClassAd::OptimizeLeftAdForMatchmaking( ad, &error_msg ) ) {
		int cluster_id=-1,proc_id=-1;
		ad->LookupInteger(ATTR_CLUSTER_ID,cluster_id);
		ad->LookupInteger(ATTR_PROC_ID,proc_id);
		dprintf(D_ALWAYS,
				"Failed to optimize job ad %d.%d for matchmaking: %s\n",	
				cluster_id,
				proc_id,
				error_msg.c_str());
	}
}

void
MatchTest::showJobAd(ClassAd *ad,char const *label)
{
    std::string gid;
    if( !ad->LookupString(ATTR_GLOBAL_JOB_ID,gid) ) {
        int cluster = -1;
        int proc = -1;
        ad->LookupInteger(ATTR_CLUSTER_ID,cluster);
        ad->LookupInteger(ATTR_PROC_ID,proc);
        formatstr(gid,"%d.%d",cluster,proc);
    }

    printf("%s: %s\n",label,gid.c_str());
}

void
MatchTest::showMachineAd(ClassAd *ad,char const *label)
{
    std::string name;
    ad->LookupString(ATTR_NAME,name);

    printf("%s: %s\n",label,name.c_str());
}

void
MatchTest::unmatch()
{
    m_matched_machines.clear();
    m_machines_matched = 0;
    m_machines_unmatched = 0;
}

void
MatchTest::matchExpr(ExprTree *claimed)
{
    ClassAd *machinead;

    m_machineads.Open();
	while( (machinead=m_machineads.Next()) ) {
		bool junk;
		if( m_matched_machines.lookup(machinead,junk) == 0 ) {
			continue; // already matched this machine
		}
		if( EvalExprBool(machinead, claimed) ) {
			m_matched_machines.insert(machinead,true);
		}
	}
}

bool
MatchTest::matchJobAd(ClassAd *jobad)
{
    ClassAd *machinead;
	bool retval=true;

	OptimizeJobAdForMatchmaking( jobad );

    // Currently, there is no cleverness about how best to fill the pool.
    // So if more of the pool could be filled by matching some job to
    // a machine other than the first one we happen to find, this
    // algorithm will not discover that fact.  No RANKs are taken
    // into account either, only Requirements.

    m_machineads.Open();
	bool found_match = false;
	while( (machinead=m_machineads.Next()) ) {
		bool junk;
		if( m_matched_machines.lookup(machinead,junk) == 0 ) {
			continue; // already matched this machine
		}
		if( IsAMatch(jobad,machinead) ) {
			m_matched_machines.insert(machinead,true);
			found_match = true;
			break;
		}
	}

    unsigned int num_machines = m_machineads.MyLength();
	if( (unsigned int)m_matched_machines.getNumElements() == num_machines ) {
		retval=false; // all machines matched
	}
	if( m_show_unmatched_jobs && !found_match ) {
		showJobAd( jobad, "UNMATCHED JOB" );
	}

	delete jobad;
    return retval;
}

bool
MatchTest::addMachineAd(ClassAd *machinead)
{
	OptimizeMachineAdForMatchmaking( machinead );
	m_machineads.Insert(machinead);
	return true;
}

void
MatchTest::ShowDemand(char const *jobads_fname,const std::vector<std::string> &job_constraints,char const *machineads_fname,char const *machine_constraint, const std::vector<std::string> &claims)
{
	for (const auto &[job_constraint, claimed]: c9::zip(job_constraints, claims)) {
        if( !job_constraint.empty() ) {
            printf("\nEstimating total possible machine matches for jobs matching:\n   %s\n", job_constraint.c_str());
        } else {
            printf("\nEstimating total possible machine maches for all jobs\n");
        }
        if( !claimed.empty()) {
            printf("plus machines already claimed:\n   %s\n", claimed.c_str());
        }
        if (!analyze_demand(jobads_fname,job_constraint.c_str(),machineads_fname,machine_constraint,claimed.c_str())) {
            exit(1);
        }

        printf("\nFraction of machines matched: %.3f (%d of %d)\n",getMachinesMatchedFraction(),m_machines_matched,m_machines_matched+m_machines_unmatched);
	}
}

bool
MatchTest::analyze_demand(char const *jobads_fname,char const *job_constraint,char const *machineads_fname,char const *machine_constraint,char const *claimed)
{
    ExprTree *job_constraint_expr = NULL;
    ExprTree *machine_constraint_expr = NULL;
    ExprTree *claimed_expr = NULL;

    if( job_constraint && *job_constraint && ParseClassAdRvalExpr( job_constraint, job_constraint_expr ) ) {
        fprintf( stderr, "Error:  could not parse job-constraint %s\n", job_constraint );
        return false;
    }

    if( claimed && *claimed && ParseClassAdRvalExpr( claimed, claimed_expr ) ) {
        fprintf( stderr, "Error:  could not parse claimed expression %s\n", claimed );
        return false;
    }

    // if we have already loaded the machine ads, skip this step
    if( m_last_machineads_fname != machineads_fname ||
        m_last_machine_constraint != machine_constraint )
    {
        if( machine_constraint && *machine_constraint && ParseClassAdRvalExpr( machine_constraint, machine_constraint_expr ) ) {
            fprintf( stderr, "Error:  could not parse machine-constraint %s\n", machine_constraint );
            return false;
        }

		m_machineads.Clear();

        if( !read_classad_file(machineads_fname,machine_constraint_expr,&MatchTest::addMachineAd) ) {
            return false;
        }

        m_last_machineads_fname = machineads_fname;
        m_last_machine_constraint = machine_constraint;
    }

    unmatch(); // clear any old matches

    if( claimed_expr ) {
        matchExpr( claimed_expr );
    }

    // now iterate over the job ads and try matching them one by one
    if( !read_classad_file(jobads_fname,job_constraint_expr,&MatchTest::matchJobAd) ) {
        return false;
    }

    if( m_show_unmatched_machines ) {
        ClassAd *machinead;
        m_machineads.Open();
        while( (machinead=m_machineads.Next()) ) {
            bool junk;
            if( m_matched_machines.lookup(machinead,junk) == 0 ) {
                continue;
            }
            showMachineAd( machinead, "UNMATCHED MACHINE" );
        }
    }

    unsigned int num_machines = m_machineads.MyLength();
    m_machines_matched = m_matched_machines.getNumElements();
    m_machines_unmatched = num_machines - m_machines_matched;

    if( claimed_expr ) {
        delete claimed_expr;
    }
    if( machine_constraint_expr ) {
        delete machine_constraint_expr;
    }
    if( job_constraint_expr ) {
        delete job_constraint_expr;
    }
	return true;
}

bool
MatchTest::read_classad_file(const char *filename, ExprTree *constraint_expr, bool (MatchTest::*work_func)(ClassAd *))
{
    int is_eof, is_error, is_empty;
    bool success;
    ClassAd *classad;
    FILE *file;
    long ad_pos = 0;

    file = safe_fopen_wrapper_follow(filename, "r", O_LARGEFILE);
    if (file == NULL) {
        fprintf(stderr, "Can't open file of ClassAds: %s\n", filename);
        return false;
    }

    do {
        ad_pos = ftell(file);
        classad = new ClassAd;
        InsertFromFile(file, *classad, "\n", is_eof, is_error, is_empty);
        if (!is_error && !is_empty) {
            if( !constraint_expr || EvalExprBool(classad, constraint_expr) )
            {
				if( ! (this->*work_func)(classad) ) {
					break;
				}
            }
            else {
                delete classad;
            }
        }
        else {
            delete classad;
        }
        if( is_error ) {
            // As a convenience, skip over errors caused by header lines
            // from condor_q -long.

            long new_pos = ftell(file);
            // jump back to start of error using relative offset in
            // case file size is larger than absolute offset can
            // handle
            if( fseek(file,ad_pos - new_pos,SEEK_CUR) != 0 ) {
                break;
            }
            std::string line;
            while( readLine(line, file) ) {
                trim(line);
                if( line.empty() ) {
                    continue; // ignore blank lines
                }
                else if( line.find("--") == 0 ) {
                    // this looks like a header line from the schedd
                    is_error = 0;
                    break;
                }
                else {
                    fprintf(stderr, "Invalid ClassAd in %s beginning at position %ld, and with first line: %s\n",filename,ad_pos,line.c_str());
                    break;
                }
            }
        }
    } while (!is_eof && !is_error);

    if (is_error) {
        success = false;
    } else {
        success = true;
    }

    fclose(file);
    return success;
}
