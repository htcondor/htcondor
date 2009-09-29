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


#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

//#define EXPERIMENTAL

#define WANT_CLASSAD_NAMESPACE
#include "condor_fix_iostream.h"
#include "classad/classad_distribution.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "boolExpr.h"
#include "resourceGroup.h"
//#include "classadList.h"
#include "explain.h"
#include "interval.h"
#include "boolValue.h"
#include "conversion.h"

#include "result.h"

/// The analyzer object
class ClassAdAnalyzer
{
 public:

		/** Default Constructor */
	ClassAdAnalyzer( );

		/** Destructor */
	~ClassAdAnalyzer( );

	classad_analysis::job::result GetResult() { return *m_result; }

		/** Analyze a job ClassAd requirements expression.
		 *	@param request The job ClassAd
		 *	@param offers A list of machine ClassAds
		 *	@param buffer The results of the analysis
		 *  @return true on success false on failure
		 */
	bool AnalyzeJobReqToBuffer( ClassAd *request, ClassAdList &offers,
								string &buffer );

#if defined( COLLECTIONS )
		/** Analyze a job ClassAd requirements expression.
		 *	@param request The job ClassAd
		 *	@param offers A collection of machine ClassAds
		 *	@param buffer The results of the analysis
		 *	@return true on success false on failure
		 */
	bool AnalyzeJobReqToBuffer( classad::ClassAd *request,
								classad::ClassAdCollectionServer &offers,
								string &buffer );
#endif

		/** Analyze job ClassAd attributes.
		 *	@param request The job ClassAd
		 *	@param offers A list of machine ClassAds
		 *	@param buffer The results of the analysis
		 *	@return true on success false on failure
		 */
	bool AnalyzeJobAttrsToBuffer( ClassAd *request, ClassAdList &offers,
								  string &buffer );

		/** Analyze an arbitrary expression.
		 *	@param mainAd The ClassAd contaning the expression
		 *	@param contextAd The target ClassAd
		 *	@param attr The attribute corresponding to the expression
		 *	@param buffer The results of the analysis
		 *	@return true on success false on failure		 
		 */
	bool AnalyzeExprToBuffer( classad::ClassAd *mainAd,
							  classad::ClassAd *contextAd,
							  string &attr, string &buffer );

 private:
	
	classad_analysis::job::result *m_result;

	MultiProfile *jobReq;
	classad::MatchClassAd mad;

	ExprTree* std_rank_condition;
	ExprTree* preempt_rank_condition;
	ExprTree* preempt_prio_condition;
	ExprTree* preemption_req;

	void ensure_result_initialized(classad::ClassAd *request);

	bool AnalyzeJobReqToBuffer( classad::ClassAd *request, ResourceGroup &offers,
								string &buffer );
	bool AnalyzeJobAttrsToBuffer( classad::ClassAd *request, ResourceGroup &offers,
								  string &buffer );

	bool BuildBoolTable( MultiProfile *, ResourceGroup &, BoolTable &result );
	bool BuildBoolTable( Profile *, ResourceGroup &, BoolTable &result );
	
	bool MakeResourceGroup( ClassAdList &, ResourceGroup &result );

#if defined( COLLECTIONS )
	bool MakeResourceGroup( classad::ClassAdCollectionServer &, ResourceGroup &result );
#endif

	bool NeedsBasicAnalysis( ClassAd *request );
	void BasicAnalyze(ClassAd* request, ClassAd* offer);

	bool SuggestCondition( MultiProfile *, ResourceGroup & );
	bool SuggestConditionRemove( Profile *, ResourceGroup & );
	bool SuggestConditionModify( Profile *, ResourceGroup & );

	bool FindConflicts( MultiProfile *, ResourceGroup & );
	bool FindConflicts( Profile *, ResourceGroup & );

	bool AnalyzeAttributes( classad::ClassAd *, ResourceGroup &,
							ClassAdExplain &result );

	bool EqualsIgnoreCase( const string &, const string & );
	bool DefinedLiteralValue( classad::Value & );
	bool AddConstraint( ValueRange *&, Condition * );
	bool AddDefaultConstraint( ValueRange *&vr );

	bool PruneDisjunction( classad::ExprTree *, classad::ExprTree *& result );
	bool PruneConjunction( classad::ExprTree *, classad::ExprTree *& result );
	bool PruneAtom( classad::ExprTree *, classad::ExprTree *& result );

		// unsupported
//	bool InDNF( classad::ExprTree * );
//	bool IsAtomicBooleanFormula( classad::Operation * );
//	bool PropagateNegation( classad::ExprTree *, classad::ExprTree *&result );
//	bool ToDNF( classad::ExprTree *, classad::ExprTree *&result );
	
};

#endif // __ANALYSIS_H__

