/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "common.h"
#include "exprTree.h"

BEGIN_NAMESPACE( classad )

//-------------classad templates --------------
	// lex buffer
template class ExtArray<char>;
	// function table
template class map<string, void*, CaseIgnLTStr>;
template class map<string, void*, CaseIgnLTStr>::iterator;
	// attribute list
template class hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr>;
template class hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr>::iterator;
template class hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr>::const_iterator;
	// expr evaluation cache
template class hash_map<const ExprTree*, Value, ExprHash >;
template class hash_map<const ExprTree*, Value, ExprHash >::iterator;
	// component stuff
template class vector< pair<string, ExprTree*> >;
template class vector<ExprTree*>;

class _ClassAdInit 
{
	public:
		_ClassAdInit( ) { tzset( ); }
} __ClassAdInit;


END_NAMESPACE
