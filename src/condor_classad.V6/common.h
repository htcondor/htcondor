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

#ifndef __COMMON_H__
#define __COMMON_H__

#if defined( WANT_NAMESPACES ) && defined(__cplusplus)
#define BEGIN_NAMESPACE( x ) namespace x {
#define END_NAMESPACE }
#else
#define BEGIN_NAMESPACE( x )
#define END_NAMESPACE
#endif

BEGIN_NAMESPACE( classad )

enum MmMode
{
	MEM_NONE	= 0,
	MEM_DISPOSE = 1,
	MEM_DUP		= 2
};

enum {
	EVAL_FAIL,
	EVAL_OK,
	EVAL_UNDEF,
	PROP_UNDEF,
	EVAL_ERROR,
	PROP_ERROR
};

/// The kinds of nodes in expression trees
enum NodeKind
{
	/** Literal node (string, integer, real, boolean, undefined, error) */
																LITERAL_NODE,
	/** Attribute reference node (attr, .attr, expr.attr) */    ATTRREF_NODE,
	/** Expression operation node (unary, binary, ternary) */   OP_NODE,
	/** Function call node */                                   FN_CALL_NODE,
	/** ClassAd node */                                         CLASSAD_NODE,
	/** Expression list node */                                 EXPR_LIST_NODE
};

/// The various kinds of values
enum ValueType
{
	/** The error value */              ERROR_VALUE,
	/** The undefined value */          UNDEFINED_VALUE,
	/** A boolean value (false, true)*/ BOOLEAN_VALUE,
	/** An integer value */             INTEGER_VALUE,
	/** A real value */                 REAL_VALUE,
	/** A relative time value */        RELATIVE_TIME_VALUE,
	/** An absolute time value */       ABSOLUTE_TIME_VALUE,
	/** A string value */               STRING_VALUE,
	/** A classad value */              CLASSAD_VALUE,
	/** An expression list value */     LIST_VALUE
};

/// The multiplicative factors for a numeric literal.
enum NumberFactor 
{ 
    /** No factor specified */ NO_FACTOR   = 1,
    /** Kilo factor */ 	K_FACTOR = 1024,
    /** Mega factor */ 	M_FACTOR = 1024*1024,
    /** Giga factor */ 	G_FACTOR = 1024*1024*1024
};

/// List of supported operators 
// The order in this enumeration should be synchronized with the opString[]
// array in operators.C
enum OpKind 
{
    /** No op */ __NO_OP__,              // convenience

    __FIRST_OP__,

    __COMPARISON_START__   	= __FIRST_OP__,
	/** @name Strict comparison operators */
	//@{
    /** Less than operator 	*/ LESS_THAN_OP            = __COMPARISON_START__,
    /** Less or equal 		*/	LESS_OR_EQUAL_OP,       // (comparison)
    /** Not equal 			*/ 	NOT_EQUAL_OP,           // (comparison)
    /** Equal 				*/	EQUAL_OP,               // (comparison)
    /** Greater or equal 	*/ 	GREATER_OR_EQUAL_OP,    // (comparison)
    /** Greater than 		*/	GREATER_THAN_OP,        // (comparison)
	//@}

	/** @name Non-strict comparison operators */
	//@{
    /** Meta-equal (same as IS)*/ META_EQUAL_OP,        // (comparison)
	/** Is 					*/ 	IS_OP= META_EQUAL_OP,	// (comparison)
    /** Meta-not-equal (same as ISNT) */META_NOT_EQUAL_OP,//(comparison)
	/** Isnt 				*/ 	ISNT_OP=META_NOT_EQUAL_OP,//(comparison)
	//@}
    __COMPARISON_END__     	= ISNT_OP,

    __ARITHMETIC_START__,     
	/** @name Arithmetic operators */
	//@{
    /** Unary plus 			*/	UNARY_PLUS_OP           = __ARITHMETIC_START__,
    /** Unary minus 		*/	UNARY_MINUS_OP,         // (arithmetic)
    /** Addition 			*/	ADDITION_OP,            // (arithmetic)
    /** Subtraction 		*/	SUBTRACTION_OP,         // (arithmetic)
    /** Multiplication 		*/	MULTIPLICATION_OP,      // (arithmetic)
    /** Division 			*/	DIVISION_OP,            // (arithmetic)
    /** Modulus 			*/	MODULUS_OP,             // (arithmetic)
	//@}
    __ARITHMETIC_END__      = MODULUS_OP,
    
    __LOGIC_START__,
	/** @name Logical operators */
	//@{
    /** Logical not 		*/	LOGICAL_NOT_OP          = __LOGIC_START__,
    /** Logical or 			*/	LOGICAL_OR_OP,          // (logical)
    /** Logical and 		*/	LOGICAL_AND_OP,         // (logical)
	//@}
    __LOGIC_END__           = LOGICAL_AND_OP,

    __BITWISE_START__,
	/** @name Bitwise operators */
	//@{
    /** Bitwise not 		*/	BITWISE_NOT_OP          = __BITWISE_START__,
    /** Bitwise or 			*/	BITWISE_OR_OP,          // (bitwise)
    /** Bitwise xor 		*/	BITWISE_XOR_OP,         // (bitwise)
    /** Bitwise and 		*/	BITWISE_AND_OP,         // (bitwise)
    /** Left shift 			*/	LEFT_SHIFT_OP,          // (bitwise)
    /** Right shift 		*/	RIGHT_SHIFT_OP, 		// (bitwise)
    /** Unsigned right shift */	URIGHT_SHIFT_OP,   		// (bitwise)
	//@}
    __BITWISE_END__         = URIGHT_SHIFT_OP, 

    __MISC_START__,
	/** @name Miscellaneous operators */
	//@{
    /** Parentheses 		*/	PARENTHESES_OP          = __MISC_START__,
    /** Subscript 			*/	SUBSCRIPT_OP,           // (misc)
    /** Conditional op 		*/	TERNARY_OP,             // (misc)
	//@}
    __MISC_END__            = TERNARY_OP,

    __LAST_OP__             = __MISC_END__
};


    // collection operations
enum {
    ClassAdCollOp_NoOp               	= 10000,

	__ClassAdCollOp_ViewOps_Begin__,
    ClassAdCollOp_CreateSubView      	= __ClassAdCollOp_ViewOps_Begin__,
    ClassAdCollOp_CreatePartition    	,
    ClassAdCollOp_DeleteView         	,
    ClassAdCollOp_SetViewInfo        	,
	ClassAdCollOp_AckViewOp				,
	__ClassAdCollOp_ViewOps_End__		= ClassAdCollOp_AckViewOp,

	__ClassAdCollOp_ClassAdOps_Begin__,
    ClassAdCollOp_AddClassAd         	= __ClassAdCollOp_ClassAdOps_Begin__,
    ClassAdCollOp_UpdateClassAd      	,
    ClassAdCollOp_ModifyClassAd      	,
    ClassAdCollOp_RemoveClassAd      	,
	ClassAdCollOp_AckClassAdOp			,
	__ClassAdCollOp_ClassAdOps_End__	= ClassAdCollOp_AckClassAdOp,

	__ClassAdCollOp_XactionOps_Begin__,
    ClassAdCollOp_OpenTransaction		= __ClassAdCollOp_XactionOps_Begin__,
	ClassAdCollOp_AckOpenTransaction	,
	ClassAdCollOp_CommitTransaction		,
	ClassAdCollOp_AbortTransaction		,
	ClassAdCollOp_AckCommitTransaction	,
    ClassAdCollOp_ForgetTransaction   	,
	__ClassAdCollOp_XactionOps_End__	= ClassAdCollOp_ForgetTransaction,

	__ClassAdCollOp_ReadOps_Begin__		,
	ClassAdCollOp_GetClassAd			= __ClassAdCollOp_ReadOps_Begin__,
	ClassAdCollOp_GetViewInfo			,
	ClassAdCollOp_GetSubordinateViewNames,
	ClassAdCollOp_GetPartitionedViewNames,
	ClassAdCollOp_FindPartitionName		,
	ClassAdCollOp_IsActiveTransaction	,
	ClassAdCollOp_IsCommittedTransaction,
	ClassAdCollOp_GetAllActiveTransactions,
	ClassAdCollOp_GetAllCommittedTransactions,
	ClassAdCollOp_GetServerTransactionState,
	ClassAdCollOp_AckReadOp				,
	__ClassAdCollOp_ReadOps_End__		= ClassAdCollOp_AckReadOp,

	__ClassAdCollOp_MiscOps_Begin__		,
	ClassAdCollOp_Connect				= __ClassAdCollOp_MiscOps_Begin__,
	ClassAdCollOp_QueryView				,
	ClassAdCollOp_Disconnect			,
	__ClassAdCollOp_MiscOps_End__		= ClassAdCollOp_Disconnect
};


	// these should be in the same order as the coll ops
static const char * const CollOpStrings[] = {
	"no op",

	"create sub view",
	"create partition",
	"delete view",
	"set view info",
	"ack view operation",

	"add classad",
	"update classad",
	"modify classad",
	"remove classad",
	"ack classad op",

	"open transaction",
	"ack open transaction",
	"commit transaction",
	"abort transaction",
	"ack commit transaction",
	"forget transaction",

	"get classad",
	"get view info",
	"get sub view names",
	"get partitioned view names",
	"find partition name",
	"is active transaction?",
	"is committed transaction?",
	"get all active transactions",
	"get all committed transactions",
	"get server transaction state",
	"ack read op",

	"connect to server",
	"query view",
	"disconnect from server"
};


static const char ATTR_AD					[]	= "Ad";
static const char ATTR_CONTEXT				[] 	= "Context";
static const char ATTR_DEEP_MODS			[] 	= "DeepMods";
static const char ATTR_DELETE_AD			[] 	= "DeleteAd";
static const char ATTR_DELETES				[] 	= "Deletes";
static const char ATTR_KEY					[]	= "Key";
static const char ATTR_NEW_AD				[]	= "NewAd";
static const char ATTR_OP_TYPE				[]	= "OpType";
static const char ATTR_PARENT_VIEW_NAME		[]	= "ParentViewName";
static const char ATTR_PARTITION_EXPRS 		[]  = "PartitionExprs";
static const char ATTR_PARTITIONED_VIEWS	[] 	= "PartitionedViews";
static const char ATTR_PROJECT_THROUGH		[]	= "ProjectThrough";
static const char ATTR_RANK					[]	= "Rank";
static const char ATTR_RANK_HINTS			[] 	= "RankHints";
static const char ATTR_REPLACE				[] 	= "Replace";
static const char ATTR_REQUIREMENTS			[]	= "Requirements";
static const char ATTR_SUBORDINATE_VIEWS	[]	= "SubordinateViews";
static const char ATTR_UPDATES				[] 	= "Updates";
static const char ATTR_WANT_LIST			[]	= "WantList";
static const char ATTR_WANT_PRELUDE			[]	= "WantPrelude";
static const char ATTR_WANT_RESULTS			[]	= "WantResults";
static const char ATTR_WANT_POSTLUDE		[]	= "WantPostlude";
static const char ATTR_VIEW_INFO			[]	= "ViewInfo";
static const char ATTR_VIEW_NAME			[]	= "ViewName";
static const char ATTR_XACTION_NAME			[]	= "XactionName";

enum AckMode { _DEFAULT_ACK_MODE, WANT_ACKS, DONT_WANT_ACKS };

#if defined(__cplusplus)
#include <string>
struct CaseIgnLTStr {
    bool operator( )( const string &s1, const string &s2 ) const {
       return( strcasecmp( s1.c_str( ), s2.c_str( ) ) < 0 );
	}
};

class ExprTree;
struct ExprHash {
	size_t operator()( const ExprTree *const &x ) const {
		return( (size_t)x );
	}
};

struct StringHash {
	size_t operator()( const string &s ) const {
		size_t h = 0;
		for( int i = s.size(); i >= 0; i-- ) {
			h = 5*h + s[i];
		}
		return( h );
	}
};
#endif


END_NAMESPACE // classad

char* strnewp( const char* );

extern int 		CondorErrno;

#ifdef __cplusplus
#include <string>
extern string 	CondorErrMsg;
static const string NULL_XACTION = "";
#endif

#include "classadErrno.h"

#endif//__COMMON_H__
