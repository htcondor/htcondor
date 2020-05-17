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
#include "proc.h"              // for job statuses
#include "condor_accountant.h" // for PriorityDelta
#include "condor_config.h"
#include "analysis.h"
#include "list.h"
#include "simplelist.h"
#include "extArray.h"
#include "condor_classad.h"

#include <sstream>

using namespace std;
using namespace classad_analysis;
using namespace classad_analysis::job;

ClassAdAnalyzer::
ClassAdAnalyzer( bool ras ) :
  result_as_struct(ras), m_result(NULL), jobReq(NULL) { 

  stringstream std_rank;
  stringstream preempt_rank;
  stringstream preempt_prio;
  
  std_rank << "MY." << ATTR_RANK << " > MY." << ATTR_CURRENT_RANK;
  preempt_rank << "MY." << ATTR_RANK << " >= MY." << ATTR_CURRENT_RANK;
  preempt_prio << "MY." << ATTR_REMOTE_USER_PRIO << " > TARGET." << ATTR_SUBMITTOR_PRIO << " + " << PriorityDelta;

  ParseClassAdRvalExpr(std_rank.str().c_str(), std_rank_condition);
  ParseClassAdRvalExpr(preempt_rank.str().c_str(), preempt_rank_condition);
  ParseClassAdRvalExpr(preempt_prio.str().c_str(), preempt_prio_condition);
  
  char *preq;
  if( NULL == ( preq = param( "PREEMPTION_REQUIREMENTS" ) ) ) {
    // No PREEMPTION_REQUIREMENTS; defaulting to FALSE
    ParseClassAdRvalExpr( "FALSE", preemption_req );
  } else {
    if( ParseClassAdRvalExpr( preq , preemption_req ) ) {
      // Failed to parse PREEMPTION_REQUIREMENTS; defaulting to FALSE
      ParseClassAdRvalExpr( "FALSE", preemption_req );
    }
    free( preq );
  }
}

ClassAdAnalyzer::
~ClassAdAnalyzer( )
{
	delete std_rank_condition;
	delete preempt_rank_condition;
	delete preempt_prio_condition;
	delete preemption_req;
	if( jobReq ) {
		delete jobReq;
	}
	
	if( m_result ) {
	  delete m_result;
	  m_result = NULL;
	}
}

// Returns "true" if the job is not matched and IDLE, false otherwise
bool ClassAdAnalyzer::
NeedsBasicAnalysis( ClassAd *request ) {
  int status;
  int matched = false;

  request->LookupInteger( ATTR_JOB_STATUS, status );
  request->LookupInteger( ATTR_JOB_MATCHED, matched );

  // XXX: are there cases where we need "basic" analysis even though
  // we're matched?
  if (matched) {
    return false;
  }
  
  switch(status) {
  // XXX:  should we have "is_running/completed/removed" in m_result?
  // XXX:  should we add "is_held" to m_result/include hold_reason?
  case RUNNING:
  case HELD:
  case REMOVED:
  case COMPLETED:
  case TRANSFERRING_OUTPUT:
    return false;
  default:
    return true;
  }
}

void ClassAdAnalyzer::
BasicAnalyze( ClassAd *request, ClassAd *offer ) {
  // XXX: this code could/should be refactored out (from here and
  // condor_q.V6/queue.cpp) and into an analysis library

  // NB:  for now, we only use this "basic analysis" if we're
  // generating a result struct; so return otherwise.  Be sure to
  // eliminate this check if this code is used elsewhere!

  if (!result_as_struct) { return; }

  char remote_user[128];
  classad::Value eval_result;
  bool val;

  bool satisfied_std_rank = EvalExprTree(std_rank_condition, offer, request, eval_result) && eval_result.IsBooleanValue(val) && val;
  bool satisfied_preempt_prio = EvalExprTree( preempt_prio_condition, offer, request, eval_result ) && eval_result.IsBooleanValue(val) && val;
  bool satisfied_preempt_rank = EvalExprTree( preempt_rank_condition, offer, request, eval_result ) && eval_result.IsBooleanValue(val) && val;
  bool satisfied_preempt_req = EvalExprTree( preemption_req, offer, request, eval_result ) && eval_result.IsBooleanValue(val) && val;

  if (!IsAHalfMatch(request, offer)) {
    result_add_explanation(classad_analysis::MACHINES_REJECTED_BY_JOB_REQS, offer);
    return;
  } 

  if (!IsAHalfMatch(offer, request)) {
    result_add_explanation(classad_analysis::MACHINES_REJECTING_JOB, offer);
    return;
  }

  if (! offer->LookupString( ATTR_REMOTE_USER, remote_user, sizeof(remote_user) )) {
    if ( satisfied_std_rank )  {
      // Machine satisfies job requirements, job satisfies machine
      // constraints, no remote user
      result_add_explanation(classad_analysis::MACHINES_AVAILABLE, offer);
      return;
    } else {
      // Standard rank condition failed
      result_add_explanation(classad_analysis::MACHINES_REJECTING_UNKNOWN, offer);
      return;
    }
  }

  if ( satisfied_preempt_prio )  {
    if ( satisfied_std_rank ) {
      // Satisfies preemption priority condition and standard rank
      // condition; thus available
      result_add_explanation(classad_analysis::MACHINES_AVAILABLE, offer);
      return;      
    } else {
      if ( satisfied_preempt_rank ) {
	// Satisfies preemption priority and rank conditions, and ...
	if (satisfied_preempt_req) {
	  // ... also satisfies PREEMPTION_REQUIREMENTS:  available
	  result_add_explanation(classad_analysis::MACHINES_AVAILABLE, offer);
	  return;      	  
	} else {
	  // ... doesn't satisfy PREEMPTION_REQUIREMENTS:  not available
	  result_add_explanation(classad_analysis::PREEMPTION_REQUIREMENTS_FAILED, offer);
	  return;
	}
      } else {
	// The comments on the equivalent code path in condor_q
	// indicate that this case usually implies "some unknown problem"
	result_add_explanation(classad_analysis::PREEMPTION_FAILED_UNKNOWN, offer);
	return;
      }
    }
  } else {
    // Failed preemption priority condition
    result_add_explanation(classad_analysis::PREEMPTION_PRIORITY_FAILED, offer);
    return;
  }

}

void ClassAdAnalyzer::
ensure_result_initialized(classad::ClassAd *request) {
    // Set up result object, only if necessary

    // Other parts of this code are written to assume that a
    // ClassAdAnalyzer can be used to analyze multiple jobs; we make
    // the same assumption here.  The overall interface of this code
    // should be marked with a big FIXME.

    if (!result_as_struct) return;

    if (m_result != NULL && !(m_result->job_ad()).SameAs(request)) {
      delete m_result;
      m_result = NULL;
    }

    if (m_result == NULL) {
      m_result = new classad_analysis::job::result(*request);
    }
}

void ClassAdAnalyzer::
result_add_suggestion(suggestion s) {
  if (!result_as_struct) return;
  ASSERT(m_result);
  
  m_result->add_suggestion(s);
}

void ClassAdAnalyzer::
result_add_explanation(matchmaking_failure_kind mfk, const classad::ClassAd &resource) {
  if (!result_as_struct) return;
  ASSERT(m_result);
  
  m_result->add_explanation(mfk, resource);
}

void ClassAdAnalyzer::
result_add_explanation(matchmaking_failure_kind mfk, ClassAd *resource) {
  if (!result_as_struct) return;
  ASSERT(m_result);
  
  m_result->add_explanation(mfk, resource);
}

void ClassAdAnalyzer::
result_add_machine(const classad::ClassAd &resource) {
  if (!result_as_struct) return;
  ASSERT(m_result);
  
  m_result->add_machine(resource);
}


bool ClassAdAnalyzer::
AnalyzeJobReqToBuffer( ClassAd *request, ClassAdList &offers, string &buffer, std::string &pretty_req )
{
	ResourceGroup     rg;
    classad::ClassAd  *explicit_classad;
    bool              success;

	pretty_req = "";

		// create a ResourceGroup object for offer ClassAds
	if( !MakeResourceGroup( offers, rg ) ) {
		buffer += "Unable to process machine ClassAds";
		buffer += "\n";
		return true;
	}

	explicit_classad  = AddExplicitTargets( request );

    ensure_result_initialized(explicit_classad);
    
    bool do_basic_analysis = NeedsBasicAnalysis(request);
    offers.Rewind();
    ClassAd *ad;
    while((ad = offers.Next())) {
      result_add_machine(*ad);

      if (do_basic_analysis) {
	BasicAnalyze(request, ad);
      }
    }

	success = AnalyzeJobReqToBuffer( explicit_classad, rg, buffer, pretty_req );

    delete explicit_classad;
    return success;
}

#if defined( COLLECTIONS )
bool ClassAdAnalyzer::
AnalyzeJobReqToBuffer( classad::ClassAd *request,
					   classad::ClassAdCollectionServer &offers,
					   string &buffer )
{
	ResourceGroup rg;

		// create a ResourceGroup object for offer ClassAds
	if( !MakeResourceGroup( offers, rg ) ) {
		buffer += "Unable to process machine ClassAds";
		buffer += "\n";
		return true;
	}

	return AnalyzeJobReqToBuffer( request, rg, buffer );
}
#endif

bool ClassAdAnalyzer::
AnalyzeJobAttrsToBuffer( ClassAd *request, ClassAdList &offers,
						 string &buffer )
{
	ResourceGroup     rg;
    classad::ClassAd  *explicit_classad;
    bool              success;

		// create a ResourceGroup object for offer ClassAds
	if( !MakeResourceGroup( offers, rg ) ) {
		buffer += "Unable to process machine ClassAds";
		buffer += "\n";
		return true;
	}

	explicit_classad  = AddExplicitTargets( request );
    ensure_result_initialized(explicit_classad);
	success = AnalyzeJobAttrsToBuffer( explicit_classad, rg, buffer );

    delete explicit_classad;
    return success;
}


bool ClassAdAnalyzer::
AnalyzeJobReqToBuffer( classad::ClassAd *request, ResourceGroup &offers, string &buffer, string &pretty_req)
{
	if( !request ) {
			// request is NULL;
		return false;
	}

	classad::PrettyPrint pp;

	classad::ExprTree *reqExpr, *flatReqExpr, *prunedReqExpr;
	reqExpr = flatReqExpr = prunedReqExpr = NULL;
	classad::Value val;

    if (jobReq != NULL) {
        delete jobReq;
    }
	jobReq = new MultiProfile( );
	Profile *profile = NULL;
	Condition *condition = NULL;

		// Look up Requirements expression in request ClassAd
	if( !( reqExpr = request->Lookup( ATTR_REQUIREMENTS ) ) ) {
		buffer += "Job ClassAd is missing ";
		buffer += ATTR_REQUIREMENTS;
		buffer += " expression.";
		buffer += "\n";
		return true;
	}

		// Print out requirements expression to buffer

		// Format req expression for 80 column screen.
	string temp_buffer;
	pp.Unparse( temp_buffer, reqExpr );
	string::iterator t, lastAnd, lineStart;
	t = lastAnd = lineStart = temp_buffer.begin( );
	while( t != temp_buffer.end( ) ) {
		if( ( *t == '&' ) && ( *( t+1 ) == '&' ) ) {
			lastAnd = t + 2;
		}
		if( distance( lineStart, t ) >= 80 ) {
			if( lastAnd != lineStart ) {
				temp_buffer.replace( lastAnd, lastAnd + 1, 1, '\n' );
				lineStart = lastAnd + 1;
				lastAnd = lineStart;
			}
		}
		t++;
	}
		// Print formatted req expression
	pretty_req += "\n";
	pretty_req += "The ";
	pretty_req += ATTR_REQUIREMENTS; 
	pretty_req += " expression for your job is:";
	pretty_req += "\n";
	pretty_req += "\n";
	pretty_req += temp_buffer;
	pretty_req += "\n";
	pretty_req += "\n";

		// Try to flatten Requirements expression
	mad.ReplaceLeftAd( request );
	if( !( request->FlattenAndInline( reqExpr, val, flatReqExpr ) ) ) {
		return true;
	}
	mad.RemoveLeftAd( );

		// Check if Requirements expression flattened to a literal value
	if( !flatReqExpr ) {
		buffer += "Job ClassAd "; 
		buffer += ATTR_REQUIREMENTS; 
		buffer += " expression evaluates to ";
		pp.Unparse( buffer, val );
		buffer += "\n";
		buffer += "\n";
		return true;
	}

		// Get rid of dangling boolean literals created by Flatten
	if( !( PruneDisjunction( flatReqExpr, prunedReqExpr ) ) ) {
		return true;
	}

		// Convert Requirements expression to a MultiProfile object
	if( !( BoolExpr::ExprToMultiProfile( prunedReqExpr, jobReq ) ) ) {
		return true;
	}

		// Determine which Conditions should be removed from Requierments
		// expression
	if( !( SuggestCondition( jobReq, offers ) ) ) {
		return true;
	}

		// Determine which Conditions conflict with one another
	if( !( FindConflicts( jobReq, offers ) ) ) {
		return true;
	}

		// req->explain.match and req->explain.numberOfMatches contain
		// information about the offers that satisfy the requests requirements 

		// The following should probably make use of ClassAdPrintMask

		// Get information from data structures and print to the buffer
	string cond_s, value_s;
	char formatted[2048];
	char cond[1024];
	char info[64];
	char suggest[128];
	char value[64];
	char tempBuff[64];
	int p = 1;
	jobReq->Rewind( );
	while( jobReq->NextProfile( profile ) ) {

			// If we have more than one profile we need to number them.
		int numProfs;
		jobReq->GetNumberOfProfiles( numProfs );
		if( numProfs > 1 ) {
			buffer += "Profile ";
			sprintf( tempBuff, "%i", p );
			buffer += tempBuff;
			if( profile->explain.match ) {
				buffer += " matched ";
				sprintf( tempBuff, "%i", profile->explain.numberOfMatches );
				buffer += tempBuff;
			} else {
				buffer += " rejected all";
			}
			if( profile->explain.numberOfMatches == 1 ) {
				buffer += " machine";
			} else {
				buffer += " machines";
			}
			buffer += "\n";
		}

			// Sort list of conditions by number of matches
		List< Condition > sortedCondList;
		profile->Rewind( );
		Condition *sortedCond;
		SimpleList< int > mapList;
		int index = 0;
		int junk;
		while( profile->NextCondition( condition ) ) {			
			if( sortedCondList.IsEmpty( ) ) {
				sortedCondList.Append( condition );
				mapList.Append( index );
			} else {
				sortedCondList.Rewind( );
				mapList.Rewind( );
				while( sortedCondList.Next( sortedCond ) ) {
					mapList.Next( junk );
					if( condition->explain.numberOfMatches <
						sortedCond->explain.numberOfMatches ) {
						sortedCondList.Insert( condition );
						mapList.Prepend( index );
						break;
					}
					if( sortedCondList.AtEnd( ) ) {
						sortedCondList.Append( condition );
						mapList.Append( index );
					}
				}
			}
			index++;
		}
		sortedCondList.Rewind( );
		mapList.Rewind( );

			// create map from original Condition order to sorted order
		int numConds = 0;
		profile->GetNumberOfConditions( numConds );
		ExtArray<int> condMap ( numConds );
		int i = 0;
		while( mapList.Next( index ) ) {
			condMap[index] = i;
			i++;
		}

			// print header for condition list
		sprintf( formatted, "    %-34s%-20s%s\n", "Condition",
				 "Machines Matched", "Suggestion" );
		buffer += formatted;
		sprintf( formatted, "    %-34s%-20s%s\n", "---------",
				 "----------------", "----------" );
		buffer += formatted;
		i = 1;

			// print each condition, number of matches, and suggestion
		while( sortedCondList.Next( condition ) ) {
			cond_s = "";
			value_s = "";
			condition->ToString( cond_s );
			strncpy( cond, cond_s.c_str( ), 1023 );
			cond[1023] = 0;

			sprintf( info, "%i", condition->explain.numberOfMatches );

			switch( condition->explain.suggestion ) {
			case ConditionExplain::REMOVE: {
				sprintf( suggest, "REMOVE" );
				result_add_suggestion(suggestion(suggestion::REMOVE_CONDITION, cond_s));
				break;
			}
			case ConditionExplain::MODIFY: {
				pp.Unparse( value_s, condition->explain.newValue );
				result_add_suggestion(suggestion(suggestion::MODIFY_CONDITION, cond_s, value_s));
				strncpy( value, value_s.c_str( ), 63 );
				sprintf( suggest, "MODIFY TO %s", value );
				break;
			}
			default: {
				sprintf( suggest, " " );
			}
			}

			if( strlen( cond ) < 46 ) {
				sprintf( formatted, "%-4i%-34s%-20s%s\n", i, cond, info,
						 suggest );
			} else {
				sprintf( formatted, "%-4i%s\n%38s%-20s%s\n", i, cond, "", info,
						 suggest );
			}

			buffer += formatted;
			i++;
		}

			// print out conflicts
		IndexSet sortedIS;
		IndexSet *rawIS;
		profile->explain.conflicts->Rewind( );
		if( !profile->explain.conflicts->IsEmpty( ) ) {
			buffer += "\n";
			buffer += "Conflicts:\n";
			buffer += "\n";
			while( profile->explain.conflicts->Next( rawIS ) ) {
				sortedIS.Init( numConds );
				IndexSet::Translate( *rawIS, condMap.getarray (), 
									numConds, numConds, sortedIS );
				buffer += "  conditions: ";
				bool firstNum = true;
				for( i = 0; i < numConds; i++ ) {
					if( sortedIS.HasIndex( i ) ) {
						if( !firstNum ) {
							buffer += ", ";
						}
						else {
							firstNum = false;
						}
						sprintf( tempBuff, "%i", i+1 );
						buffer += tempBuff;
					}
				}
				buffer += "\n";
			}
		}	

        
		p++;
	}
	return true;
}

bool ClassAdAnalyzer::
AnalyzeJobAttrsToBuffer( classad::ClassAd *request, ResourceGroup &offers,
						 string &buffer )
						                                        
{
	if( !request ) {
		buffer += "request ClassAd is NULL\n";
			// request is NULL;
		return false;
	}

	classad::PrettyPrint pp;

	ClassAdExplain adExplain;
	char formatted[2048];
	char attr[64];
	char suggest[64];

	if( !( AnalyzeAttributes( request, offers, adExplain ) ) ) {
		errstm << "error in AnalyzeAttributes" << endl << endl;
	}

		// get information from ClassAdExplain

		// print list of undefined job attributes
	if( !adExplain.undefAttrs.IsEmpty( ) ) {
		buffer += "\n";
		buffer += "The following attributes are missing from the job ClassAd:";
		buffer += "\n";
		buffer += "\n";
		string undefAttr = "";
		adExplain.undefAttrs.Rewind( );
		while( adExplain.undefAttrs.Next( undefAttr ) ) {
		  result_add_suggestion(suggestion(suggestion::DEFINE_ATTRIBUTE, undefAttr));
			buffer += undefAttr;
			buffer += "\n";
		}
	}

		// print ideal ranges for attributes
	if( !adExplain.attrExplains.IsEmpty( ) ) {
		string value_s = "";
		string suggest_s = "";
		string tempBuff = "";
		int numModAttrs = 0;

		tempBuff += "\nThe following attributes should be added or modified:";
		tempBuff += "\n";
		tempBuff += "\n";

			// print header for attribute list
		sprintf( formatted, "%-24s%s\n", "Attribute", "Suggestion" );
		tempBuff += formatted;
		sprintf( formatted, "%-24s%s\n", "---------", "----------" );
		tempBuff += formatted;

			// print each attribute and suggestion
		AttributeExplain *attrExplain = NULL;
		adExplain.attrExplains.Rewind( );
		while( adExplain.attrExplains.Next( attrExplain ) ) {
			switch( attrExplain->suggestion ) {
			case AttributeExplain::MODIFY: {
				numModAttrs++;
				strncpy( attr, attrExplain->attribute.c_str( ), 63 );
				if( attrExplain->isInterval ) {
					double lower = 0;
					double upper = 0;
					GetLowDoubleValue( attrExplain->intervalValue, lower );
					GetHighDoubleValue( attrExplain->intervalValue, upper );
					suggest_s = "use a value ";
					if( lower > -( FLT_MAX ) ) { // lower bound exists
						if( attrExplain->intervalValue->openLower ) {
							suggest_s += "> ";
						}
						else {
							suggest_s += ">= ";
						}
						pp.Unparse( value_s,attrExplain->intervalValue->lower);
						suggest_s += value_s;
						value_s = "";
						if( upper < FLT_MAX ) {
							suggest_s += " and ";
						}
					}
					if( upper < FLT_MAX ) { // upper bound exists
						if( attrExplain->intervalValue->openUpper ) {
							suggest_s += "< ";
						}
						else {
							suggest_s += "<= ";
						}
						pp.Unparse( value_s,attrExplain->intervalValue->upper);
						suggest_s += value_s;
						value_s = "";
					}
				}
				else {  // attrExplain has a discrete value
					suggest_s = "change to ";
					pp.Unparse( value_s, attrExplain->discreteValue );
					suggest_s += value_s;
					value_s = "";
				}
				strncpy( suggest, suggest_s.c_str( ), 63 ); 
				sprintf( formatted, "%-24s%s\n", attr, suggest );
				result_add_suggestion(suggestion(suggestion::MODIFY_ATTRIBUTE, attr, suggest_s));
				tempBuff += formatted;
			}
			default: { }
			}
		}
		if( numModAttrs > 0 ) {
			buffer += tempBuff;
		}
	}
	return true;
}

bool ClassAdAnalyzer::
AnalyzeExprToBuffer( classad::ClassAd *mainAd, classad::ClassAd *contextAd, string &attr,
					 string &buffer )
{
	classad::PrettyPrint pp;
	classad::Value val;
	string tempBuff_s = "";
	ResourceGroup rg;
	List<classad::ClassAd> contextList;
	MultiProfile *mp = new MultiProfile;
	Profile *profile = NULL;
	Condition *condition = NULL;
	classad::ExprTree *expr = NULL;
	classad::ExprTree *flatExpr = NULL;
	classad::ExprTree *prunedExpr = NULL;
	char tempBuff[64];
	char formatted[2048];
	string cond_s = "";
	char cond[1024];
	string value_s = "";
	char value[64];

    classad::ClassAd *copyContextAd = (classad::ClassAd *) contextAd->Copy();
	contextList.Append( copyContextAd );

	if( !( rg.Init( contextList ) ) ) {
		errstm << "problem adding job ad to ResourceGroup\n";
	}
	if( !( expr = mainAd->Lookup( attr ) ) ) {
		errstm << "error looking up " << attr << " expression\n";
        delete mp;
		return false;
	}
	
	if( !( mainAd->FlattenAndInline( expr, val, flatExpr ) ) ) {
		errstm << "error flattening machine ad\n";
        delete mp;
		return false;
	}

	if( !flatExpr ) {
		buffer += attr;
		buffer += " expresion flattens to ";
		pp.Unparse( buffer, val );
		buffer += "\n";
        delete mp;
		return true;
	}

	if( !PruneDisjunction( flatExpr, prunedExpr ) ) {
		errstm << "error pruning expression:\n";
		pp.Unparse( tempBuff_s, flatExpr );
		errstm << tempBuff_s << "\n";
        delete mp;
		return false;
	}

	if( !( BoolExpr::ExprToMultiProfile( prunedExpr, mp ) ) ) {
		errstm << "error in ExprToMultiProfile\n";
        delete mp;
		return false;
	}		
	
		// Do analysis
	if( !SuggestCondition( mp, rg ) ) {
		errstm << "error in SuggestCondition\n";
	}
	
		// Print results
	buffer += "\n";
	buffer += "=====================\n";
	buffer += "RESULTS OF ANALYSIS :\n";
	buffer += "=====================\n";
	buffer += "\n";

	buffer += attr;
	buffer += " expression ";
	if( mp->explain.match ) {
		buffer += "is true\n";
	} else {
		buffer += "is not true\n";
	}

	int p = 1;
	int numProfiles;
	mp->Rewind( );
	while( mp->NextProfile( profile ) ) {
		mp->GetNumberOfProfiles( numProfiles );
		if( numProfiles > 1 ) {
			buffer += "  Profile ";
			sprintf( tempBuff, "%i", p );
			buffer += tempBuff;
			if( profile->explain.match ) {
				buffer += " is true\n";
			} else {
				buffer += " is false\n";
			}
		}
		profile->Rewind( );
		while( profile->NextCondition( condition ) ) {
			condition->ToString( cond_s );
			strncpy( cond, cond_s.c_str( ), 1023 );
			cond_s = "";

			if( condition->explain.match ) {
				value_s = "is true";
			} else {
				value_s = "is false";
			}
			strncpy( value, value_s.c_str( ), 63 );
			value_s = "";

			sprintf( formatted, "    %-25s%s\n", cond, value );
			buffer += formatted;
		}
		p++;
	}		
	buffer += "=====================\n";
	buffer += "\n";
    delete mp;
	return true;
}


// private methods

bool ClassAdAnalyzer::
BuildBoolTable( MultiProfile *mp, ResourceGroup &rg, BoolTable &result )
{
	BoolValue bval;
	Profile *profile;
	classad::ClassAd *ad;
	List<classad::ClassAd> contexts;
	int numProfs = 0;
	int numContexts = 0;

	if( !mp->GetNumberOfProfiles( numProfs ) ) {
		errstm << "BuildBoolTable: error calling GetNumberOfProfiles" << endl;
	}

	if( !rg.GetNumberOfClassAds( numContexts ) ) {
		errstm << "BuildBoolTable: error calling GetNumberOfClassAds" << endl;
	}

	if( !rg.GetClassAds( contexts ) ) {
		errstm << "BuildBoolTable: error calling GetClassAds" << endl;
	}

	if( !result.Init( numContexts, numProfs ) ) {
		errstm << "BuildBoolTable: error calling BoolTable::Init" << endl;
	}

	contexts.Rewind( );
	int col = 0;
	while( contexts.Next( ad ) ) {
		int row = 0;
		mp->Rewind( );
		while( mp->NextProfile( profile ) ) {
			profile->EvalInContext( mad, ad, bval );
			result.SetValue( col, row, bval );
			row++;
	    }
		col++;
	}

	return true;
}


bool ClassAdAnalyzer::
BuildBoolTable( Profile *p, ResourceGroup &rg, BoolTable &result ) {

	BoolValue bval;
	Condition *condition;
	classad::ClassAd *ad;
	int numConds = 0;
	int numContexts = 0;

	p->GetNumberOfConditions( numConds );

	rg.GetNumberOfClassAds( numContexts );
	List<classad::ClassAd> contexts;
	rg.GetClassAds( contexts );

	result.Init( numContexts, numConds );

	contexts.Rewind( );
	int col = 0;
	while( contexts.Next( ad ) ) {
		int row = 0;

		p->Rewind( );

		while( p->NextCondition( condition ) ) {
			condition->EvalInContext( mad, ad, bval );
			result.SetValue( col, row, bval );
			row++;
	    }
		col++;
	}

	return true;
}

bool ClassAdAnalyzer::
MakeResourceGroup( ClassAdList &caList, ResourceGroup &rg )
{
	List<classad::ClassAd> newList;
	ClassAd *ad;
	caList.Rewind( );
	ad = caList.Next( );
	while( ad ) {
        classad::ClassAd *explicit_classad;

		explicit_classad  = AddExplicitTargets(ad);
		newList.Append(explicit_classad);
		ad = caList.Next( );
	}
	if( !rg.Init( newList ) ) {
		return false;
	}
	return true;
}


#if defined( COLLECTIONS )
bool ClassAdAnalyzer::
MakeResourceGroup( ClassAdCollectionServer &server, ResourceGroup &rg )
{
	List<classad::ClassAd> newList;
	string q_key;
	classad::ClassAd *tmp;
	LocalCollectionQuery a;
	a.Bind(&server);
	a.Query("root",NULL);
	a.ToFirst();
	bool ret=a.Current(q_key);
	if (ret==true){
		do{
			tmp=server.GetClassAd(q_key);
			newList.Append( (classad::ClassAd*) tmp->Copy( ) );
		}while(a.Next(q_key)==true);
	}
	if( !rg.Init( newList ) ) {
		return false;
	}
	return true;
}
#endif

bool ClassAdAnalyzer::
SuggestCondition( MultiProfile *mp, ResourceGroup &rg )
{
	if( mp == NULL ) {
		errstm << "SuggestCondition: tried to pass null MultiProfile"
			 << endl;
		return false;
	}

	BoolTable bt;
	if( !BuildBoolTable( mp, rg, bt ) ) {
		return false;
	}

		// Get info from BoolTable for MultiProfileExplain
	int mpMatches = 0;
	int numCols = 0;
	bt.GetNumColumns( numCols );
	int currentColTotalTrue;
	IndexSet matchedClassAds;
	matchedClassAds.Init( numCols );
	for( int i = 0; i < numCols; i++ ) {
		bt.ColumnTotalTrue( i, currentColTotalTrue );
		if( currentColTotalTrue > 0 ) {
			mpMatches++;
			matchedClassAds.AddIndex( i );
		}
	}
	if( mpMatches > 0 ) {
		if( !( mp->explain.Init( true, mpMatches, matchedClassAds, 
								 numCols ) ) ) {
			return false;
		}
	}
	else {
		if( !( mp->explain.Init( false, 0, matchedClassAds, numCols ) ) ) {
			return false;
		}
	}

		// call SuggestConditionRemove on all profiles
	Profile *currentProfile;
	mp->Rewind( );
	while( mp->NextProfile( currentProfile ) ) {
		if( !SuggestConditionModify( currentProfile, rg ) ) {
			errstm << "error in SuggestConditionModify" << endl;
			return false;
		}
//		if( !SuggestConditionRemove( currentProfile, rg ) ) {
//			return false;
//		}
	}

	return true;
}


bool ClassAdAnalyzer::
SuggestConditionRemove( Profile *p, ResourceGroup &rg )
{
	List<AnnotatedBoolVector> abvList;
	BoolTable bt;
	Condition *condition;
	BoolValue bval;
	int profileMatches = 0;
	int numRows = 0;
	int numColumns = 0;
	int currentColTotalTrue = 0;
	int numberOfMatches = 0;
	int row = 0;
	bool match = false;

	AnnotatedBoolVector *bestABV = NULL;
	AnnotatedBoolVector *abv = NULL;
	string buffer;

	if( !BuildBoolTable( p, rg, bt ) ) {
		return false;
	}

  	if( !bt.GenerateMaxTrueABVList( abvList ) ) {
  		return false;
  	}
	
		// Get info from BoolTable for Profile
	bt.GetNumRows( numRows );
	bt.GetNumColumns( numColumns );
	for( int i = 0; i < numColumns; i++ ) {
		bt.ColumnTotalTrue( i, currentColTotalTrue );
		if( currentColTotalTrue == numRows ) {
			profileMatches++;
		}
	}

	if( profileMatches > 0 ) {
		if( !( p->explain.Init( true, profileMatches ) ) ) {
			abvList.Rewind( );
			while( abvList.Next( abv ) ) {
				delete abv;
			}
			return false;
		}
	}
	else {
		if( !( p->explain.Init( false, 0 ) ) ) {
			abvList.Rewind( );
			while( abvList.Next( abv ) ) {
				delete abv;
			}
			return false;
		}
	}

		// Get info from BoolTable for ConditionExplains

	p->Rewind( );
	row = 0;
	while( p->NextCondition( condition ) ) {
		bt.RowTotalTrue( row, numberOfMatches );
		if( numberOfMatches == 0 ) {
			match = false;
		}
		else {
			match = true;
		}
		if( !( condition->explain.Init( match, numberOfMatches ) ) ) {
			abvList.Rewind( );
			while( abvList.Next( abv ) ) {
				delete abv;
			}
			return false;
		}
		row++;
	}
	
		// find first ABV with max frequency & max total true	
		// set up ConditionExplains using ABV
	if( !AnnotatedBoolVector::MostFreqABV( abvList, bestABV ) ) {
		errstm << "Analysis::SuggestConditionRemove(): error - bad ABV" << endl;
		abvList.Rewind( );
		while( abvList.Next( abv ) ) {
			delete abv;
		}
		return false;
	}
	else {
		int i = 0;
		p->Rewind( );
		while( p->NextCondition( condition ) ) {
			bestABV->GetValue( i, bval );
			if( bval == TRUE_VALUE ) {
				condition->explain.suggestion = ConditionExplain::KEEP;
			}
			else{
				condition->explain.suggestion = ConditionExplain::REMOVE;
			}
			i++;
		}
	}

	abvList.Rewind( );
	while( abvList.Next( abv ) ) {
		delete abv;
	}
	return true;
}

bool ClassAdAnalyzer::
SuggestConditionModify( Profile *p, ResourceGroup &rg )
{
	List<AnnotatedBoolVector> abvList;
	BoolTable bt;
	ValueTable vt;
	Condition *condition;
	int profileMatches = 0;
	int numConds = 0;
	int numContexts = 0;
	int currentColTotalTrue = 0;
	int numberOfMatches = 0;
	bool match = false;
	classad::MatchClassAd mcad;

	if( !BuildBoolTable( p, rg, bt ) ) {
		return false;
	}

		// Get info from BoolTable for Profile
	bt.GetNumRows( numConds );
	bt.GetNumColumns( numContexts );
	for( int i = 0; i < numContexts; i++ ) {
		bt.ColumnTotalTrue( i, currentColTotalTrue );
		if( currentColTotalTrue == numConds ) {
			profileMatches++;
		}
	}

	if( profileMatches > 0 ) {
		if( !( p->explain.Init( true, profileMatches ) ) ) {
			return false;
		}
	}
	else {
		if( !( p->explain.Init( false, 0 ) ) ) {
			return false;
		}
	}

		// Get info from BoolTable for ConditionExplains
		// set up array of operators and get list of attrs;
	ExtArray< string > attrs;
	ExtArray< ValueRange * > vrs;
	string attr = "";
	ExtArray<int> vr4Cond ( numConds );
	int attrNum = 0;
	int condNum = 0;
	int vrNum = 0;
	ExtArray<classad::Operation::OpKind> ops ( numConds );
	ExtArray<Condition*> conds ( numConds );
//	ExtArray<bool> tooComplex ( numConds ); 
	std::vector<bool> tooComplex( numConds, false);
//	classad::Operation::OpKind op1, op2;
	classad::Value val;
	p->Rewind( );
	while( p->NextCondition( condition ) ) {
		conds[condNum] = condition;
		if( !condition->HasMultipleAttrs( ) ) {
			tooComplex[condNum] = false;

				// Attribute
			condition->GetAttr( attr );
			string currAttr;
			bool seenAttr = false;
			vrNum = 0;
			for( int i = 0; i < attrs.getsize( ); i++ ) {
				currAttr = attrs[i];
				if( EqualsIgnoreCase( attr, currAttr ) ) {
					seenAttr = true;
					vrNum = i;
					break;
				}
			}

			if( !seenAttr ) {
				vrNum = attrNum;
				attrs.resize( attrNum + 1 );
				attrs[attrNum] = attr;
				vrs.resize( attrNum + 1 );
				vrs[vrNum] = NULL;
				attrNum++;
			}

			condition->GetOp( ops[condNum] );
			if( vrs[vrNum] == NULL ) {
				vrs[vrNum] = new ValueRange( );
			}
			AddConstraint( vrs[vrNum], condition );
			vr4Cond[condNum] = vrNum;
		}
		else {
				// we treat a complex condtion as ( attr == true )
			tooComplex[condNum] = true;
			ops[condNum] = classad::Operation::__NO_OP__;

			vrNum = attrNum;
			attrs.resize( attrNum + 1 );
			attrs[attrNum] = "";
			vrs.resize( attrNum + 1 );
			vrs[vrNum] = new ValueRange( );
			attrNum++;

			AddDefaultConstraint( vrs[vrNum] );
			vr4Cond[condNum] = vrNum;
		}

			// get info from BoolTable
		bt.RowTotalTrue( condNum, numberOfMatches );
		if( numberOfMatches == 0 ) {
			match = false;
		}
		else {
			match = true;
		}
		if( !( condition->explain.Init( match, numberOfMatches ) ) ) {
			for( int i = 0; i < vrs.getsize( ); i++ ){
				delete vrs[i];
			}
            return false;
		}
		condNum++;
	}

	int numVRs = attrs.getsize( );
	ExtArray<classad::Value*> nearestValues ( numVRs );
    for( int i = 0; i < numVRs; i++ ) {
		nearestValues[i] = NULL;
	}

	List<classad::ClassAd> contexts;
	rg.GetClassAds( contexts );
	classad::ClassAd *context = NULL;

		// build ValueTable
	vt.Init( numContexts, numVRs );
	for( int row = 0; row < numVRs; row++ ) {
		attr = attrs[row];
		p->Rewind( );
		string currAttr;
		while( p->NextCondition( condition ) ) {
			if( !condition->HasMultipleAttrs( ) ) {
				condition->GetAttr( currAttr );
				if( EqualsIgnoreCase( currAttr, attr ) ) {
					classad::Operation::OpKind op;
					classad::Value c_val;
					condition->GetOp( op );
					condition->GetVal( c_val );
					vt.SetOp( row, op );
					if( condition->IsComplex( ) ) {
						condition->GetOp2( op );
						condition->GetVal2( c_val );
						vt.SetOp( row, op );
					}
				}
			}
		}

		contexts.Rewind( );
		for( int col = 0; col < numContexts; col++ ) {
			(void) contexts.Next( context );
			classad::Value c_val;
			if( tooComplex[row] ){
				BoolValue result;
				conds[row]->EvalInContext( mcad, context, result );
				switch( result ) {
				case TRUE_VALUE: { c_val.SetBooleanValue( true ); break; }
				case FALSE_VALUE: { c_val.SetBooleanValue( false ); break; }
				case UNDEFINED_VALUE: { c_val.SetUndefinedValue( ); break; }
				default: c_val.SetErrorValue( );
				}
			}
			else {
				(void) context->EvaluateAttr( attr, c_val );
			}
			vt.SetValue( col, row, c_val );
		}
	}

		// Calculate distances
	classad::Value::ValueType type;
	classad::Value currPt, currUpper, currLower;
	BoolValue currBVal, tempBVal;
	val.SetUndefinedValue( );
	ExtArray<classad::Value*> tempVals ( numVRs );
	for( int i = 0; i < numVRs; i++ ) {
		tempVals[i] = NULL;
	}
	int closestCtx = -1;
	double currDist = 0, sumDist = 0, minSumDist = (double)numVRs + 1; 
	for( int col = 0; col < numContexts; col++ ) {
		for( int row = 0; row < numVRs; row++ ) {
			currBVal = TRUE_VALUE;
			for( int i = 0; i < numConds; i++ ) {
				if( vr4Cond[i] == row ) {
					bt.GetValue( col, i, tempBVal );
					And( currBVal, tempBVal, currBVal );
				}
			}
			if( currBVal == TRUE_VALUE ) {
				currDist = 0;
			}
			else if( currBVal == UNDEFINED_VALUE ) {
				currDist = 1;
			}
			else {
				vt.GetValue( col, row, currPt );
				type = currPt.GetType( );
				if( type == classad::Value::BOOLEAN_VALUE || 
					type == classad::Value::STRING_VALUE ) {
					currDist = 1;
					tempVals[row] = new classad::Value( );
					tempVals[row]->CopyFrom( currPt );
				}
				else {
					vt.GetUpperBound( row, currUpper );
					vt.GetLowerBound( row, currLower );
					vrs[row]->GetDistance( currPt, currLower, currUpper,
										   currDist, val );
					tempVals[row] = new classad::Value( );
					tempVals[row]->CopyFrom( val );
				}
			}
			sumDist += currDist;
		}

		if( sumDist < minSumDist ) {
			minSumDist = sumDist;
			closestCtx = col;
			for( int i = 0; i < numVRs; i++ ) {
				if( tempVals[i] ) {				
					nearestValues[i] = tempVals[i];
				}
			}
		}
		sumDist = 0;
	}
	
	classad::Value condVal;
	condNum = 0;
  	p->Rewind( );
  	while( p->NextCondition( condition ) ) {
		bt.GetValue( closestCtx, condNum, currBVal );
		if( currBVal == TRUE_VALUE ) {
 			condition->explain.suggestion = ConditionExplain::KEEP;
  		}
  		else if( currBVal == UNDEFINED_VALUE ) {
  			condition->explain.suggestion = ConditionExplain::REMOVE;
		}
		else if( condition->HasMultipleAttrs( ) ) {
			condition->explain.suggestion = ConditionExplain::REMOVE;
		}
		else {
			attrNum = vr4Cond[condNum];
			vt.GetValue( closestCtx, attrNum, val );
			if( nearestValues[attrNum] ) {
				condition->GetVal( condVal );
				condition->explain.suggestion = ConditionExplain::MODIFY;
				if( EqualValue( condVal, *nearestValues[attrNum] ) ) {
					if( ops[condNum] == classad::Operation::LESS_THAN_OP ) {
						IncrementValue( val );
					}
					else if ( ops[condNum] == classad::Operation::GREATER_THAN_OP ) {
						DecrementValue( val );
					}
				}
				condition->explain.newValue.CopyFrom( val );
			}
			else {
				condition->explain.suggestion = ConditionExplain::REMOVE;
			}
  		}
		condNum++;
 	}
	for( int i = 0; i < numVRs; i++ ) {
		if (tempVals[i] != NULL) {
			delete tempVals[i];
		}
	}  
    return true;
}


bool ClassAdAnalyzer::
FindConflicts( MultiProfile *mp, ResourceGroup &rg )
{
	Profile *currentProfile = NULL;
	mp->Rewind( );
	while( mp->NextProfile( currentProfile ) ) {
		if( !FindConflicts( currentProfile, rg ) ) {
			return false;
		}
	}
	return true;
}

bool ClassAdAnalyzer::
FindConflicts( Profile *p, ResourceGroup &rg )
{
	BoolTable bt;
	List< BoolVector > bvList;
	BoolVector *currBV = NULL;
	IndexSet *currIS = NULL;
	BoolValue bval;
	int numConds = 0;

	if( !p->GetNumberOfConditions( numConds ) ) {
		return false;
	}

	if( !BuildBoolTable( p, rg, bt ) ) {
		return false;
	}

	if( !bt.GenerateMinimalFalseBVList( bvList ) ) {
		return false;
	}

	int card;
	bvList.Rewind( );
	while( bvList.Next( currBV ) ) {
		if( currBV == NULL ) {
			delete currIS;
			return false;
		}
		currIS = new IndexSet( );
		currIS->Init( numConds );
		for( int i = 0; i < numConds; i++ ) {
			currBV->GetValue( i, bval );
			if( bval == TRUE_VALUE ) {
				currIS->AddIndex( i );
			}
		}
		currIS->GetCardinality( card );
		if( card > 1 ) {
			p->explain.conflicts->Append( currIS );
		} else {
			delete currIS;
			currIS = NULL;
		}
	}

	return true;
}

bool ClassAdAnalyzer::
AnalyzeAttributes( classad::ClassAd *ad, ResourceGroup &rg, ClassAdExplain &caExplain )
{
	classad::ExprTree *reqExpr = NULL;
	classad::ExprTree *flatReqExpr = NULL;
	classad::ExprTree *prunedReqExpr = NULL;
	classad::Value val;

  	string buffer;
  	classad::PrettyPrint pp;

	List<classad::ClassAd> offerList;
	List<MultiProfile> reqList;
	classad::ClassAd *offer = NULL;
	MultiProfile *currReq = NULL;
	List<AnnotatedBoolVector> abvList;
	AnnotatedBoolVector *abv;
	BoolTable bt;
	ValueRangeTable vrt;
	
		/////////////////////////////////////
		// STEP 1 - CREATE LIST OF OFFER   //
		// REQUIREMENTS FROM RESOURCEGROUP //                   
		/////////////////////////////////////

		// get list of ClassAds from ResourceGroup
	if( !( rg.GetClassAds( offerList ) ) ) {
		errstm << "CA::AA: error with GetClassAds" << endl << endl;
		return false;
	}

  	int numOffers = offerList.Number( );

		// Only do this if SuggestCondition has been run successfully
	if( jobReq ) {
		if( jobReq->explain.numberOfClassAds == numOffers ) { 
			offerList.Rewind( );
			for( int i = 0; i < numOffers; i++ ) {
				offerList.Next( );
				if( !jobReq->explain.matchedClassAds.HasIndex( i ) ) {
					offerList.DeleteCurrent( );
				}
			}
			if( offerList.Number( ) == 0 ) {
				return true;
			}
		}
	}	
				
		// create MultiProfile object for each offer Requirements expression;
	offerList.Rewind( );
	while( offerList.Next( offer ) ) {

		currReq = new MultiProfile( );

		if( !( reqExpr = offer->Lookup( ATTR_REQUIREMENTS ) ) ) {
			errstm << "error looking up requirements" << endl << endl;
            delete currReq;
			reqList.Rewind( );
			while( reqList.Next( currReq ) ) {
				delete currReq;
			}
			return false;
		}

		
		if( !( offer->FlattenAndInline( reqExpr, val, flatReqExpr ) ) ) {
			errstm << "error flattening request" << endl << endl;
            delete currReq;
			reqList.Rewind( );
			while( reqList.Next( currReq ) ) {
				delete currReq;
			}
			return false;
		}

		if( flatReqExpr ) {
				// we have a non-literal boolean expression 

			if( !( PruneDisjunction( flatReqExpr, prunedReqExpr ) ) ) {
				errstm << "error pruning expression:" << endl;
				pp.Unparse( buffer, flatReqExpr );
				errstm << buffer << endl << endl;
				buffer = "";
				delete flatReqExpr;
                delete currReq;
				reqList.Rewind( );
				while( reqList.Next( currReq ) ) {
					delete currReq;
				}
				return false;
			}
			
			if( !( BoolExpr::ExprToMultiProfile( prunedReqExpr, currReq ) ) ) {
				errstm << "error in ExprToMultiProfile" << endl << endl;
				delete flatReqExpr;
				delete prunedReqExpr;
				delete currReq;
				reqList.Rewind( );
				while( reqList.Next( currReq ) ) {
					delete currReq;
				}
				return false;
			}

				// Add MultiProfile to the list
			reqList.Append( currReq );
			delete flatReqExpr;
			delete prunedReqExpr;
		}
		else {

				// we have a literal boolean expression
			if( !( BoolExpr::ValToMultiProfile( val, currReq ) ) ) {
				errstm << "error in ValToMultiProfile" << endl << endl;
				delete currReq;
				reqList.Rewind( );
				while( reqList.Next( currReq ) ) {
					delete currReq;
				}
				return false;
			}
			reqList.Append( currReq );
		}			
	}

		////////////////////////////////////
		// STEP 2 - BUILD DATA STRUCTURES //
		////////////////////////////////////

	int numReqs = reqList.Number( );

	ExtArray<int> firstContext ( numReqs );
    List< string > jobAttrs, undefAttrs, refdAttrs;
	classad::ClassAd::iterator itr;

		// build list of attributes
	for( itr = ad->begin( ); itr != ad->end( ); itr++ ) {
		jobAttrs.Append( new string( itr->first ) );
	}

	int numRefdAttrs = 0;

	int arrayCount = 0;
	int reqNo = 0;
	List< ExtArray< BoolValue > > boolValueArrayList;
	List< ExtArray< ValueRange * > > intervalArrayList;
	ExtArray< BoolValue > literalValues;
	ExtArray< BoolValue > *tempBools = NULL;
	ExtArray< ValueRange * > *tempIntervals = NULL;
	Profile *currProfile;
	Condition *currCondition;

		// iterate through machine requirements statements
	reqList.Rewind( );
	while( reqList.Next( currReq ) ) {
		firstContext[reqNo] = arrayCount;
		literalValues.resize( arrayCount + 1);
		literalValues[arrayCount] = TRUE_VALUE;
			// None of the attribute values contribute to this expression
		if( currReq->IsLiteral( ) ) {
			BoolValue bVal = FALSE_VALUE;
			currReq->GetLiteralValue( bVal );
			literalValues[arrayCount] = bVal;
			if( numRefdAttrs == 0 ) {
				tempBools = NULL;
				tempIntervals = NULL;
			}
			else {
				tempBools = new ExtArray< BoolValue>( numRefdAttrs );
				tempIntervals = new ExtArray< ValueRange * >(numRefdAttrs);
				for( int i = 0; i < numRefdAttrs; i++ ) {
					( *tempBools )[i] = bVal;
					( *tempIntervals )[i] = NULL;
				}
			}
			boolValueArrayList.Append( tempBools );
			intervalArrayList.Append( tempIntervals );
			arrayCount++;
			reqNo++;
			continue;
		}
			

		    // iterate through Profiles in a machine requirements statement
		currReq->Rewind( );
		while( currReq->NextProfile( currProfile ) ) {
			
			if( numRefdAttrs == 0 ) {
				tempBools = NULL;
				tempIntervals = NULL;
			}
			else {
				tempBools = new ExtArray< BoolValue>( numRefdAttrs );
				tempIntervals = new ExtArray< ValueRange * >(numRefdAttrs);
				for( int i = 0; i < numRefdAttrs; i++ ) {
					( *tempBools )[i] = TRUE_VALUE;
					( *tempIntervals )[i] = NULL;
				}
			}

				// iterate through Conditions in a profile
			currProfile->Rewind( );
			while( currProfile->NextCondition( currCondition ) ) {
				if( currCondition->IsComplex( ) && 
					currCondition->HasMultipleAttrs( ) ) {
						// We need a better way to deal with complex Conditions
						// LOOK INTO THIS
					continue;
				}

				string currAttr;
				if( !currCondition->GetAttr( currAttr ) ) {
					errstm << "AA error: couldn't get attribute" << endl;
					exit(1);
				}

					// Try to find attribute in job
				bool attrInJob = false;
				string jobAttr;
				jobAttrs.Rewind( );
				while( jobAttrs.Next( jobAttr ) ) {
					if( EqualsIgnoreCase( currAttr, jobAttr ) ) {
						attrInJob = true;				
						break;
					}
				}

					// See if attribute has been previously referenced
				bool attrInRefdAttrs = false;
				string refdAttr;
				int attrNum = 0;
				refdAttrs.Rewind( );
				while( refdAttrs.Next( refdAttr ) ) { 
					if(EqualsIgnoreCase( currAttr, refdAttr ) ) {
						attrInRefdAttrs = true;
						break;
					}
					attrNum++;
				}

				BoolValue conditionValue;
				if( !currCondition->EvalInContext( mad, ad, conditionValue) ) {
					errstm << "AA error: BoolExpr::EvalInContext failed" << endl;
					exit(1);
				}

					// attribute has been previously encountered.
				if( attrInRefdAttrs && tempBools) {
					BoolValue newValue;
					BoolValue oldValue = ( *tempBools )[attrNum];
					And( oldValue, conditionValue, newValue );
					( *tempBools )[attrNum] = newValue;
					if( ( *tempIntervals )[attrNum] == NULL ) {
						( *tempIntervals )[attrNum] = new ValueRange( );
					}
					AddConstraint( ( *tempIntervals )[attrNum],
								   currCondition );
				}
					// attribute is undefined and has thus far not been in any
					// machine requirements.
				else {
					refdAttrs.Append( new string( currAttr ) );
					numRefdAttrs++;				   
					if( tempBools == NULL ) {
						tempBools = new ExtArray< BoolValue>( numRefdAttrs );
						tempIntervals = new ExtArray< ValueRange * >( numRefdAttrs );
						for( int i = 0; i < numRefdAttrs; i++ ) {
							( *tempBools )[i] = TRUE_VALUE;
							( *tempIntervals )[i] = NULL;
						}
					}
					else {
						tempBools->resize( numRefdAttrs );
						tempIntervals->resize( numRefdAttrs );
						(*tempIntervals)[numRefdAttrs - 1] = NULL;
					}
					( *tempBools )[numRefdAttrs - 1] = conditionValue;

					if( ( *tempIntervals )[attrNum] == NULL ) {
						( *tempIntervals )[attrNum] = new ValueRange( );
					}
					AddConstraint( ( *tempIntervals )[attrNum], 
								   currCondition );
				}

					// attribute is not defined in job
				if( !attrInJob ) {
					if( undefAttrs.IsEmpty( ) ) {
						undefAttrs.Append( new string( currAttr ) );
					}
					else {
						string undefAttr = "";
						bool foundAttr = false;
						undefAttrs.Rewind( );
						while( undefAttrs.Next( undefAttr ) ) { 
							if( EqualsIgnoreCase( currAttr, undefAttr ) ) {
								foundAttr = true;
								break;
							}
						}
						if( !foundAttr ) {
							undefAttrs.Append( new string( currAttr ) );
						}
					}
				}
			}
			boolValueArrayList.Append( tempBools );
			intervalArrayList.Append( tempIntervals );
			arrayCount++;
		}
		reqNo++;
	}

	string* attr;
	jobAttrs.Rewind( );
	while( jobAttrs.Next( attr ) ) {
		delete attr;
	}
	

		// Convert machine map
	int numContexts = boolValueArrayList.Number();
	ExtArray<int> machineForContext ( numContexts );
    int m = 0;
  	for( int i = 0; i < numContexts; i++ ) {
  	    if( ( m+1 ) < numReqs) {
  			if( i >= firstContext[m+1] ) {
  				m++;
  			}
  	    }
  	    machineForContext[i] = m;
  	}
    
		// Create BoolTable and ValueRangeTable
	bt.Init( numContexts, numRefdAttrs );
	vrt.Init( numContexts, numRefdAttrs );
	ExtArray< BoolValue > *currBVArray;
	ExtArray< ValueRange * > *currVRLArray;
	boolValueArrayList.Rewind( );
	intervalArrayList.Rewind( );
	for( int ctxtNum = 0; ctxtNum < numContexts; ctxtNum++ ) {
		boolValueArrayList.Next( currBVArray );
		intervalArrayList.Next( currVRLArray );
		int arraySize = 0;
		if( currBVArray != NULL ) {
			arraySize = currBVArray->getsize( );
		}
		for( int attrNum = 0; attrNum < numRefdAttrs; attrNum++ ) {
			if( attrNum >= arraySize ) {
				bt.SetValue( ctxtNum, attrNum, literalValues[ctxtNum] );
				vrt.SetValueRange( ctxtNum, attrNum, NULL ); 
			}
			else {
				bt.SetValue( ctxtNum, attrNum, (*currBVArray)[ attrNum ] );
				vrt.SetValueRange( ctxtNum, attrNum,
									  ( *currVRLArray )[ attrNum ] );
			}
		}
	}

  	if( !bt.GenerateMaxTrueABVList( abvList ) ) {
		cout << "CA::AA: error in GenerateMaxTrueABVList" << endl;
		reqList.Rewind( );
		while( reqList.Next( currReq ) ) {
			if( currReq ) {
				delete currReq;
			}
		}
		boolValueArrayList.Rewind( );
		while( boolValueArrayList.Next( tempBools ) ) {
			if( tempBools ) {
				delete tempBools;
			}
		}
		intervalArrayList.Rewind( );
		while( intervalArrayList.Next( tempIntervals ) ) {
			if( tempIntervals ) {
				for( int i = 0; i < tempIntervals->getsize( ); i++ ) {
					if( ( *tempIntervals )[i] ) {
						delete ( *tempIntervals )[i];
					}
				}
				delete tempIntervals;
			}
		}
		refdAttrs.Rewind( );
		while( refdAttrs.Next( attr ) ) {
			delete attr;
		}
		undefAttrs.Rewind( );
		while( undefAttrs.Next( attr ) ) {
			delete attr;
		}
  		return false;
  	}


	///////////////////////////////////////
	// STEP 3 - GENERATE HYPERRECTANGLES //
	///////////////////////////////////////
	// find values corresponding to each ABV

	AnnotatedBoolVector *currentABV = NULL;
	List< ExtArray< HyperRect * > > allHyperRectangles;
	abvList.Rewind( );
	while( abvList.Next( currentABV ) ) {
	    ExtArray< ValueRange * > vrForAttr;
		vrForAttr.resize( numRefdAttrs );
		for( int i = 0; i < numRefdAttrs; i++ ) {
			vrForAttr[i] = NULL;
		}

	    // go through each attribute/boolean value (only look at non-true)
	    for( int i = 0; i < numRefdAttrs; i++ ) {
			ValueRange *mergedVR = new ValueRange( );
			
			BoolValue bval;
			currentABV->GetValue( i, bval ); 

			if( bval != TRUE_VALUE ) {
				ValueRange *vr = NULL;
				int context;
				bool firstContextBool = true;

					// - go through all contexts corresponding to current ABV
					// - merge the ValueRanges into one multiply indexed 
					//   ValueRange
				for( int j = 0; j < numContexts; j++ ) {
					bool hasContext = false;
					currentABV->HasContext( j, hasContext );
					if( hasContext ) {
						vrt.GetValueRange( j, i, vr );
						context = j;
						if( firstContextBool ) {
							if( vr != NULL ) {
								mergedVR->Init( vr, context, numContexts );
								firstContextBool = false;
							}
						}
						else if( vr != NULL ) {							
							mergedVR->Union( vr, context );
						}
					}
				}
				vrForAttr[i] = mergedVR;
		    }
			else {
				delete mergedVR;
				vrForAttr[i] = NULL;
			}
		}

				// sticks all HyperRectangles together
				// may want to keep them partitioned by BoolVector

		ValueRange::BuildHyperRects( vrForAttr, numRefdAttrs,
									numContexts, allHyperRectangles );
		for( int i = 0; i < vrForAttr.getsize( ); i++ ) {
			if( vrForAttr[i] ) {
				delete vrForAttr[i];
			}
		}

	}
	
  	ExtArray< HyperRect * > *hrs = NULL;
  	HyperRect *currHR = NULL;

	// find HyperRect with most contexts (machines)
	int maxNumContexts = -1;
	int currNumContexts = 0;
	currentABV = NULL;
	hrs = NULL;
	currHR = NULL;
	HyperRect *bestHR = NULL;
	IndexSet hasContext;
	IndexSet hasMachine;
	hasContext.Init( numContexts );
	hasMachine.Init( numReqs );
	abvList.Rewind( );
	allHyperRectangles.Rewind( );
	while( allHyperRectangles.Next( hrs ) ) {
		(void) abvList.Next( currentABV );
		for( int i = 0; i < hrs->getsize( ); i++ ) {
			currHR = ( *hrs )[i];
			currHR->GetIndexSet( hasContext );
			IndexSet::Translate( hasContext, machineForContext.getarray (), 
								numContexts, numReqs, hasMachine );
			hasMachine.GetCardinality( currNumContexts );
			if( currNumContexts > maxNumContexts ) {
				maxNumContexts = currNumContexts;
				bestHR = currHR;
			}
		}
	}
    
	///////////////////////////////////////
	// STEP 4 - SET UP ATTRIBUTE EXPLAIN //
	///////////////////////////////////////

	List< AttributeExplain> attrExplains;
	AttributeExplain *currAttrExplain;
	string refdAttr = "";
	Interval *ival = NULL;
	refdAttrs.Rewind( );
	int i = 0;
	while( bestHR && refdAttrs.Next( refdAttr ) ) { 
		bestHR->GetInterval( i, ival );
		currAttrExplain = new AttributeExplain( );
		if( ival == NULL ) {
			currAttrExplain->Init( refdAttr );
		}
		else {
			switch( GetValueType( ival ) ) {
			case classad::Value::BOOLEAN_VALUE:
			case classad::Value::STRING_VALUE: {
				currAttrExplain->Init( refdAttr, ival->lower );
				break;
			}
			case classad::Value::INTEGER_VALUE:
			case classad::Value::REAL_VALUE:
			case classad::Value::RELATIVE_TIME_VALUE:
			case classad::Value::ABSOLUTE_TIME_VALUE: {
				double low = 0;
				double high = 0;
				GetLowDoubleValue( ival, low );
				GetHighDoubleValue( ival, high );
				if( low == high && !ival->openLower && !ival->openUpper ) {
					currAttrExplain->Init( refdAttr, ival->lower );
				}
				else {
					currAttrExplain->Init( refdAttr, ival );
				}
				break;
			}
			default: {
				currAttrExplain->Init( refdAttr, ival->lower );
				break;
			}
			}
		}
		attrExplains.Append( currAttrExplain );
		i++;
	}

	if( !caExplain.Init( undefAttrs, attrExplains ) ) {
		cout << "error with ClassAdExplain::Init" << endl;
		reqList.Rewind( );
		while( reqList.Next( currReq ) ) {
			delete currReq;
		}
		refdAttrs.Rewind( );
		while( refdAttrs.Next( attr ) ) {
			delete attr;
		}
		undefAttrs.Rewind( );
		while( undefAttrs.Next( attr ) ) {
			delete attr;
		}
		abvList.Rewind( );
		while( abvList.Next( abv ) ) {
			delete abv;
		}
		return false;
	}

	reqList.Rewind( );
	while( reqList.Next( currReq ) ) {
		delete currReq;
	}
	refdAttrs.Rewind( );
	while( refdAttrs.Next( attr ) ) {
		delete attr;
	}
	undefAttrs.Rewind( );
	while( undefAttrs.Next( attr ) ) {
		delete attr;
	}
	abvList.Rewind( );
	while( abvList.Next( abv ) ) {
		delete abv;
	}
	return true;
}

bool ClassAdAnalyzer::
EqualsIgnoreCase( const string &s1, const string &s2 )
{
	return( strcasecmp( s1.c_str( ), s2.c_str( ) ) == 0 );
}

bool ClassAdAnalyzer::
DefinedLiteralValue( classad::Value &val )
{
	return ( val.IsStringValue( ) ||
			 val.IsBooleanValue( ) ||
			 val.IsNumber( ) ||
			 val.IsAbsoluteTimeValue( ) ||
			 val.IsRelativeTimeValue( ) );
}

bool ClassAdAnalyzer::
AddConstraint( ValueRange *&vr, Condition *condition )
{
	classad::PrettyPrint pp;
	string buffer;
	if( condition == NULL ) {
		errstm << "Error: passed NULL Condition pointer to AddConstraint"
			 << endl;
		return false;
	}

	if( vr == NULL ) {
		errstm << "Error: passed NULL ValueRange pointer to AddConstraint"
			 << endl;
		return false;
	}

	if( condition->IsComplex( ) && condition->HasMultipleAttrs( ) ) {
		errstm << "AddConstraint: can't process complex Condition:" << endl;
		condition->ToString( buffer );
		errstm << buffer << endl;
		return false;
	}

	classad::Operation::OpKind op = (classad::Operation::OpKind)0;
	classad::Value val;
	bool undef = false;
	bool twoVals = false;
	classad::Value val1, val2;

	if( condition->IsComplex( ) && !condition->HasMultipleAttrs( ) ) {
		classad::Operation::OpKind op1, op2;
		condition->GetOp( op1 );
		condition->GetOp2( op2 );
		condition->GetVal( val1 );
		condition->GetVal2( val2 );

		undef = false;
		if( val1.IsUndefinedValue( ) &&
			DefinedLiteralValue( val2 ) ) {
			undef = true;
			val.CopyFrom( val2 );
			op = op2;
		}
		else if( val2.IsUndefinedValue( ) &&
			DefinedLiteralValue( val1 ) ) {
			undef = true;
			val.CopyFrom( val1 );
			op = op1;
		}
		else {
			classad::Value::ValueType type1 = val1.GetType( );
			classad::Value::ValueType type2 = val2.GetType( );
//			if( !SameType( type1, type2 ) ||
//				!DefinedLiteralValue( val1 ) ||
//				val1.IsStringValue ||
//				val1.IsBooleanValue ) {
			if( DefinedLiteralValue( val1 ) &&
				DefinedLiteralValue( val2 ) &&
				op1 == classad::Operation::EQUAL_OP &&
				op2 == classad::Operation::EQUAL_OP &&
				SameType( type1, type2 ) ) {
				twoVals = true;
			}
			else {
				errstm << "AddConstraint: can't process complex Condition"<<endl;
				pp.Unparse( buffer, val1 );
				errstm << "val1 is " << buffer << endl;
				buffer = "";
				pp.Unparse( buffer, val2 );
				errstm << "val2 is " << buffer << endl;
				buffer = "";
                condition->ToString( buffer );
                errstm << buffer << endl;
                return false;
			}
//			}
		}
	}

	if( !condition->IsComplex( ) ) {
		condition->GetOp( op );
		condition->GetVal( val );
	}

	classad::Value::ValueType type = val.GetType( );
	if( twoVals ) {
		Interval *i1 = new Interval( );
		Interval *i2 = new Interval( );
		if(i1) {
			if(i2) {
				i1->lower.CopyFrom( val1 );
				i2->lower.CopyFrom( val2 );
				i1->upper.CopyFrom( val1 );
				i2->upper.CopyFrom( val2 );
				i1->openLower = false;
				i2->openLower = false;
				i1->openUpper = false;
				i2->openUpper = false;
				if( vr->IsInitialized( ) ) {
					vr->Intersect2( i1, i2, false );
				}
				else {
					vr->Init( i1, i2, false );
				}
			} else {
				delete i1;
				i1=0;
			}
		}
		if(i1 && i2) {
			delete i1;
			delete i2;
		}
	}
	else if( op == classad::Operation::NOT_EQUAL_OP || 
			 op == classad::Operation::ISNT_OP ) {
			// we need multiple intervals
		switch( type ) {
		case classad::Value::UNDEFINED_VALUE: {
			if( op != classad::Operation::ISNT_OP ) {
				vr->EmptyOut( );
				return true;
			}
			if( vr->IsInitialized( ) ) {
				vr->IntersectUndef( false );
			}
			else {
				vr->InitUndef( false );
			}
			return true;
		}
		case classad::Value::BOOLEAN_VALUE: {
			bool b;
			if( val.IsBooleanValue( b ) ) {
				Interval *i = new Interval( );
				i->lower.SetBooleanValue( !b );
				if( vr->IsInitialized( ) ) {
					if( op == classad::Operation::ISNT_OP ) {
						vr->Intersect( i, true );
					}
					else {
						vr->Intersect( i, undef );
					}
				}
				else {
					if( op == classad::Operation::ISNT_OP ) {
						vr->Init( i, true );
					}
					else {
						vr->Init( i, undef );
					}
				}
				delete i;
				return true;
			}
			else {
				errstm << "AddConstraint: error: boolean value expected"
					 << endl;
				return false;
			}
		}
		case classad::Value::STRING_VALUE: {
			Interval *i = new Interval( );
			i->lower.CopyFrom( val );
			if( vr->IsInitialized( ) ) {
				if( op == classad::Operation::ISNT_OP ) {
					vr->Intersect( i, true, true );
				}
				else {
					vr->Intersect( i, undef, true );
				}
			}
			else {
				if( op == classad::Operation::ISNT_OP ) {
					vr->Init( i, true, true );
				}
				else {
					vr->Init( i, undef, true );
				}
			}
			delete i;
			return true;
		}
		case classad::Value::INTEGER_VALUE:
		case classad::Value::REAL_VALUE:
		case classad::Value::RELATIVE_TIME_VALUE:
		case classad::Value::ABSOLUTE_TIME_VALUE: {
			Interval *i1 = new Interval( );
			Interval *i2 = new Interval( );
			i1->lower.SetRealValue( -( FLT_MAX ) );
			i1->upper.CopyFrom( val );
			i1->openLower = false;
			i1->openUpper = false;
			i2->lower.CopyFrom( val );
			i2->upper.SetRealValue( FLT_MAX );
			i2->openLower = false;
			i2->openUpper = false;
			if( vr->IsInitialized( ) ) {
				if( op == classad::Operation::ISNT_OP ) {
					vr->Intersect2( i1, i2, true );
				}
				else {
					vr->Intersect2( i1, i2, undef );
				}
			}
			else {
				if( op == classad::Operation::ISNT_OP ) {
					vr->Init2( i1, i2, true );
				}
				else {
					vr->Init2( i1, i2, undef );
				}
			}
			delete i1;
			delete i2;
		}
		return true;
		default: {
			string expr;
			condition->ToString(expr);
			errstm << "AddConstraint: Condition value not literal: '" << val << "' in '" << expr << "'" << endl;
			return false;
		}
		}
	}
	else {
			// op is not '!=' or 'isnt' so we only need one interval
		Interval *i = new Interval( );
		switch( type ) {
		case classad::Value::UNDEFINED_VALUE:
			if( op != classad::Operation::IS_OP ) {
				vr->EmptyOut( );
				delete i;
				return true;
			}
			if( vr->IsInitialized( ) ) {
				vr->IntersectUndef( );
			}
			else {
				vr->InitUndef( );
			}
			delete i;
			return true;
		case classad::Value::BOOLEAN_VALUE:
		case classad::Value::STRING_VALUE: {
			if( op != classad::Operation::EQUAL_OP && op != classad::Operation::IS_OP ) {
					// should be type error
				vr->EmptyOut( );
				delete i;
				return true;
			}
			i->lower.CopyFrom( val );
			if( vr->IsInitialized( ) ) {
				vr->Intersect( i, undef );
			}
			else {
				vr->Init( i, undef );
			}
			delete i;
			return true;
		}
		case classad::Value::INTEGER_VALUE:
		case classad::Value::REAL_VALUE:
		case classad::Value::RELATIVE_TIME_VALUE:
		case classad::Value::ABSOLUTE_TIME_VALUE: {
			switch( op ) {
			case classad::Operation::LESS_THAN_OP: 
				{
					i->lower.SetRealValue( -( FLT_MAX ) );
					i->upper.CopyFrom( val );
					i->openLower = true;
					i->openUpper = true;
					if( vr->IsInitialized( ) ) {
						vr->Intersect( i, undef );
					}
					else {
						vr->Init( i, undef );
					}
					delete i;
					return true;
				}
			case classad::Operation::LESS_OR_EQUAL_OP:
				{
					i->lower.SetRealValue( -( FLT_MAX ) );
					i->upper.CopyFrom( val );
					i->openLower = true;
					i->openUpper = false;
					if( vr->IsInitialized( ) ) {
						vr->Intersect( i, undef );
					}
					else {
						vr->Init( i, undef );
					}
					delete i;
					return true;
				}
			case classad::Operation::EQUAL_OP:
			case classad::Operation::IS_OP:
				{
					i->lower.CopyFrom( val );
					i->upper.CopyFrom( val );
					i->openLower = false;
					i->openUpper = false;
					if( vr->IsInitialized( ) ) {
						vr->Intersect( i, undef );
					}
					else {
						vr->Init( i, undef );
					}
					delete i;
					return true;
				}
			case classad::Operation::GREATER_OR_EQUAL_OP:
				{
					i->lower.CopyFrom( val );
					i->upper.SetRealValue( FLT_MAX );
					i->openLower = false;
					i->openUpper = true;
					if( vr->IsInitialized( ) ) {
						vr->Intersect( i, undef );
					}
					else {
						vr->Init( i, undef );
					}
					delete i;
					return true;
				}
			case classad::Operation::GREATER_THAN_OP:
				{
					i->lower.CopyFrom( val );
					i->upper.SetRealValue( FLT_MAX );
					i->openLower = true;
					i->openUpper = true;
					if( vr->IsInitialized( ) ) {
						vr->Intersect( i, undef );
					}
					else {
						vr->Init( i, undef );
					}
					delete i;
					return true;
				}
			default:
				{
						// should be type error
					if( vr->IsInitialized( ) ) {						
						vr->EmptyOut( );	
					}
					delete i;
					return true;
				}
			}
		}
		default:
			{
					// should be type error
				if( vr->IsInitialized( ) ) {						
					vr->EmptyOut( );
				}
				delete i;
				return true;
			}
		}
	}
	
	return true;
}


bool ClassAdAnalyzer::
AddDefaultConstraint( ValueRange *&vr )
{
	Interval *i = new Interval( );
	i->lower.SetBooleanValue( true );
	if( vr->IsInitialized( ) ) {
		vr->Intersect( i );
	}
	else {
		vr->Init( i );
	}
	delete i;
	return true;	
}

bool ClassAdAnalyzer::
PruneDisjunction( classad::ExprTree *expr, classad::ExprTree *&result )
{
	if( !expr ) {
		errstm << "PD error: null expr" << endl; 
		return false;
	}

	classad::ExprTree::NodeKind kind;
	classad::Operation::OpKind op;
	classad::ExprTree *left, *right, *junk;
	classad::ExprTree *newLeft = NULL;
	classad::ExprTree *newRight = NULL;
	classad::Value val;
	bool boolValue;

	classad::ExprTree *currentTree = expr;

	kind = currentTree->GetKind( );
	if( kind != classad::ExprTree::OP_NODE ) {
		return PruneAtom( currentTree, result );
	}

	( ( classad::Operation * )currentTree )->GetComponents( op, left, right, junk );
	
	if( op == classad::Operation::PARENTHESES_OP ) {
		if( !PruneDisjunction( left, result ) ) {
			return false;
		}
		else if( !( result=classad::Operation::MakeOperation( classad::Operation::PARENTHESES_OP,
													 result, NULL, NULL ) ) ) {
			errstm << "PD error: can't make Operation" << endl;
			return false;
		}
		else {
			return true;
		}
	}

	if( op != classad::Operation::LOGICAL_OR_OP ) {
		return PruneConjunction( currentTree, result );
	}

	kind = left->GetKind( );
	if( kind == classad::ExprTree::LITERAL_NODE ) {
		( ( classad::Literal * )left )->GetValue( val );
		if( val.IsBooleanValue( boolValue ) && boolValue == false ) {
			return PruneDisjunction( right, result );
		}
	}

	if( !PruneDisjunction( left, newLeft ) ||
		!PruneConjunction( right, newRight ) ||
		!newLeft || !newRight ||
		!( result = classad::Operation::MakeOperation( classad::Operation::LOGICAL_OR_OP,
											  newLeft, newRight, NULL ) ) ) {
		errstm << "PD error: can't make Operation" << endl;
		return false;
	}
	return true;
}

bool ClassAdAnalyzer::
PruneConjunction( classad::ExprTree *expr, classad::ExprTree *&result ) {
	if( !expr ) {
		errstm << "PC error: null expr" << endl; 
		return false;
	}

	classad::ExprTree::NodeKind kind;
	classad::Operation::OpKind op;
	classad::ExprTree *left, *right, *junk;
	classad::ExprTree *newRight = NULL;
	classad::ExprTree *newLeft = NULL;
	classad::Value val;
	bool boolValue;

	classad::ExprTree *currentTree = expr;

	kind = currentTree->GetKind( );
	if( kind != classad::ExprTree::OP_NODE ) {		
		return PruneAtom( currentTree, result );
	}
		
	( ( classad::Operation * )currentTree )->GetComponents( op, left, right, junk );

	if( op == classad::Operation::PARENTHESES_OP ) {
		if( !PruneConjunction( left, result ) ) {
			return false;
		}
		else if( !( result=classad::Operation::MakeOperation( classad::Operation::PARENTHESES_OP,
													 result, NULL, NULL ) ) ) {
			errstm << "PC error: can't make Operation" << endl;
			return false;
		}
		else {
			return true;
		}
	}

	if( op != classad::Operation::LOGICAL_AND_OP && op != classad::Operation::LOGICAL_OR_OP ) {
		return PruneAtom( currentTree, result );
	}

	if( op == classad::Operation::LOGICAL_OR_OP ) {
		return PruneDisjunction( currentTree, result );
	}

	kind = left->GetKind( );
	if( kind == classad::ExprTree::LITERAL_NODE ) {
		( ( classad::Literal * )left )->GetValue( val );
		if( val.IsBooleanValue( boolValue ) && boolValue == true ) {
			return PruneConjunction( right, result );
		}
	}

	if( !PruneConjunction( left, newLeft ) ||
		!PruneDisjunction( right, newRight ) ||
		!newLeft || !newRight  ||
		!( result = classad::Operation::MakeOperation( classad::Operation::LOGICAL_AND_OP,
											  newLeft, newRight, NULL ) ) ) {
		errstm << "PC error: can't Make Operation" << endl;
		return false;
	}
	return true;
}

bool ClassAdAnalyzer::
PruneAtom( classad::ExprTree *expr, classad::ExprTree *&result )
{
	if( !expr ) {
		errstm << "PA error: null expr" << endl; 
		return false;
	}

	classad::ExprTree::NodeKind kind;
	classad::Operation::OpKind op;
	classad::ExprTree *left, *right, *junk;
	classad::Value val;
	bool boolValue;
	string attr;

	kind = expr->GetKind( );

	if( kind != classad::ExprTree::OP_NODE ) {
		result = expr->Copy( );
		return true;
	}

	( ( classad::Operation * )expr )->GetComponents( op, left, right, junk );

	if( op == classad::Operation::PARENTHESES_OP ) {
		if( !PruneAtom( left, result ) ) {
			errstm << "PA error: problem with expression in parens" << endl;
			return false;
		}
		else if( !( result=classad::Operation::MakeOperation(classad::Operation::PARENTHESES_OP, 
													result, NULL, NULL ) ) ) {
			errstm << "PA error: can't make Operation" << endl;
			return false;
		}
		else {
			return true;
		}
	}

	if( op == classad::Operation::LOGICAL_OR_OP &&
		left->GetKind( ) == classad::ExprTree::LITERAL_NODE ) {
		( ( classad::Literal *)left )->GetValue( val );
		if( val.IsBooleanValue( boolValue ) && !boolValue ) {
			return PruneAtom( right, result );
		}
	}

	if( left == NULL || right == NULL ) {
		errstm << "PA error: NULL ptr in expr" << endl;
			// error: NULL ptr in expr
		return false;
	}

	if( !( result = classad::Operation::MakeOperation( op, left->Copy( ), right->Copy(),
											  NULL ) ) ) {
		errstm << "PA error: can't make Operation" << endl;
		return false;
	}

	return true;
}

/*
bool ClassAdAnalyzer::
InDNF( classad::ExprTree * tree )
{
	if( tree == NULL ) {
		errstm << "InDNF: tried to pass null pointer" << endl;
		return false;
	}

	if( tree->GetKind( ) != classad::ExprTree::OP_NODE ) {
		return false;
	}
	
	classad::Operation::OpKind op;
	classad::ExprTree *arg1;
	classad::ExprTree *arg2;
	classad::ExprTree *junk;

	( ( classad::Operation *)tree )->GetComponents( op, arg1, arg2, junk );

		// all comparisons are atomic formulas
	if( op >= classad::Operation::__COMPARISON_START__ &&
		op <= classad::Operation::__COMPARISON_END__ ) {
		return true;
	}

	if( op == classad::Operation::PARENTHESES_OP ) {
		return InDNF( arg1 );
	}

		// operators must be comparison or logical
	if( op < classad::Operation::__LOGIC_START__ ||
		op > classad::Operation::__LOGIC_END__ ) {
		return false;
	}

		// FINISH THIS CODE

	return true;
}

bool ClassAdAnalyzer::
IsAtomicBooleanFormula( classad::Operation *tree )
{
	if( tree == NULL ) {
		errstm << "IsAtomicBooleanFormula: tried to pass null pointer" << endl;
		return false;
	}

		// FINISH THIS CODE

	return true;
}

bool ClassAdAnalyzer::
PropagateNegation( classad::ExprTree *tree, classad::ExprTree *&result )
{
	if( tree == NULL ) {
		errstm << "PropagateNegation: tried to pass null pointer" << endl;
		return false;
	}

		// FINISH THIS CODE

	return true;
}

bool ClassAdAnalyzer::
ToDNF( classad::ExprTree *tree, classad::ExprTree *&result )
{
	if( tree == NULL ) {
		errstm << "ToDNF: tried to pass null pointer" << endl;
		return false;
	}
		// FINISH THIS CODE

	return true;
}

*/
