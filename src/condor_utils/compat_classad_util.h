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

#ifndef COMPAT_CLASSAD_UTIL_H
#define COMPAT_CLASSAD_UTIL_H

#include "compat_classad.h"

// parse str into attr=expression, returning the attr and the expression and true on success
bool ParseLongFormAttrValue(const char*str, std::string &attr, classad::ExprTree*& tree);

int ParseClassAdRvalExpr(const char*s, classad::ExprTree*&tree);

const char * ExprTreeToString( const classad::ExprTree *expr, std::string & buffer );
const char * ExprTreeToString( const classad::ExprTree *expr );
const char * ClassAdValueToString ( const classad::Value & value, std::string & buffer );
const char * ClassAdValueToString ( const classad::Value & value );

bool ExprTreeIsLiteral(classad::ExprTree * expr, classad::Value & value);
bool ExprTreeIsLiteralNumber(classad::ExprTree * expr, long long & ival);
bool ExprTreeIsLiteralNumber(classad::ExprTree * expr, double & rval);
bool ExprTreeIsLiteralString(classad::ExprTree * expr, std::string & sval);
bool ExprTreeIsLiteralString(classad::ExprTree * expr, const char* & cstr);
bool ExprTreeIsLiteralBool(classad::ExprTree * expr, bool & bval);
bool ExprTreeIsAttrRef(classad::ExprTree * expr, std::string & attr, bool * is_absolute=NULL);
bool ExprTreeMayDollarDollarExpand(classad::ExprTree *tree, std::string & unparsed);

// returns true when the expression is a comparision between an attribute ref and a literal
// on exit:
//  when return is true
//   cmp_op is the comparison operator
//   attr   is the name of the attribute
//   value  is the literal value
//  when return is false, attr and/or value *may* have been modified
bool ExprTreeIsAttrCmpLiteral(classad::ExprTree * tree, classad::Operation::OpKind & cmp_op, std::string & attr, classad::Value & value);

// Returns true when the expression selects a single job by cluster or proc,
// or a single cluster
// on exit:
//  cluster is clusterid of expression, or -1, 
//  proc is procid of expresion or -1
//  cluster_only is true if expression should match ONLY the cluster ad
bool ExprTreeIsJobIdConstraint(classad::ExprTree * expr, int & cluster, int & proc, bool & cluster_only);
// same as above, but a trailing clause that matches and all DAG children is allowed
// on exit:
//  same as above.
//  dagman_job_id is true if clusterid was set AND expression has (DagmanJobId == cluster)
bool ExprTreeIsJobIdConstraint(classad::ExprTree * expr, int & cluster, int & proc, bool & cluster_only, bool & dagman_job_id);

// check to see that a classad expression is valid
// if attrs is not NULL, it also adds attribute references from the expression into the current set.
bool IsValidClassAdExpression(const char * expr, classad::References * attrs=NULL, classad::References *scopes=NULL);

typedef std::map<std::string, std::string, classad::CaseIgnLTStr> NOCASE_STRING_MAP;
// edit the given expr changing attribute references as the mapping indicates
// for instance if mapping["TARGET"] = "My" it will change all instance of "TARGET" to "MY"
// if mapping["TARGET"] = "", it will remove target prefixes.
int RewriteAttrRefs(classad::ExprTree * expr, const NOCASE_STRING_MAP & mapping);

// add attribute references to attrs when they are of the given scope. for example when scope is "MY"
// and the expression contains MY.Foo, the Foo is added to attrs.
int GetAttrRefsOfScope(classad::ExprTree * expr, classad::References &attrs, const std::string &scope);

const classad::ExprTree * SkipExprEnvelope(const classad::ExprTree * tree);
inline classad::ExprTree * SkipExprEnvelope(classad::ExprTree * tree) {
	return const_cast<classad::ExprTree *>(SkipExprEnvelope(const_cast<const classad::ExprTree *>(tree)));
}
const classad::ExprTree * SkipExprParens(const classad::ExprTree * tree);
inline classad::ExprTree * SkipExprParens(classad::ExprTree * tree) {
	return const_cast<classad::ExprTree *>(SkipExprParens(const_cast<const classad::ExprTree *>(tree)));
}
// create an op node, using copies of the input expr trees. this function will not copy envelope nodes (it skips over them)
// and it is clever enough to insert parentheses around the input expressions when needed to insure that the expression
// will unparse correctly.
classad::ExprTree * JoinExprTreeCopiesWithOp(classad::Operation::OpKind, classad::ExprTree * exp1, classad::ExprTree * exp2);
// note: do NOT pass an envelope node to this function!! it's fine to pass the output of ParseClassAdRvalExpr
classad::ExprTree * WrapExprTreeInParensForOp(classad::ExprTree * expr, classad::Operation::OpKind op);

bool EvalExprBool(ClassAd *ad, classad::ExprTree *tree);

bool ClassAdsAreSame( ClassAd *ad1, ClassAd * ad2, StringList * ignored_attrs=NULL, bool verbose=false );

void CopyMachineResources(ClassAd &destAd, const ClassAd & srcAd, bool include_res_list);

void CopySelectAttrs(ClassAd &destAd, const ClassAd &srcAd, const std::string &attrs, bool overwrite=true);

// returns TRUE if the expression evaluates successfully and the result was a pod type
// or one of the complex types in the type mask.  If a mask of 0 matches all types.
// returns FALSE if the expression could not be evaluated or if the value was unsafe and not in the type mask
// note that ERROR can be a valid result of evaluation if the type_mask allows for it. a failure to
// evaluate is more fundamental than evaluation to error.
// NOTE: this function will NOT return false for successful evaluation to a type not in the mask
// it only returns false for successful evalatation when the type was unsafe and was therefore destroyed
// This is done so that callers can print useful diagnostics.
bool EvalExprTree( classad::ExprTree *expr, ClassAd *source,
				  ClassAd *target, classad::Value &result,
				  classad::Value::ValueType type_mask,
				  const std::string & sourceAlias = "",
				  const std::string & targetAlias = "" );

// useful for evaluating to a number or to a bool
inline int EvalExprToNumber( classad::ExprTree *expr, ClassAd *source,
	ClassAd *target, classad::Value &result,
	const std::string & sourceAlias = "",
	const std::string & targetAlias = "") {
	return EvalExprTree(expr, source, target, result, classad::Value::ValueType::NUMBER_VALUES, sourceAlias, targetAlias);
}

inline int EvalExprToBool( classad::ExprTree *expr, ClassAd *source,
	ClassAd *target, classad::Value &result,
	const std::string & sourceAlias = "",
	const std::string & targetAlias = "") {
	return EvalExprTree(expr, source, target, result, classad::Value::ValueType::NUMBER_VALUES, sourceAlias, targetAlias);
}

// use this when only a string value is useful
inline int EvalExprToString( classad::ExprTree *expr, ClassAd *source,
	ClassAd *target, classad::Value &result,
	const std::string & sourceAlias = "",
	const std::string & targetAlias = "") {
	return EvalExprTree(expr, source, target, result, classad::Value::ValueType::STRING_VALUE, sourceAlias, targetAlias);
}

// returns any single-valued literal including undefined and error, but not a classad or a list
inline int EvalExprToScalar( classad::ExprTree *expr, ClassAd *source,
	ClassAd *target, classad::Value &result,
	const std::string & sourceAlias = "",
	const std::string & targetAlias = "") {
	return EvalExprTree(expr, source, target, result, classad::Value::ValueType::SCALAR_EX_VALUES, sourceAlias, targetAlias);
}

//ad2 treated as candidate to match against ad1, so we want to find a match for ad1
// This does reciprocal Requirements matching, but as of 23.0 (and 10.0.9) it no longer checks TargetType
bool IsAMatch( ClassAd *ad1, ClassAd *ad2 );

// evaluate My.Requirements in the context of a target ad
// also check that the MyType of the target ad matches the given targetType.  The collector uses this
bool IsATargetMatch( ClassAd *my, ClassAd *target, const char * targetType );

// evaluates the query REQUIREMENTS against the target ad
// but does *NOT* care about TargetType
bool IsAConstraintMatch( ClassAd *query, ClassAd *target );

bool ParallelIsAMatch(ClassAd *ad1, std::vector<ClassAd*> &candidates, std::vector<ClassAd*> &matches, int threads, bool halfMatch = false);

void AddClassAdXMLFileHeader(std::string &buffer);
void AddClassAdXMLFileFooter(std::string &buffer);

void clear_user_maps(std::vector<std::string> * keep_list);
int add_user_map(const char * mapname, const char * filename, MapFile * mf /*=NULL*/);
int add_user_mapping(const char * mapname, const char * mapdata);
// these functions are in classad_usermap.cpp (and also libcondorapi_stubs.cpp)
int reconfig_user_maps();
bool user_map_do_mapping(const char * mapname, const char * input, std::string & output);

// a class to hold (and delete) a constraint ExprTree
// it can be initialized with either a string for a tree
// and produce both string and tree on demand.
class ConstraintHolder {
public:
	ConstraintHolder() : expr(NULL), exprstr(NULL) {}
	ConstraintHolder(char * str) : expr(NULL), exprstr(str) {}
	ConstraintHolder(classad::ExprTree * tree) : expr(tree), exprstr(NULL) {}
	ConstraintHolder(ConstraintHolder&& that) : expr(that.expr), exprstr(that.exprstr) { // move constructor
		that.expr = nullptr;
		that.exprstr = nullptr;
	}
	ConstraintHolder(const ConstraintHolder& that) : expr(NULL), exprstr(NULL) {  // for copy constructor, we prefer the tree form
		if (this == &that) return;
		if (that.expr) { this->set(that.expr->Copy()); }
		else if (that.exprstr) { set(strdup(that.exprstr)); }
	}
	ConstraintHolder & operator=(const ConstraintHolder& that) { // for assignment we deep copy
		if (this != &that) {
			clear();
			if (that.expr) this->expr = that.expr->Copy();
			if (that.exprstr) this->exprstr = strdup(that.exprstr);
		}
		return *this;
	}
	ConstraintHolder & operator=(ConstraintHolder&& that) { // move assignment operator
		if (this != &that) {
			clear();
			expr = that.expr; that.expr = nullptr;
			exprstr = that.exprstr; that.exprstr = nullptr;
		}
		return *this;
	}
	bool operator==(const ConstraintHolder & rhs) const { // for equality, we compare expressions (not strings)
		classad::ExprTree *tree1 = Expr(), *tree2 = rhs.Expr();
		if ( ! tree1 && ! tree2) return true;
		if ( ! tree1 || ! tree2) return false;
		return *tree1 == *tree2;
	}
	~ConstraintHolder() { clear(); }
	void clear() { delete expr; expr = NULL; if (exprstr) { free(exprstr); exprstr = NULL; } }
	// return true if there is no expression
	bool empty() const { return ! expr && ( ! exprstr || ! exprstr[0]); }
	// get a printable representation, will never return null
	const char * c_str() const { const char * p = Str(); return p ? p : ""; }
	// detach the ExprTree pointer and return it, then clear
	classad::ExprTree * detach() { classad::ExprTree * t = Expr(); expr = NULL; clear(); return t; }
	// change the expr tree, freeing the old pointer if needed.
	void set(classad::ExprTree * tree) { if (tree && (tree != expr)) { clear(); expr = tree; } }
	// change the expression using a string, freeing the old pointer if needed.
	void set(char * str) { if (str && (exprstr != str)) { clear(); exprstr = str; } }
	// get the constraint, parsing the string if needed to construct it.
	// returns NULL if no constraint
	bool parse(const char * str) { clear(); return ParseClassAdRvalExpr(str, expr); }
	classad::ExprTree * Expr(int * error=NULL) const {
		int rval = 0;
		if ( ! expr && ! empty()) {
			if (ParseClassAdRvalExpr(exprstr, expr)) {
				rval = -1;
			}
		}
		if (error) { *error = rval; };
		return expr;
	}
	// get the constraint as as string, unparsing the constraint expression if needed
	// returns NULL if no constraint
	const char * Str() const {
		if (( ! exprstr || ! exprstr[0]) && expr) { exprstr = strdup(ExprTreeToString(expr)); }
		return exprstr;
	}
protected:
	// hold the constraint in either string or tree form. If both forms exist the tree is canonical
	// mutable because we might construct one from the other in a const method.
	mutable classad::ExprTree * expr;
	mutable char * exprstr;
};

typedef CondorClassAdFileParseHelper ClassAdFileParseType;
ClassAdFileParseType::ParseType parseAdsFileFormat(const char * arg, ClassAdFileParseType::ParseType def_parse_type);

#endif
