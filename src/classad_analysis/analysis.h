/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

//#define EXPERIMENTAL

#define WANT_NAMESPACES
#include "classad_distribution.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "boolExpr.h"
#include "resourceGroup.h"
//#include "classadList.h"
#include "explain.h"
#include "interval.h"
#include "boolValue.h"
#include "conversion.h"

/// The analyzer object
class ClassAdAnalyzer
{
 public:

		/** Default Constructor */
	ClassAdAnalyzer( );

		/** Destructor */
	~ClassAdAnalyzer( );

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
	MultiProfile *jobReq;
	classad::MatchClassAd mad;

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

