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
#include "compat_classad_util.h"
#include "classad_oldnew.h"
#include "condor_adtypes.h"
#include "condor_attributes.h"
#include "classad/classadCache.h" // for CachedExprEnvelope

#include "compat_classad_list.h"
#ifdef _OPENMP
#include <omp.h>
#endif

 // returns the length of the string including quotes 
 // if str starts and ends with doublequotes
 // and contains no internal \ or doublequotes.
static size_t IsSimpleString( const char *str )
{
	if ( *str != '"') return false;

	++str;
	size_t n = strcspn(str,"\\\"");
	if  (str[n] == '\\') {
		return 0;
	}
	if (str[n] == '"') { // found a close quote - good so far.
		// trailing whitespace is permitted (but leading whitespace is not)
		// return 0 if anything but whitespace follows
		// return length of quoted string (including quotes) if just whitespace.
		str += n+1;
		for (;;) {
			char ch = *str++;
			if ( ! ch) return n+2;
			if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') return 0;
		}
	}

	return 0;
}

static long long myatoll(const char *s, const char* &end) {
	long long result = 0;
	int negative = 0;
	if (*s == '-') {
		negative = 1;
		s++;
	}

	while ((*s >= '0') && (*s <= '9')) {
		result = (result * 10) - (*s - '0');
		s++;
	}
	end = s;
	if (!negative) {
		result = -result;
	}
	return result;
}

// fast pre-parsing of null terminated strings when they happen to be simple literals
classad::Literal * fastParseSomeClassadLiterals(const char * rhs, size_t cbrhs, const size_t max_fastparse_string)
{
	char ch = rhs[0];
	if (cbrhs == 5 && (ch&~0x20) == 'T' && (rhs[1]&~0x20) == 'R' && (rhs[2]&~0x20) == 'U' && (rhs[3]&~0x20) == 'E') {
		return classad::Literal::MakeBool(true);
	} else if (cbrhs == 6 && (ch&~0x20) == 'F' && (rhs[1]&~0x20) == 'A' && (rhs[2]&~0x20) == 'L' && (rhs[3]&~0x20) == 'S' && (rhs[4]&~0x20) == 'E') {
		return classad::Literal::MakeBool(false);
	} else if (cbrhs < 30 && (ch == '-' || (ch >= '0' && ch <= '9'))) {
		if (strchr(rhs, '.')) {
			char *pe = NULL;
			double d = strtod(rhs, &pe);
			if (*pe == 0 || *pe == '\r' || *pe == '\n') {
				return classad::Literal::MakeReal(d);
			}
		} else {
			const char * pe = NULL;
			long long ll = myatoll(rhs, pe);
			if (*pe == 0 || *pe == '\r' || *pe == '\n') {
				return classad::Literal::MakeInteger(ll);
			}
		}
	} else if (cbrhs < max_fastparse_string && ch == '"' && rhs[cbrhs] == 0) { // 128 because we want long strings in the cache.
		size_t cch = IsSimpleString(rhs);
		if (cch) {
			return classad::Literal::MakeString(rhs+1, cch-2);
		}
	}
	return nullptr;
}


/* TODO This function needs to be tested.
 */
int ParseClassAdRvalExpr(const char*s, classad::ExprTree*&tree)
{
	classad::ClassAdParser parser;
	parser.SetOldClassAd( true );
	tree = parser.ParseExpression(s, true);
	if ( tree ) {
		return 0;
	} else {
		return 1;
	}
}

bool ParseLongFormAttrValue(const char * str, std::string & attr, classad::ExprTree*&tree)
{
	const char * rhs = NULL;
	if ( ! SplitLongFormAttrValue(str, attr, rhs)) {
		return false;
	}
	return ParseClassAdRvalExpr(rhs, tree) == 0;
}

/*
 */
const char *ExprTreeToString( const classad::ExprTree *expr, std::string & buffer )
{
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );
	unparser.Unparse( buffer, expr );
	return buffer.c_str();
}

const char *ExprTreeToString( const classad::ExprTree *expr )
{
	static std::string buffer;
	buffer = "";
	return ExprTreeToString(expr, buffer);
}

const char * ClassAdValueToString ( const classad::Value & value, std::string & buffer )
{
	classad::ClassAdUnParser unparser;

	unparser.SetOldClassAd( true, true );
	unparser.Unparse( buffer, value );

	return buffer.c_str();
}

const char * ClassAdValueToString ( const classad::Value & value )
{
	static std::string buffer;
	buffer = "";
	return ClassAdValueToString(value, buffer);
}

const classad::ExprTree * SkipExprEnvelope(const classad::ExprTree * tree) {
	if ( ! tree) return tree;
	classad::ExprTree::NodeKind kind = tree->GetKind();
	if (kind == classad::ExprTree::EXPR_ENVELOPE) {
		return (dynamic_cast<const classad::CachedExprEnvelope *>(tree))->get();
	}
	return tree;
}

const classad::ExprTree * SkipExprParens(const classad::ExprTree * tree) {
	if ( ! tree) return tree;
	classad::ExprTree::NodeKind kind = tree->GetKind();
	classad::ExprTree * expr = const_cast<classad::ExprTree*>(tree);
	if (kind == classad::ExprTree::EXPR_ENVELOPE) {
		expr = (dynamic_cast<const classad::CachedExprEnvelope*>(tree))->get();
		if (expr) tree = expr;
	}

	kind = tree->GetKind();
	while (kind == classad::ExprTree::OP_NODE) {
		classad::ExprTree *e2, *e3;
		classad::Operation::OpKind op;
		(dynamic_cast<const classad::Operation*>(tree))->GetComponents(op, expr, e2, e3);
		if ( ! expr || op != classad::Operation::PARENTHESES_OP) break;
		tree = expr;
		kind = tree->GetKind();
	}

	return tree;
}

// wrap an expr tree with a new PARENTHESES_OP if it will be needed to preserve operator precedence
// when used as the left or right hand side of the given op.
// 
classad::ExprTree * WrapExprTreeInParensForOp(classad::ExprTree * expr, classad::Operation::OpKind op)
{
	if ( ! expr) return expr;

	classad::ExprTree::NodeKind kind = expr->GetKind();
	if (kind == classad::ExprTree::OP_NODE) {
		classad::Operation::OpKind op1 = ((classad::Operation*)expr)->GetOpKind();
		if (op1 == classad::Operation::PARENTHESES_OP) {
			// no need to insert parends, this already is one.
		} else if (classad::Operation::PrecedenceLevel(op1) < classad::Operation::PrecedenceLevel(op)) {
			expr = classad::Operation::MakeOperation(classad::Operation::PARENTHESES_OP, expr, NULL, NULL);
		}
	}
	return expr;
}

classad::ExprTree * JoinExprTreeCopiesWithOp(classad::Operation::OpKind op, classad::ExprTree * exp1, classad::ExprTree * exp2)
{
	// before we join these into a new tree, we want to skip over the envelope nodes (if any) and copy them.
	if (exp1) {
		exp1 = SkipExprEnvelope(exp1)->Copy();
		exp1 = WrapExprTreeInParensForOp(exp1, op);
	}
	if (exp2) {
		exp2 = SkipExprEnvelope(exp2)->Copy();
		exp2 = WrapExprTreeInParensForOp(exp2, op);
	}
	
	return classad::Operation::MakeOperation(op, exp1, exp2, NULL);
}

bool ExprTreeIsLiteral(classad::ExprTree * expr, classad::Value & value)
{
	if ( ! expr) return false;

	classad::ExprTree::NodeKind kind = expr->GetKind();
	if (kind == classad::ExprTree::EXPR_ENVELOPE) {
		expr = ((classad::CachedExprEnvelope*)expr)->get();
		if ( ! expr) return false;
		kind = expr->GetKind();
	}

	// dive into parens
	while (kind == classad::ExprTree::OP_NODE) {
		classad::ExprTree *e2, *e3;
		classad::Operation::OpKind op;
		((classad::Operation*)expr)->GetComponents(op, expr, e2, e3);
		if ( ! expr || op != classad::Operation::PARENTHESES_OP) return false;

		kind = expr->GetKind();
	}

	if (dynamic_cast<classad::Literal *>(expr) != nullptr) {
		((classad::Literal*)expr)->GetValue(value);
		return true;
	}

	return false;
}

bool ExprTreeIsLiteralString(classad::ExprTree * expr, const char * & cstr)
{
	if ( ! expr) return false;

	classad::ExprTree::NodeKind kind = expr->GetKind();
	if (kind == classad::ExprTree::EXPR_ENVELOPE) {
		expr = ((classad::CachedExprEnvelope*)expr)->get();
		if ( ! expr) return false;
		kind = expr->GetKind();
	}

	// dive into parens
	while (kind == classad::ExprTree::OP_NODE) {
		classad::ExprTree *e2, *e3;
		classad::Operation::OpKind op;
		((classad::Operation*)expr)->GetComponents(op, expr, e2, e3);
		if ( ! expr || op != classad::Operation::PARENTHESES_OP) return false;

		kind = expr->GetKind();
	}

	if (dynamic_cast<classad::StringLiteral*>(expr) != nullptr) {
		const char *s = ((classad::StringLiteral*)expr)->getCString();
		cstr = s;
		return true;
	}

	return false;
}

bool ExprTreeIsLiteralNumber(classad::ExprTree * expr, long long & ival)
{
	classad::Value val;
	if ( ! ExprTreeIsLiteral(expr, val)) return false;
	return val.IsNumber(ival);
}

bool ExprTreeIsLiteralNumber(classad::ExprTree * expr, double & rval)
{
	classad::Value val;
	if ( ! ExprTreeIsLiteral(expr, val)) return false;
	return val.IsNumber(rval);
}

bool ExprTreeIsLiteralBool(classad::ExprTree * expr, bool & bval)
{
	classad::Value val;
	if ( ! ExprTreeIsLiteral(expr, val)) return false;
	long long ival;
	if ( !  val.IsNumber(ival)) return false;
	bval = ival != 0;
	return true;
}

bool ExprTreeIsLiteralString(classad::ExprTree * expr, std::string & sval)
{
	classad::Value val;
	if ( ! ExprTreeIsLiteral(expr, val)) return false;
	return val.IsStringValue(sval);
}

bool ExprTreeIsAttrRef(classad::ExprTree * expr, std::string & attr, bool * is_absolute /*=NULL*/)
{
	if ( ! expr) return false;

	classad::ExprTree::NodeKind kind = expr->GetKind();
	if (kind == classad::ExprTree::ATTRREF_NODE) {
		classad::ExprTree *e2=NULL;
		bool absolute=false;
		((classad::AttributeReference*)expr)->GetComponents(e2, attr, absolute);
		if (is_absolute) *is_absolute = absolute;
		return !e2;
	}
	return false;
}

// returns true and appends the unparsed value of the given attribute
// IFF the value might have $$() expansions in it
// returns false if $$() is impossible (because int, etc).
bool ExprTreeMayDollarDollarExpand(classad::ExprTree * tree,  std::string & unparse_buf)
{
	tree = SkipExprEnvelope(tree);
	if ( ! tree) return false;

	if (dynamic_cast<classad::StringLiteral *>(tree) != nullptr) {
		// simple string literals can be quickly checked for possible $$ expansion
		classad::StringLiteral *lit = (classad::StringLiteral *) tree;
		const char * cstr  = lit->getCString();
		if (!strchr(cstr, '$')) {
			return false;
		}
	}

	// append unparsed value into the buffer
	return ExprTreeToString(tree, unparse_buf) != nullptr;
}

// return true if the expression is a comparision between an Attribute and a literal
// returns attr name, the comparison operation, and the literal value if it returns true.
bool ExprTreeIsAttrCmpLiteral(classad::ExprTree * tree, classad::Operation::OpKind & cmp_op, std::string & attr, classad::Value & value)
{
	if (! tree) return false;
	tree = SkipExprParens(tree);
	classad::ExprTree::NodeKind kind = tree->GetKind();
	if (kind != classad::ExprTree::OP_NODE)
		return false;

	classad::Operation::OpKind	op;
	classad::ExprTree *t1, *t2, *t3;
	((const classad::Operation*)tree)->GetComponents(op, t1, t2, t3);
	if (op < classad::Operation::__COMPARISON_START__ || op > classad::Operation::__COMPARISON_END__)
		return false;

	t1 = SkipExprParens(t1);
	t2 = SkipExprParens(t2);

	if (ExprTreeIsAttrRef(t1, attr) && ExprTreeIsLiteral(t2, value)) {
		cmp_op = op;
		return true;
	} else if (ExprTreeIsLiteral(t1, value) && ExprTreeIsAttrRef(t2, attr)) {
		cmp_op = op;
		return true;
	}
	return false;
}

// returns true if the expression is one of these forms
//   ClusterId == <number>
//   ClusterId == <number> && ProcId == <number>
//   ClusterId == <number> && ProcId is undefined
//
// parens are ignored, as is the order of the arguments
// so (<number> == ClusterId) will return true
bool ExprTreeIsJobIdConstraint(classad::ExprTree * tree, int & cluster, int & proc, bool & cluster_only)
{
	cluster = proc = -1;
	cluster_only = false;
	if (! tree) return false;

	std::string attr1, attr2;
	classad::Value value1, value2;

	tree = SkipExprParens(tree);
	classad::ExprTree::NodeKind kind = tree->GetKind();
	if (kind == classad::ExprTree::OP_NODE) {
		classad::Operation::OpKind	op;
		classad::ExprTree *t1, *t2, *t3;
		((const classad::Operation*)tree)->GetComponents(op, t1, t2, t3);
		if (op == classad::Operation::LOGICAL_AND_OP) {
			if (ExprTreeIsAttrCmpLiteral(t1, op, attr1, value1) &&
				ExprTreeIsAttrCmpLiteral(t2, op, attr2, value2)) {

				classad::Value * procval = NULL;
				// check for ClusterId == N && ProcId == N
				if (MATCH == strcasecmp(attr1.c_str(), "ClusterId") &&
					value1.IsNumber(cluster) &&
					MATCH == strcasecmp(attr2.c_str(), "ProcId")) {
					procval = &value2;
				} else if (MATCH == strcasecmp(attr1.c_str(), "ProcId") && 
					MATCH == strcasecmp(attr2.c_str(), "ClusterId") &&
					value2.IsNumber(cluster)) {
					procval = &value1;
				}
				// check for proc to be compared to a number or to undefined
				if (procval) {
					if (procval->IsUndefinedValue()) {
						cluster_only = true;
						proc = -1;
						return true;
					} else if (procval->IsNumber(proc)) {
						return true;
					}
				}
			}
		} else {
			// it was an op node, but not a logical &&, so it might be ==, 
			if (ExprTreeIsAttrCmpLiteral(tree, op, attr1, value1)) {
				if ((op == classad::Operation::EQUAL_OP || op == classad::Operation::META_EQUAL_OP) &&
					MATCH == strcasecmp(attr1.c_str(), "ClusterId") &&
					value1.IsNumber(cluster)) {
					proc = -1;
					return true;
				}
			}
		}
	}

	return false;
}

// Returns true if expression is
//   ClusterId == <number>
//   ClusterId == <number> && ProcId == <number>
//   ClusterId == <number> && ProcId is undefined
//   <any of the above 3> || DagmanJobId == <number>
//
bool ExprTreeIsJobIdConstraint(classad::ExprTree * tree, int & cluster, int & proc, bool & cluster_only, bool & dagman_job_id)
{
	cluster = proc = -1;
	dagman_job_id = cluster_only = false;
	if ( ! tree) return false;

	int dagid = -1;
	std::string attr;
	classad::Value value;

	tree = SkipExprParens(tree);
	classad::ExprTree::NodeKind kind = tree->GetKind();
	if (kind == classad::ExprTree::OP_NODE) {
		classad::Operation::OpKind	op;
		classad::ExprTree *t1, *t2, *t3;
		((const classad::Operation*)tree)->GetComponents(op, t1, t2, t3);
		if (op == classad::Operation::LOGICAL_OR_OP) {
			if (ExprTreeIsAttrCmpLiteral(t2, op, attr, value) &&
				MATCH == strcasecmp(attr.c_str(), "DAGManJobId") &&
				value.IsNumber(dagid)) {
				dagman_job_id = true;
			}
			if ( ! dagman_job_id)
				return false;
			// no compare the right hand side of the || to see if it is a job id constraint
			tree = t1;
		}
	}

	if (ExprTreeIsJobIdConstraint(tree, cluster, proc, cluster_only)) {
		if (dagman_job_id && dagid != cluster) {
			return false;
		}
		return true;
	}
	return false;
}

static int GetAttrsAndScopes(classad::ExprTree * expr, classad::References * attrs, classad::References *scopes);

// check to see that a classad expression is valid, and optionally return the names of the attributes that it references
bool IsValidClassAdExpression(const char * str, classad::References * attrs /*=NULL*/, classad::References *scopes /*=NULL*/)
{
	if ( ! str || ! str[0]) return false;

	classad::ExprTree * expr = NULL;
	int rval = ParseClassAdRvalExpr(str, expr);
	if (0 == rval) {
		if (attrs) {
			GetAttrsAndScopes(expr, attrs, scopes);
		}
		delete expr;
	}
	return rval == 0;
}

// walk an ExprTree, calling a function each time a ATTRREF_NODE is found.
//
int walk_attr_refs (
	const classad::ExprTree * tree,
	int (*pfn)(void *pv, const std::string & attr, const std::string &scope, bool absolute),
	void *pv)
{
	int iret = 0;
	if ( ! tree) return 0;
	switch (tree->GetKind()) {
		case ExprTree::ERROR_LITERAL:
		case ExprTree::UNDEFINED_LITERAL:
		case ExprTree::BOOLEAN_LITERAL:
		case ExprTree::INTEGER_LITERAL:
		case ExprTree::REAL_LITERAL:
		case ExprTree::RELTIME_LITERAL:
		case ExprTree::ABSTIME_LITERAL:
		case ExprTree::STRING_LITERAL: 
			break;
		case classad::ExprTree::ATTRREF_NODE: {
			const classad::AttributeReference* atref = reinterpret_cast<const classad::AttributeReference*>(tree);
			classad::ExprTree *expr;
			std::string ref;
			std::string tmp;
			bool absolute;
			atref->GetComponents(expr, ref, absolute);
			// if there is a non-trivial left hand side (something other than X from X.Y attrib ref)
			// then recurse it.
			if (expr && ! ExprTreeIsAttrRef(expr, tmp)) {
				iret += walk_attr_refs(expr, pfn, pv);
			} else {
				iret += pfn(pv, ref, tmp, absolute);
			}
		}
		break;

		case classad::ExprTree::OP_NODE: {
			classad::Operation::OpKind	op;
			classad::ExprTree *t1, *t2, *t3;
			((const classad::Operation*)tree)->GetComponents( op, t1, t2, t3 );
			if (t1) iret += walk_attr_refs(t1, pfn, pv);
			//if (iret && stop_on_first_match) return iret;
			if (t2) iret += walk_attr_refs(t2, pfn, pv);
			//if (iret && stop_on_first_match) return iret;
			if (t3) iret += walk_attr_refs(t3, pfn, pv);
		}
		break;

		case classad::ExprTree::FN_CALL_NODE: {
			std::string fnName;
			std::vector<classad::ExprTree*> args;
			((const classad::FunctionCall*)tree)->GetComponents( fnName, args );
			for (std::vector<classad::ExprTree*>::iterator it = args.begin(); it != args.end(); ++it) {
				iret += walk_attr_refs(*it, pfn, pv);
				//if (iret && stop_on_first_match) return iret;
			}
		}
		break;

		case classad::ExprTree::CLASSAD_NODE: {
			std::vector< std::pair<std::string, classad::ExprTree*> > attrs;
			((const classad::ClassAd*)tree)->GetComponents(attrs);
			for (std::vector< std::pair<std::string, classad::ExprTree*> >::iterator it = attrs.begin(); it != attrs.end(); ++it) {
				iret += walk_attr_refs(it->second, pfn, pv);
				//if (iret && stop_on_first_match) return iret;
			}
		}
		break;

		case classad::ExprTree::EXPR_LIST_NODE: {
			std::vector<classad::ExprTree*> exprs;
			((const classad::ExprList*)tree)->GetComponents( exprs );
			for (std::vector<classad::ExprTree*>::iterator it = exprs.begin(); it != exprs.end(); ++it) {
				iret += walk_attr_refs(*it, pfn, pv);
				//if (iret && stop_on_first_match) return iret;
			}
		}
		break;

		case classad::ExprTree::EXPR_ENVELOPE: {
			classad::ExprTree * expr = SkipExprEnvelope(const_cast<classad::ExprTree*>(tree));
			if (expr) iret += walk_attr_refs(expr, pfn, pv);
		}
		break;
	}
	return iret;
}

class AttrsAndScopes {
public:
	AttrsAndScopes() : attrs(NULL), scopes(NULL) {}
	classad::References *attrs;
	classad::References *scopes;
};
int AccumAttrsAndScopes(void *pv, const std::string & attr, const std::string &scope, bool /*absolute*/)
{
	AttrsAndScopes & p = *(AttrsAndScopes *)pv;
	if ( ! attr.empty()) p.attrs->insert(attr);
	if ( ! scope.empty()) p.scopes->insert(scope);
	return 1;
}

static int GetAttrsAndScopes(classad::ExprTree * expr, classad::References * attrs, classad::References *scopes)
{
	 AttrsAndScopes tmp;
	 tmp.attrs = attrs;
	 tmp.scopes = scopes ? scopes : attrs;
	 return walk_attr_refs(expr, AccumAttrsAndScopes, &tmp);
}

int AccumAttrsOfScopes(void *pv, const std::string & attr, const std::string &scope, bool /*absolute*/)
{
	AttrsAndScopes & p = *(AttrsAndScopes *)pv;
	if (p.scopes->find(scope) != p.scopes->end()) {
		p.attrs->insert(attr);
	}
	return 1;
}

// add attribute references to attrs when they are of the given scope. for example when scope is "MY"
// and the expression contains MY.Foo, the Foo is added to attrs.
int GetAttrRefsOfScope(classad::ExprTree * expr, classad::References &attrs, const std::string &scope)
{
	 classad::References scopes;
	 scopes.insert(scope);
	 AttrsAndScopes tmp;
	 tmp.attrs = &attrs;
	 tmp.scopes = &scopes;
	 return walk_attr_refs(expr, AccumAttrsOfScopes, &tmp);
}


// edit the given expr changing attribute references as the mapping indicates
int RewriteAttrRefs(classad::ExprTree * tree, const NOCASE_STRING_MAP & mapping)
{
	int iret = 0;
	if ( ! tree) return 0;
	switch (tree->GetKind()) {
		case ExprTree::ERROR_LITERAL:
		case ExprTree::UNDEFINED_LITERAL:
		case ExprTree::BOOLEAN_LITERAL:
		case ExprTree::INTEGER_LITERAL:
		case ExprTree::REAL_LITERAL:
		case ExprTree::RELTIME_LITERAL:
		case ExprTree::ABSTIME_LITERAL:
		case ExprTree::STRING_LITERAL: 
			break;
		case classad::ExprTree::ATTRREF_NODE: {
			classad::AttributeReference* atref = reinterpret_cast<classad::AttributeReference*>(tree);
			classad::ExprTree *expr;
			std::string ref;
			std::string tmp;
			bool absolute;
			atref->GetComponents(expr, ref, absolute);
			// if there is a non-trivial left hand side (something other than X from X.Y attrib ref)
			// then recurse it.
			if (expr && ! ExprTreeIsAttrRef(expr, tmp)) {
				iret += RewriteAttrRefs(expr, mapping);
			} else {
				bool change_it = false;
				if (expr) {
					NOCASE_STRING_MAP::const_iterator found = mapping.find(tmp);
					if (found != mapping.end()) {
						if (found->second.empty()) {
							expr = NULL; // the left hand side is a simple attr-ref. and we want to set it to EMPTY
							change_it = true;
						} else {
							iret += RewriteAttrRefs(expr, mapping);
						}
					}
				} else {
					NOCASE_STRING_MAP::const_iterator found = mapping.find(ref);
					if (found != mapping.end() && ! found->second.empty()) {
						ref = found->second;
						change_it = true;
					}
				}
				if (change_it) {
					atref->SetComponents(NULL, ref, absolute);
					iret += 1;
				}
			}
		}
		break;

		case classad::ExprTree::OP_NODE: {
			classad::Operation::OpKind	op;
			classad::ExprTree *t1, *t2, *t3;
			((classad::Operation*)tree)->GetComponents( op, t1, t2, t3 );
			if (t1) iret += RewriteAttrRefs(t1, mapping);
			if (t2) iret += RewriteAttrRefs(t2, mapping);
			if (t3) iret += RewriteAttrRefs(t3, mapping);
		}
		break;

		case classad::ExprTree::FN_CALL_NODE: {
			std::string fnName;
			std::vector<classad::ExprTree*> args;
			((classad::FunctionCall*)tree)->GetComponents( fnName, args );
			for (std::vector<classad::ExprTree*>::iterator it = args.begin(); it != args.end(); ++it) {
				iret += RewriteAttrRefs(*it, mapping);
			}
		}
		break;

		case classad::ExprTree::CLASSAD_NODE: {
			std::vector< std::pair<std::string, classad::ExprTree*> > attrs;
			((classad::ClassAd*)tree)->GetComponents(attrs);
			for (std::vector< std::pair<std::string, classad::ExprTree*> >::iterator it = attrs.begin(); it != attrs.end(); ++it) {
				iret += RewriteAttrRefs(it->second, mapping);
			}
		}
		break;

		case classad::ExprTree::EXPR_LIST_NODE: {
			std::vector<classad::ExprTree*> exprs;
			((classad::ExprList*)tree)->GetComponents( exprs );
			for (std::vector<classad::ExprTree*>::iterator it = exprs.begin(); it != exprs.end(); ++it) {
				iret += RewriteAttrRefs(*it, mapping);
			}
		}
		break;

		case classad::ExprTree::EXPR_ENVELOPE:
		default:
			// unknown or unallowed node.
			ASSERT(0);
		break;
	}
	return iret;
}



bool EvalExprBool(ClassAd *ad, classad::ExprTree *tree)
{
	classad::Value result;
	bool boolVal;

	// Evaluate constraint with ad in the target scope so that constraints
	// have the same semantics as the collector queries.  --RR
	if ( !EvalExprToBool( tree, ad, NULL, result ) ) {
		return false;
	}

	if( result.IsBooleanValueEquiv( boolVal ) ) {
		return boolVal;
	}

	return false;
}

// TODO ClassAd::SameAs() does a better job, but lacks an ignore list.
//   This function will return true if ad1 has attributes that ad2 lacks.
//   Both functions ignore any chained parent ad.
bool ClassAdsAreSame( ClassAd *ad1, ClassAd * ad2, classad::References *ignored_attrs, bool verbose )
{
	classad::ExprTree *ad1_expr, *ad2_expr;
	const char* attr_name;
	bool found_diff = false;
	for ( auto itr = ad2->begin(); itr != ad2->end(); itr++ ) {
		attr_name = itr->first.c_str();
		ad2_expr = itr->second;
		if( ignored_attrs && ignored_attrs->count(attr_name) > 0 ) {
			if( verbose ) {
				dprintf( D_FULLDEBUG, "ClassAdsAreSame(): skipping \"%s\"\n",
						 attr_name );
			}
			continue;
		}
		ad1_expr = ad1->LookupExpr( attr_name );
		if( ! ad1_expr ) {
				// no value for this in ad1, the ad2 value is
				// certainly different
			if( verbose ) {
				dprintf( D_FULLDEBUG, "ClassAdsAreSame(): "
						 "ad2 contains %s and ad1 does not\n", attr_name );
			}
			found_diff = true;
			break;
		}
		if( ad1_expr->SameAs( ad2_expr ) ) {
			if( verbose ) {
				dprintf( D_FULLDEBUG, "ClassAdsAreSame(): value of %s in "
						 "ad1 matches value in ad2\n", attr_name );
			}
		} else {
			if( verbose ) {
				dprintf( D_FULLDEBUG, "ClassAdsAreSame(): value of %s in "
						 "ad1 is different than in ad2\n", attr_name );
			}
			found_diff = true;
			break;
		}
	}
	return ! found_diff;
}

// copy attributes listed as "MachineResources" from the src to the destination ad
// This will also copy Available<res> attributes and any attributes referenced therein
void CopyMachineResources(ClassAd &destAd, const ClassAd & srcAd, bool include_res_list)
{
	std::string resnames;
	std::string attr;
	if(srcAd.LookupString( ATTR_MACHINE_RESOURCES, resnames)) {
		if (include_res_list) { destAd.Assign(ATTR_MACHINE_RESOURCES, resnames); }
	} else {
		resnames = "CPUs, Disk, Memory";
	}

	// copy the primary quantities of each of the machine resources into the starter ad
	// for AvailableGPUs, also copy the gpu properties if any
	StringTokenIterator tags(resnames);
	for (const char * tag = tags.first(); tag != nullptr; tag = tags.next()) {
		ExprTree * tree = srcAd.Lookup(tag);
		if (tree) { 
			tree = SkipExprEnvelope(tree);
			destAd.Insert(tag, tree->Copy());
		}

		// if there is an attribute "Available<tag>" in the machine ad
		// copy it, and also any attributes that it references
		attr = "Available"; attr += tag;
		tree = srcAd.Lookup(attr);
		if (tree) {
			tree = SkipExprEnvelope(tree);
			destAd.Insert(attr, tree->Copy());

			classad::References refs;
			srcAd.GetInternalReferences(tree, refs, true);
			for (const auto& it : refs) {
				ExprTree * expr = srcAd.Lookup(it);
				if (expr) { 
					expr = SkipExprEnvelope(expr);
					destAd.Insert(it, expr->Copy());
				}
			}
		}
	}
}

// Copy all matching attributes in attrs list along with any referenced attrs
// from srcAd to destAd. If overwrite is True then an attrbute that already
// exists in the destAd can be overwritten by data from the srcAd
void CopySelectAttrs(ClassAd &destAd, const ClassAd &srcAd, const std::string &attrs, bool overwrite)
{
	classad::References refs;
	StringTokenIterator listAttrs(attrs);
	// Create set of attribute references to copy
	for (const auto& attr : listAttrs) {
		ExprTree *tree = srcAd.Lookup(attr);
		if (tree) {
			refs.insert(attr);
			srcAd.GetInternalReferences(tree, refs, true);
		}
	}
	// Copy found references
	for (const auto &it : refs) {
		ExprTree *expr = srcAd.Lookup(it);
		if (expr) {
			// Only copy if given overwrite or if not found in destAd
			if (overwrite || !destAd.Lookup(it)) {
				expr = SkipExprEnvelope(expr);
				destAd.Insert(it, expr->Copy());
			}
		}
	}
}

bool EvalExprTree( classad::ExprTree *expr, ClassAd *source,
				  ClassAd *target, classad::Value &result,
				  classad::Value::ValueType type_mask,
				  const std::string & sourceAlias,
				  const std::string & targetAlias )
{
	bool rc = true;
	if ( !expr || !source ) {
		return false;
	}

	const classad::ClassAd *old_scope = expr->GetParentScope();
	classad::MatchClassAd *mad = NULL;

	expr->SetParentScope( source );
	if ( target && target != source ) {
		mad = getTheMatchAd( source, target, sourceAlias, targetAlias );
	}
	if ( !source->EvaluateExpr( expr, result, type_mask ) ) {
		rc = false;
	}

	if ( mad ) {
		releaseTheMatchAd();
	}
	expr->SetParentScope( old_scope );

	return rc;
}

bool IsAMatch( ClassAd *ad1, ClassAd *ad2 )
{
	classad::MatchClassAd *mad = getTheMatchAd( ad1, ad2 );

	bool result = mad->symmetricMatch();

	releaseTheMatchAd();
	return result;
}

static classad::MatchClassAd *match_pool = NULL;
static ClassAd *target_pool = NULL;
static std::vector<ClassAd*> *matched_ads = NULL;

bool ParallelIsAMatch(ClassAd *ad1, std::vector<ClassAd*> &candidates, std::vector<ClassAd*> &matches, int threads, bool halfMatch)
{
	int adCount = candidates.size();
	static size_t cpu_count = 0;
	size_t current_cpu_count = threads;
	int iterations = 0;
	size_t matched = 0;

	if(cpu_count != current_cpu_count)
	{
		cpu_count = current_cpu_count;
		if(match_pool)
		{
			delete[] match_pool;
			match_pool = NULL;
		}
		if(target_pool)
		{
			delete[] target_pool;
			target_pool = NULL;
		}
		if(matched_ads)
		{
			delete[] matched_ads;
			matched_ads = NULL;
		}
	}

	if(!match_pool)
		match_pool = new classad::MatchClassAd[cpu_count];
	if(!target_pool)
		target_pool = new ClassAd[cpu_count];
	if(!matched_ads)
		matched_ads = new std::vector<ClassAd*>[cpu_count];

	if(!candidates.size())
		return false;

	for(size_t index = 0; index < cpu_count; index++)
	{
		target_pool[index].CopyFrom(*ad1);
		match_pool[index].ReplaceLeftAd(&(target_pool[index]));
		matched_ads[index].clear();
	}

	iterations = ((candidates.size() - 1) / cpu_count) + 1;

#ifdef _OPENMP
	omp_set_num_threads(cpu_count);
#endif

#pragma omp parallel
	{

#ifdef _OPENMP
		int omp_id = omp_get_thread_num();
#else
		int omp_id = 0;
#endif
		for(int index = 0; index < iterations; index++)
		{
			bool result = false;
			int offset = omp_id + index * cpu_count;
			if(offset >= adCount)
				break;
			ClassAd *ad2 = candidates[offset];

			match_pool[omp_id].ReplaceRightAd(ad2);
		
			if(halfMatch)
				result = match_pool[omp_id].rightMatchesLeft();
			else
				result = match_pool[omp_id].symmetricMatch();

			match_pool[omp_id].RemoveRightAd();

			if(result)
			{
				matched_ads[omp_id].push_back(ad2);
			}
		}
	}

	for(size_t index = 0; index < cpu_count; index++)
	{
		match_pool[index].RemoveLeftAd();
		matched += matched_ads[index].size();
	}

	if(matches.capacity() < matched)
		matches.reserve(matched);

	for(size_t index = 0; index < cpu_count; index++)
	{
		if(matched_ads[index].size())
			matches.insert(matches.end(), matched_ads[index].begin(), matched_ads[index].end());
	}

	return matches.size() > 0;
}

bool IsAConstraintMatch( ClassAd *query, ClassAd *target )
{
	classad::MatchClassAd *mad = getTheMatchAd( query, target );

	bool result = mad->rightMatchesLeft();

	releaseTheMatchAd();
	return result;
}

bool IsATargetMatch( ClassAd *my, ClassAd *target, const char * targetType )
{
	// first check to see that the MyType of the target matches the desired targetType
	if (targetType && targetType[0] && YourStringNoCase(targetType) != ANY_ADTYPE) {
		char const *mytype_of_target = GetMyTypeName(*target);
		if( !mytype_of_target ) {
			mytype_of_target = "";
		}
		if (YourStringNoCase(targetType) != mytype_of_target) {
			return false;
		}
	}

	return IsAConstraintMatch(my, target);
}


/**************************************************************************
 *
 * Function: AddClassAdXMLFileHeader
 * Purpose:  Print the stuff that should appear at the beginning of an
 *           XML file that contains a series of ClassAds.
 *
 **************************************************************************/
void AddClassAdXMLFileHeader(std::string &buffer)
{
	buffer += "<?xml version=\"1.0\"?>\n";
	buffer += "<!DOCTYPE classads SYSTEM \"classads.dtd\">\n";
	buffer += "<classads>\n";
	return;

}

/**************************************************************************
 *
 * Function: AddClassAdXMLFileFooter
 * Purpose:  Print the stuff that should appear at the end of an XML file
 *           that contains a series of ClassAds.
 *
 **************************************************************************/
void AddClassAdXMLFileFooter(std::string &buffer)
{
	buffer += "</classads>\n";
	return;

}

ClassAdFileParseType::ParseType parseAdsFileFormat(const char * arg, ClassAdFileParseType::ParseType def_parse_type)
{
	ClassAdFileParseType::ParseType parse_type = def_parse_type;
	YourString fmt(arg);
	if (fmt == "long") { parse_type = ClassAdFileParseType::Parse_long; }
	else if (fmt == "json") { parse_type = ClassAdFileParseType::Parse_json; }
	else if (fmt == "xml") { parse_type = ClassAdFileParseType::Parse_xml; }
	else if (fmt == "new") { parse_type = ClassAdFileParseType::Parse_new; }
	else if (fmt == "auto") { parse_type = ClassAdFileParseType::Parse_auto; }
	return parse_type;
}

