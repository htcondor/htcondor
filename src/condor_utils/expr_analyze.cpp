/***************************************************************
 *
 * Copyright (C) 2014-2015, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_attributes.h"
#include "string_list.h"
#include "CondorError.h"
#include "ad_printmask.h"
#include "daemon.h" // for STARTD_ADTYPE

#include <map>
#include <vector>
#include "classad/classadCache.h" // for CachedExprEnvelope
#include "expr_analyze.h"


// return true if expression is an attribute refer that will resolve
// to the current ad, either because it is an explicit MY.Foo, or a bare Foo that is present in myad
bool ExprTreeIsMyRef(classad::ExprTree * expr, ClassAd * myad)
{
	if ( ! expr) return false;
	expr = SkipExprParens(expr);
	if (expr->GetKind() != classad::ExprTree::ATTRREF_NODE) return false;

	// we want to push TARGET refs, but not other refs
	bool absAttr = false, absScope = false;
	std::string strAttr, strScope;
	classad::ExprTree *scope=nullptr, *scope2=nullptr;
	((classad::AttributeReference*)expr)->GetComponents(scope, strAttr, absAttr);
	if (scope && scope->GetKind() == classad::ExprTree::ATTRREF_NODE) {
		((classad::AttributeReference*)scope)->GetComponents(scope2, strScope, absScope);
		if (YourStringNoCase("MY") == strScope) {
			return true;
		}
	} else if ( ! scope && myad->Lookup(strAttr)) {
		return true;
	}
	return false;
}

// AnalyzeThisSubExpr recursively walks an ExprTree and builds a vector of
// this class, one entry for each clause of the ExprTree that deserves to be
// separately evaluated. 
//
class AnalSubExpr {

public:
	// these members are set while parsing the SubExpr
	classad::ExprTree *  tree; // this pointer is NOT owned by this class, and should not be freeed from here!!
	int  depth;	   // nesting depth (parens only)
	int  logic_op; // 0 = non-logic, 1=!, 2=||, 3=&&, 4=?:
	int  ix_left;
	int  ix_right;
	int  ix_grip;
	int  ix_effective;  // when this clause reduces down to exactly the same as another clause.
	std::string label;

	// these are used during iteration of Target ads against each of the sub-expressions
	int  matches;
	int  hard_value;  // if constant, this is set to the value
	int  pruned_by;   // index of entry that set dont_care
	bool constant;
	bool variable;
	bool dont_care;
	bool reported;
	std::string unparsed;

	AnalSubExpr(classad::ExprTree * expr, const char * lbl, int dep, int logic=0)
		: tree(expr)
		, depth(dep)
		, logic_op(logic)
		, ix_left(-1)
		, ix_right(-1)
		, ix_grip(-1)
		, ix_effective(-1)
		, label(lbl)
		, matches(0)
		, hard_value(-1)
		, pruned_by(-1)
		, constant(false)
		, variable(false)
		, dont_care(false)
		, reported(false)
		{
		};

	void CheckIfConstant(ClassAd & ad)
	{
		classad::ClassAdUnParser unparser;
		unparser.Unparse(unparsed, tree);

#if 1
		classad::References target;
		GetExprReferences(unparsed.c_str(), ad, NULL, &target);
		constant = target.empty();
#else
		classad::References myrefs;
		classad::References target;
		GetExprReferences(unparsed.c_str(), ad, &myrefs, &target);
		constant = target.empty();
		if (constant && myrefs.count("CurrentTime")) {
			constant = false;
		}
#endif
		if (constant) {
			hard_value = 0;
			bool bool_val = false;
			classad::Value eval_result;
			if (EvalExprToBool(tree, &ad, NULL, eval_result)
				&& eval_result.IsBooleanValue(bool_val)
				&& bool_val) {
				hard_value = 1;
			}
		}
	};

	const char * Label()
	{
		if (label.empty()) {
			if (MakeLabel(label)) {
				// nothing to do.
			} else if ( ! unparsed.empty()) {
				return unparsed.c_str();
			} else {
				return "empty";
			}
		}
		return label.c_str();
	}

	bool MakeLabel(std::string & lbl)
	{
		if (logic_op) {
			if (logic_op < 2)
				formatstr(lbl, " ! [%d]", ix_left);
			else if (logic_op > 3)
				formatstr(lbl, (logic_op==4) ? "[%d] ? [%d] : [%d]" : "ifThenElse([%d],[%d],[%d])", ix_left, ix_right, ix_grip);
			else
				formatstr(lbl, "[%d] %s [%d]", ix_left, (logic_op==2) ? "||" : "&&", ix_right);
			return true;
		}
		return false;
	}

	// helper function that returns width-adjusted step numbers
	const char * StepLabel(std::string & str, int index, int width=4) {
		formatstr(str, "[%d]      ", index);
		str.erase(width+2, std::string::npos);
		return str.c_str();
	}
};

// printf formatting helper for indenting
#if 0 // indent using counted spaces
static const char * GetIndentPrefix(int indent) {
  #if 0
	static std::string str = "                                            ";
	while (str.size()/1 < indent) { str += "  "; }
	return &str.c_str()[str.size() - 1*indent];
  #else  // indent using N> 
	static std::string str;
	formatstr(str, "%d>     ", indent);
	str.erase(5, string::npos);
	return str.c_str();
  #endif
}
static const bool analyze_show_work = true;
#else
static const char * GetIndentPrefix(int) { return ""; }
#endif

static std::string s_strStep;
#define StepLbl(ii) subs[ii].StepLabel(s_strStep, ii, 3)

int AnalyzeThisSubExpr(
	ClassAd *myad,
	classad::ExprTree* expr,
	classad::References & inline_attrs, // expand attrs with these names inline
	std::vector<AnalSubExpr> & clauses,
	bool & varres, bool must_store, int depth,
	const anaFormattingOptions & fmt)
{
	classad::ExprTree::NodeKind kind = expr->GetKind( );
	classad::ClassAdUnParser unparser;

	bool show_work = fmt.detail_mask & detail_diagnostic;
	bool show_ifthenelse = fmt.detail_mask & detail_show_ifthenelse;
	bool my_elvis_is_not_interesting = true; // ?: operator should get an index [] number (i.e. push_it)
	bool elvis_is_never_interesting = false; // this was set to 'true' for years (until 23.x)
	bool evaluate_logical = false;
	int  child_depth = depth;
	int  logic_op = 0;
	bool push_it = must_store;
	bool is_time = false;
	bool chatty = (fmt.detail_mask & detail_diagnostic) != 0;
	const char * pop = "";
	int ix_me = -1, ix_left = -1, ix_right = -1, ix_grip = -1;

	std::string strLabel;

	classad::ExprTree *left=NULL, *right=NULL, *gripping=NULL;
	switch(kind) {
		case classad::ExprTree::LITERAL_NODE: {
			classad::Value val;
			classad::Value::NumberFactor factor;
			((classad::Literal*)expr)->GetComponents(val, factor);
			unparser.UnparseAux(strLabel, val, factor);
			if (chatty) {
				printf("     %d:const : %s\n", kind, strLabel.c_str());
			}
			show_work = false;
			break;
		}

		case classad::ExprTree::ATTRREF_NODE: {
			bool absolute;
			std::string strAttr; // simple attr, need unparse to get TARGET. or MY. prefix.
			((classad::AttributeReference*)expr)->GetComponents(left, strAttr, absolute);
			if ( ! left && MATCH == strcasecmp(strAttr.c_str(),"CurrentTime")) {
				is_time = true;
				varres = true;
			}
			if (chatty) {
				printf("     %d:attr  : %s %s at %p%s\n", kind, absolute ? "abs" : "ref", strAttr.c_str(), left, is_time ? " {variable-result}" : "");
			}
			if (absolute) {
				left = NULL;
			} else if ( ! left && inline_attrs.find(strAttr) != inline_attrs.end()) {
				// special case for inline_attrs and CurrentTime expressions, we want behave as if the *value* of the
				// attribute were here rather than just the attr reference.
				left = myad->LookupExpr(strAttr);
				printf("              : inlining %s = %p\n", strAttr.c_str(), left);
			}
			show_work = false;
			break;
		}

		case classad::ExprTree::OP_NODE: {
			classad::Operation::OpKind op = classad::Operation::__NO_OP__;
			((classad::Operation*)expr)->GetComponents(op, left, right, gripping);
			pop = "??";
			if (op <= classad::Operation::__LAST_OP__) 
				pop = unparser.opString[op];
			if (chatty) {
				printf("     %d:op    : %2d:%s %p %p %p\n", kind, op, pop, left, right, gripping);
			}
			if (op >= classad::Operation::__COMPARISON_START__ && op <= classad::Operation::__COMPARISON_END__) {
				//show_work = true;
				evaluate_logical = false;
				push_it = true;
			} else if (op >= classad::Operation::__LOGIC_START__ && op <= classad::Operation::__LOGIC_END__) {
				//show_work = true;
				evaluate_logical = true;
				logic_op = 1 + (int)(op - classad::Operation::__LOGIC_START__);
				push_it = true;
			} else if (op == classad::Operation::PARENTHESES_OP){
				push_it = false;
				evaluate_logical = true;
				child_depth += 1;
			} else if (op == classad::Operation::TERNARY_OP && ! right) {
				// when the right op of the ?: operator is NULL, this is the defaulting operator
				// elvis of MY refs are not interesting,
				// elvis of possible TARGET refs are interesting
				if (my_elvis_is_not_interesting) {
					if (ExprTreeIsMyRef(left, myad) && SkipExprParens(gripping)->GetKind() == classad::ExprTree::LITERAL_NODE) {
						push_it = false;
					} else {
						//logic_op = 4;
						//evaluate_logical = true;
					}
				} else if (elvis_is_never_interesting) {
					push_it = false;
				}
			} else {
				//show_work = false;
			}
			break;
		}

		case classad::ExprTree::FN_CALL_NODE: {
			std::vector<classad::ExprTree*> args;
			((classad::FunctionCall*)expr)->GetComponents(strLabel, args);
			if ( ! args.size() && MATCH == strcasecmp(strLabel.c_str(),"time")) {
				is_time = true;
				varres = true;
			} else if (show_ifthenelse && args.size() == 3 && MATCH == strcasecmp(strLabel.c_str(), "ifthenelse")) {
				push_it = true;
				evaluate_logical = true;
				logic_op = 5;
				left = args[0];
				right = args[1];
				gripping = args[2];
			}
			strLabel += "()";
			if (chatty) {
				printf("     %d:call  : %s %d args%s\n", kind, strLabel.c_str(), (int)args.size(), is_time ? " {variable-result}" : "");
			}
			if (must_store) {
				std::string str;
				unparser.Unparse(str, expr);
				if ( ! str.empty()) strLabel = str;
			}
			break;
		}

		case classad::ExprTree::CLASSAD_NODE: {
			std::vector< std::pair<std::string, classad::ExprTree*> > attrsT;
			((classad::ClassAd*)expr)->GetComponents(attrsT);
			if (chatty) {
				printf("     %d:ad    : %d attrs\n", kind, (int)attrsT.size());
			}
			break;
		}

		case classad::ExprTree::EXPR_LIST_NODE: {
			std::vector<classad::ExprTree*> exprs;
			((classad::ExprList*)expr)->GetComponents(exprs);
			if (chatty) {
				printf("     %d:list  : %d items\n", kind, (int)exprs.size());
			}
			break;
		}
		
		case classad::ExprTree::EXPR_ENVELOPE: {
			// recurse b/c we indirect for this element.
			left = ((classad::CachedExprEnvelope*)expr)->get();
			if (chatty) {
				printf("     %d:env  :     %p \n", kind, left);
			}
			break;
		}
	}

	bool vr_left = false, vr_right = false, vr_grip = false;

	if (left) ix_left = AnalyzeThisSubExpr(myad, left, inline_attrs, clauses, vr_left, evaluate_logical, child_depth, fmt);
	if (right) ix_right = AnalyzeThisSubExpr(myad, right, inline_attrs, clauses, vr_right, evaluate_logical, child_depth, fmt);
	if (gripping) ix_grip = AnalyzeThisSubExpr(myad, gripping, inline_attrs, clauses, vr_grip, evaluate_logical, child_depth, fmt);

	varres = varres || vr_left || vr_right || vr_grip;

	if (push_it) {
		if (left && ! right && ! gripping && ix_left >= 0) {
			ix_me = ix_left;
		} else {
			ix_me = (int)clauses.size();
			AnalSubExpr sub(expr, strLabel.c_str(), depth, logic_op);
			sub.variable = varres;
			sub.ix_left = ix_left;
			sub.ix_right = ix_right;
			sub.ix_grip = ix_grip;
			clauses.push_back(sub);
		}
	} else if (left && ! right && ! gripping) {
		ix_me = ix_left;
	}

	if (show_work) {

		std::string strExpr;
		unparser.Unparse(strExpr, expr);
		if (push_it) {
			if (left && ! right && ! gripping && ix_left >= 0) {
				printf("(---):");
			} else {
				printf("(%3d):", (int)clauses.size()-1);
			}
		} else { printf("      "); }

		if (evaluate_logical) {
			printf("[%3d] %5s : [%3d] %s [%3d] %s\n", 
				   ix_me, "", ix_left, pop, ix_right,
				   chatty ? strExpr.c_str() : "");
		} else {
			printf("[%3d] %5s : %s\n", ix_me, "", strExpr.c_str());
		}
	}

	return ix_me;
}

// recursively mark subexpressions as irrelevant
//
static void MarkIrrelevant(std::vector<AnalSubExpr> & subs, int index, std::string & irr_path, int at_index)
{
	if (index >= 0) {
		subs[index].dont_care = true;
		subs[index].pruned_by = at_index;
		//printf("(%d:", index);
		formatstr_cat(irr_path,"(%d:", index);
		if (subs[index].ix_left >= 0)  MarkIrrelevant(subs, subs[index].ix_left, irr_path, at_index);
		if (subs[index].ix_right >= 0) MarkIrrelevant(subs, subs[index].ix_right, irr_path, at_index);
		if (subs[index].ix_grip >= 0)  MarkIrrelevant(subs, subs[index].ix_grip, irr_path, at_index);
		formatstr_cat(irr_path,")");
		//printf(")");
	}
}


static void AnalyzePropagateConstants(std::vector<AnalSubExpr> & subs, bool show_work)
{

	// propagate hard true-false
	for (int ix = 0; ix < (int)subs.size(); ++ix) {

		int ix_effective = -1;
		int ix_irrelevant = -1;
		bool soft_irrelevant = false;

		const char * pindent = GetIndentPrefix(subs[ix].depth);

		// fixup logic_op entries, propagate hard true/false

		if (subs[ix].logic_op) {
			// unset, false, true, indeterminate, undefined, error, unset
			static const char * truthy[] = {"?", "F", "T", "", "u", "e", "?", "f", "t", "s"};
			int hard_left = 2, hard_right = 2, hard_grip = 2;
			bool soft_left = false, soft_right = false, soft_grip = false;
			if (subs[ix].ix_left >= 0) {
				int il = subs[ix].ix_left;
				if (subs[il].constant) {
					hard_left = subs[il].hard_value;
					soft_left = subs[il].variable;
				}
			}
			if (subs[ix].ix_right >= 0) {
				int ir = subs[ix].ix_right;
				if (subs[ir].constant) {
					hard_right = subs[ir].hard_value;
					soft_right = subs[ir].variable;
				}
			}
			if (subs[ix].ix_grip >= 0) {
				int ig = subs[ix].ix_grip;
				if (subs[ig].constant) {
					hard_grip = subs[ig].hard_value;
					soft_grip = subs[ig].variable;
				}
			}

			switch (subs[ix].logic_op) {
				case 1: { // ! 
					formatstr(subs[ix].label, " ! [%d]%s", subs[ix].ix_left, truthy[hard_left+1+(soft_left*6)]);
					break;
				}
				case 2: { // || 
					// true || anything is true
					if (hard_left == 1 || hard_right == 1) {
						subs[ix].constant = true;
						subs[ix].hard_value = true;
						subs[ix].variable = soft_left && soft_right;
						// mark the non-constant irrelevant clause
						if (hard_left == 1) {
							subs[ix].ix_effective = ix_effective = subs[ix].ix_left;
							ix_irrelevant = subs[ix].ix_right;
							soft_irrelevant = soft_left && (hard_right || soft_right);
						} else if (hard_right == 1) {
							subs[ix].ix_effective = ix_effective = subs[ix].ix_right;
							ix_irrelevant = subs[ix].ix_left;
							soft_irrelevant = soft_right && (hard_left || soft_left);
						}
					} else
					// false || false is false
					if (hard_left == 0 && hard_right == 0) {
						subs[ix].constant = true;
						subs[ix].variable = soft_left || soft_right;
						subs[ix].hard_value = false;
					} else if (hard_left == 0) {
						subs[ix].ix_effective = ix_effective = subs[ix].ix_right;
						ix_irrelevant = subs[ix].ix_left;
						soft_irrelevant = soft_left;
					} else if (hard_right == 0) {
						subs[ix].ix_effective = ix_effective = subs[ix].ix_left;
						ix_irrelevant = subs[ix].ix_right;
						soft_irrelevant = soft_right;
					}
					formatstr(subs[ix].label, "[%d]%s || [%d]%s", 
					          subs[ix].ix_left, truthy[hard_left+1+(soft_left*6)], 
					          subs[ix].ix_right, truthy[hard_right+1+(soft_right*6)]);
					break;
				}
				case 3: { // &&
					// false && anything is false
					if (hard_left == 0 || hard_right == 0) {
						subs[ix].constant = true;
						subs[ix].variable = soft_left || soft_right;
						subs[ix].hard_value = false;
						// mark the non-constant irrelevant clause
						if (hard_left == 0) {
							subs[ix].ix_effective = ix_effective = subs[ix].ix_left;
							ix_irrelevant = subs[ix].ix_right;
							soft_irrelevant = soft_left;
						} else if (hard_right == 0) {
							subs[ix].ix_effective = ix_effective = subs[ix].ix_right;
							ix_irrelevant = subs[ix].ix_left;
							soft_irrelevant = soft_right;
						}
					} else
					// true && true is true
					if (hard_left == 1 && hard_right == 1) {
						subs[ix].constant = true;
						subs[ix].variable = soft_left || soft_right;
						subs[ix].hard_value = true;
					} else if (hard_left == 1) {
						subs[ix].ix_effective = ix_effective = subs[ix].ix_right;
					} else if (hard_right == 1) {
						subs[ix].ix_effective = ix_effective = subs[ix].ix_left;
					}
					formatstr(subs[ix].label, "[%d]%s && [%d]%s", 
					          subs[ix].ix_left, truthy[hard_left+1+(soft_left*6)], 
					          subs[ix].ix_right, truthy[hard_right+1+(soft_right*6)]);
					break;
				}
				case 4: // ?:
				case 5: { // ifthenelse
					if (hard_left == 0 || hard_left == 1) {
						ix_effective = hard_left ? subs[ix].ix_right : subs[ix].ix_grip; 
						subs[ix].ix_effective = ix_effective;
						if ((ix_effective >= 0) && subs[ix_effective].constant) {
							subs[ix].constant = true;
							subs[ix].variable = soft_left;
							subs[ix].hard_value = subs[ix_effective].hard_value;
						}
						// mark the irrelevant clause
						ix_irrelevant = hard_left ? subs[ix].ix_grip : subs[ix].ix_right;
						soft_irrelevant = soft_left;
					}
					formatstr(subs[ix].label, (subs[ix].logic_op==4) ? "[%d]%s ? [%d]%s : [%d]%s" : "ifThenElse([%d]%s, [%d]%s, [%d]%s)",
					          subs[ix].ix_left, truthy[hard_left+1+(soft_left*6)], 
					          subs[ix].ix_right, truthy[hard_right+1+(soft_right*6)], 
					          subs[ix].ix_grip, truthy[hard_grip+1+(soft_grip*6)]);
					break;
				}
			}
		}

		// now walk effective expressions backward
		std::string effective_path = "";
		if (ix_effective >= 0) {
			if (ix_irrelevant < 0) {
				if (ix_effective == subs[ix].ix_left)
					ix_irrelevant = subs[ix].ix_right;
				if (ix_effective == subs[ix].ix_right)
					ix_irrelevant = subs[ix].ix_left;
				// this marks some things as soft that really aren't. but its needed to make sure parents of soft prunes are also soft.
				if (subs[ix].variable)
					soft_irrelevant = true;
			}
			formatstr(effective_path, "%d->%d", ix, ix_effective);
			while (subs[ix_effective].ix_effective >= 0) {
				ix_effective = subs[ix_effective].ix_effective;
				subs[ix].ix_effective = ix_effective;
				formatstr_cat(effective_path, "->%d", ix_effective);
			}
		}

		std::string irr_path = "";
		if (ix_irrelevant >= 0) {
			if (show_work) { printf("\tMarkIrrelevant(%d,%s) by %d = ", ix_irrelevant, soft_irrelevant?"soft":"hard", ix); }
			if ( ! soft_irrelevant) {
				MarkIrrelevant(subs, ix_irrelevant, irr_path, ix);
			}
			if (show_work) { printf("\n"); }
		}


		if (show_work) {

			const char * const_val = "";
			if (subs[ix].constant) {
				const_val = subs[ix].hard_value ? "always" : "never";
				if (subs[ix].variable) {
					const_val = subs[ix].hard_value ? "usually" : "seldom";
				}
			}
			if (ix_effective >= 0) {
				printf("%s %5s\t%s%s\t is effectively %s e<%s>\n", 
					   StepLbl(ix), const_val, pindent, subs[ix].Label(), subs[ix_effective].Label(), effective_path.c_str());
			} else {
				printf("%s %5s\t%s%s\n", StepLbl(ix), const_val, pindent, subs[ix].Label());
			}
			if (ix_irrelevant >= 0) {
				printf("           \tpruning %s\n", irr_path.c_str());
			}
		}
	} // check-if-constant
}

// insert spaces and \n into temp_buffer so that it will print out neatly on a display with the given width
// spaces are added at the start of each newly created line for the given indent,
// but spaces are NOT added to the start of the input buffer.
//
const char * PrettyPrintExprTree(classad::ExprTree *tree, std::string & temp_buffer, int indent, int width)
{
	classad::ClassAdUnParser unparser;
	unparser.Unparse(temp_buffer, tree);

	if (indent > width)
		indent = width*2/3;

	std::string::iterator it, lastAnd, lineStart;
	it = lastAnd = lineStart = temp_buffer.begin();
	int indent_at_last_and = indent;
	int pos = indent;  // we presume that the initial indent has already been printed by the caller.
	bool was_and = false;

	char chPrev = 0;
	while (it != temp_buffer.end()) {

		// as we iterate characters, keep track of the indent level
		// and whether the current position is the 2nd character of && or ||
		//
		bool is_and = false;
		if ((*it == '&' || *it == '|') && chPrev == *it) {
			is_and = true;
		}
		else if (*it == '(') { indent += 2; }
		else if (*it == ')') { indent -= 2; }

		// if the current position exceeds the width, we want to replace
		// the character after the last && or || with a \n. We can count on
		// the call putting a space after each && or ||, so replacing is safe.
		// Note: this code may insert charaters into the stream, but will only do
		// so BEFORE the current position, and it gurantees that the iterator
		// will still point to the same character after the insert as it did before,
		// so we don't loose our place.
		//
		if (pos >= width) {
			if (lastAnd != lineStart) {
				temp_buffer.replace(lastAnd, lastAnd + 1, 1, '\n');
				lineStart = lastAnd + 1;
				lastAnd = lineStart;
				pos = 0;

				// if we have to indent, we do that by inserting spaces at the start of line
				// then we have to fixup iterators and position counters to account for
				// the insertion. we do the fixup to avoid problems caused by std:string realloc
				// as well as to avoid scanning any character in the input string more than once,
				// which in turn guarantees that this code will always get to the end.
				if (indent_at_last_and > 0) {
					size_t cch = distance(temp_buffer.begin(), it);
					size_t cchStart = distance(temp_buffer.begin(), lineStart);
					temp_buffer.insert(lineStart, indent_at_last_and, ' ');
					// it, lineStart & lastAnd can be invalid if the insert above reallocated the buffer.
					// so we have to recreat them.
					it = temp_buffer.begin() + cch + indent_at_last_and;
					lastAnd = lineStart = temp_buffer.begin() + cchStart;
					pos = (int) distance(lineStart, it);
				}
				indent_at_last_and = indent;
			}
		}
		// was_and will be true if this is the character AFTER && or ||
		// if it is, this is a potential place to put a \n, so remember it.
		if (was_and) {
			lastAnd = it;
			indent_at_last_and = indent;
		}

		chPrev = *it;
		++it;
		++pos;
		was_and = is_and;
	}
	return temp_buffer.c_str();
}

// This is the function you probably want to call.
// It does match analysis of all of the clauses in the given attribute of the request ad
// against all of the target ads and appends the analysis to the given return_buf.
// typical use would be
//    AnalyzeRequirementsForEachTarget(jobAd, ATTR_REQUIREMENTS, attrs, &startdAds[0], startdAds.size(), ...
// or
//    ClassAd::References attrs; attrs.insert(ATTR_START);
//    AnalyzeRequirementsForEachTarget(slotClassAd, ATTR_REQUIREMENTS, attrs, &jobAds[0], jobAds.size(), ...
//
void AnalyzeRequirementsForEachTarget(
	ClassAd *request,
	const char * attrConstraint, // must be an attribute in the request ad
	classad::References & inline_attrs, // expand these attrs inline
	std::vector<ClassAd *> targets,
	std::string & return_buf,
	const anaFormattingOptions & fmt)
{
	int console_width = fmt.console_width;
	bool show_work = (fmt.detail_mask & detail_diagnostic) != 0;
	const bool count_soft_matches = false; // when true, "soft" always and never show  up as counts of machines

	/*
	bool request_is_machine = false;
	if (0 == strcmp(GetMyTypeName(*request),STARTD_ADTYPE)) {
		//attrConstraint = ATTR_START;
		request_is_machine = true;
	}
	*/

	classad::ExprTree* exprReq = request->LookupExpr(attrConstraint);
	if ( ! exprReq)
		return;

	std::vector<AnalSubExpr> subs;
	std::string strStep;
	#undef StepLbl
	#define StepLbl(ii) subs[ii].StepLabel(strStep, ii, 3)

	if (show_work) { printf("\nDump ExprTree in evaluation order\n"); }

	bool variable_result = false;
	AnalyzeThisSubExpr(request, exprReq, inline_attrs, subs, variable_result, true, 0, fmt);

	if (fmt.detail_mask & detail_dump_intermediates) {
		printf("\nDump subs %s\n", variable_result ? "(variable)" : "");
		classad::ClassAdUnParser unparser;
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			printf("[%3d]%p %2d %2d %c [%3d %3d %3d] %3d %3d %3d ", ix, subs[ix].tree, subs[ix].depth, subs[ix].logic_op,
				subs[ix].variable ? 'v' : ' ',
				subs[ix].ix_left, subs[ix].ix_right, subs[ix].ix_grip,
				subs[ix].ix_effective, (int)subs[ix].label.size(), (int)subs[ix].unparsed.size());
			std::string lbl;
			if (subs[ix].MakeLabel(lbl)) {
			} else if (subs[ix].ix_left < 0) {
				unparser.Unparse(lbl, subs[ix].tree);
			} else {
				lbl = "";
			}
			printf("%s\n", lbl.c_str());
		}
	}

	// check of each sub-expression has any target references.
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		subs[ix].CheckIfConstant(*request);
	}

	if (fmt.detail_mask & detail_dump_intermediates) {
		printf("\nDump subs %s after constant detection\n", variable_result ? "(variable)" : "");
		classad::ClassAdUnParser unparser;
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			printf("[%3d]%p %2d %2d %c [%3d %3d %3d] %3d %3d %3d %d %2d ", ix, subs[ix].tree, subs[ix].depth, subs[ix].logic_op,
				subs[ix].variable ? 'v' : ' ',
				subs[ix].ix_left, subs[ix].ix_right, subs[ix].ix_grip,
				subs[ix].ix_effective, (int)subs[ix].label.size(), (int)subs[ix].unparsed.size(),
				subs[ix].constant, subs[ix].hard_value
				);
			std::string lbl;
			if (subs[ix].MakeLabel(lbl)) {
			} else if (subs[ix].ix_left < 0) {
				unparser.Unparse(lbl, subs[ix].tree);
			} else {
				lbl = "";
			}
			printf("%s\n", lbl.c_str());
		}
	}

	if (show_work) { printf("\nEvaluate constants:\n"); }
	AnalyzePropagateConstants(subs, show_work);

	//
	// now print out the strength-reduced expression
	//
	if (show_work) {
		printf("\nReduced boolean expressions:\n");
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			const char * const_val = "";
			if (subs[ix].constant) {
				const_val = subs[ix].hard_value ? "always" : "never";
				if (subs[ix].variable) {
					const_val = subs[ix].hard_value ? "usually" : "seldom";
				}
			}
			std::string pruned = "";
			if (subs[ix].dont_care) {
				const_val = (subs[ix].constant) ? (subs[ix].hard_value ? "n/aT" : "n/aF") : "n/a";
				formatstr(pruned, "\tpruned-by:%d", subs[ix].pruned_by);
			}
			if (subs[ix].ix_effective < 0) {
				const char * pindent = GetIndentPrefix(subs[ix].depth);
				printf("%s %5s\t%s%s%s\n", StepLbl(ix), const_val, pindent, subs[ix].Label(), pruned.c_str());
			}
		}
	}

		// propagate effectives back up the chain. 
		//

	if (show_work) {
		printf("\nPropagate effectives:\n");
	}
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		bool fchanged = false;
		int jj = subs[ix].ix_left;
		if ((jj >= 0) && (subs[jj].ix_effective >= 0)) {
			subs[ix].ix_left = subs[jj].ix_effective;
			fchanged = true;
		}

		jj = subs[ix].ix_right;
		if ((jj >= 0) && (subs[jj].ix_effective >= 0)) {
			subs[ix].ix_right = subs[jj].ix_effective;
			fchanged = true;
		}

		jj = subs[ix].ix_grip;
		if ((jj >= 0) && (subs[jj].ix_effective >= 0)) {
			subs[ix].ix_grip = subs[jj].ix_effective;
			fchanged = true;
		}

		if (fchanged) {
			// force the label to be rebuilt.
			std::string oldlbl = subs[ix].label;
			if (oldlbl.empty()) oldlbl = "";
			subs[ix].label.clear();
			if (show_work) {
				printf("%s   %s  is effectively  %s\n", StepLbl(ix), oldlbl.c_str(), subs[ix].Label());
			}
		}
	}

	if (fmt.detail_mask & detail_dump_intermediates) {
		printf("\nDump subs\n");
		classad::ClassAdUnParser unparser;
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			printf("[%3d] %2d %2d [%3d %3d %3d] %3d %d %d ", ix, subs[ix].depth, subs[ix].logic_op,
				subs[ix].ix_left, subs[ix].ix_right, subs[ix].ix_grip,
				subs[ix].ix_effective, (int)subs[ix].label.size(), (int)subs[ix].unparsed.size());
			std::string lbl;
			if (subs[ix].MakeLabel(lbl)) {
			} else if (subs[ix].ix_left < 0) {
				unparser.Unparse(lbl, subs[ix].tree);
			} else {
				lbl = "";
			}
			printf("%s\n", lbl.c_str());
		}
	}

	if (show_work) {
		printf("\nFinal expressions:\n");
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			const char * const_val = "";
			if (subs[ix].constant) {
				const_val = subs[ix].hard_value ? "always" : "never";
				if (subs[ix].variable) {
					const_val = subs[ix].hard_value ? "usually" : "seldom";
				}
			}
			if (subs[ix].ix_effective < 0 && ! subs[ix].dont_care) {
				std::string altlbl;
				//if ( ! subs[ix].MakeLabel(altlbl)) altlbl = "";
				const char * pindent = GetIndentPrefix(subs[ix].depth);
				printf("%s %5s\t%s%s\n", StepLbl(ix), const_val, pindent, subs[ix].Label() /*, altlbl.c_str()*/);
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////////

	//
	//
	// build counts of matching machines, notice which expressions match all or no machines
	//
	if (show_work) {
		printf("Evaluation against %s ads:\n", fmt.target_type_name);
		printf(" Step  %8ss  Condition\n", fmt.target_type_name);
	}

	std::string linebuf;
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		if (subs[ix].ix_effective >= 0 || subs[ix].dont_care)
			continue;

		if (subs[ix].constant && ( ! count_soft_matches || ! subs[ix].variable)) {
			if (show_work) {
				const char * const_val = subs[ix].hard_value ? "always" : "never";
				if (subs[ix].variable) const_val = subs[ix].hard_value ? "now" : "not now";
				formatstr(linebuf, "%s %9s  %s%s\n", StepLbl(ix), const_val, GetIndentPrefix(subs[ix].depth), subs[ix].Label());
				printf("%s", linebuf.c_str()); 
			}
			continue;
		}

		subs[ix].matches = 0;

		// do short-circuit evaluation of && operations
		int matches_all = (int)targets.size();
		int ix_left = subs[ix].ix_left;
		int ix_right = subs[ix].ix_right;
		if (ix_left >= 0 && ix_right >= 0 && subs[ix].logic_op) {
			int ix_effective = -1;
			int ix_prune = -1;
			const char * reason = "";
			if (subs[ix].logic_op == 3) { // &&
				reason = "&&";
				if (subs[ix_left].matches == 0) {
					ix_effective = ix_left;
				} else if (subs[ix_right].matches == 0) {
					ix_effective = ix_right;
				}
			} else if (subs[ix].logic_op == 2) { // || 
				if (subs[ix_left].matches == matches_all) {
					reason = "a||";
					ix_effective = ix_left;
					ix_prune = ix_right;
				} else if (subs[ix_right].matches == matches_all) {
					reason = "||a";
					ix_effective = ix_right;
					ix_prune = ix_left;
				} else if (subs[ix_left].matches == 0 && subs[ix_right].matches != 0) {
					reason = "0||";
					ix_effective = ix_right;
					ix_prune = ix_left;
				} else if (subs[ix_left].matches != 0 && subs[ix_right].matches == 0) {
					reason = "||0";
					ix_effective = ix_left;
					ix_prune = ix_right;
				}
			}

			if (ix_prune >= 0) {
				std::string irr_path = "";
				MarkIrrelevant(subs, ix_prune, irr_path, ix);
				if (show_work) { printf("pruning peer [%d] %s %s\n", ix_prune, reason, irr_path.c_str()); }
			}

			if (ix_effective >= 0) {
				while(subs[ix_effective].ix_effective >= 0)
					ix_effective = subs[ix_effective].ix_effective;
				subs[ix].ix_effective = ix_effective;
				if (show_work) { printf("pruning [%d] back to [%d] because %s\n", ix, ix_effective, reason); }
			} else {
				if (subs[ix_left].ix_effective >= 0) {
					subs[ix].ix_left = subs[ix_left].ix_effective;
					subs[ix].label.clear();
				}
				if (subs[ix_right].ix_effective >= 0) {
					subs[ix].ix_right = subs[ix_right].ix_effective;
					subs[ix].label.clear();
				}
			}
		}

		for (size_t ixt = 0; ixt < targets.size(); ++ixt) {
			ClassAd *target = targets[ixt];

			classad::Value eval_result;
			bool bool_val;
			if (EvalExprToBool(subs[ix].tree, request, target, eval_result) && 
				eval_result.IsBooleanValue(bool_val) && 
				bool_val) {
				subs[ix].matches += 1;
			}
		}

		// did the left or right path of a logic op dominate the result?
		if (ix_left >= 0 && ix_right >= 0 && subs[ix].logic_op) {
			int ix_effective = -1;
			const char * reason = "";
			if (subs[ix].logic_op == 3) { // &&
				reason = "&&";
				if (subs[ix_left].matches == subs[ix].matches) {
					ix_effective = ix_left;
					if (subs[ix_right].matches > subs[ix].matches) {
						subs[ix_right].dont_care = true;
						subs[ix_right].pruned_by = ix_left;
						if (show_work) { printf("pruning peer [%d] because of &&>[%d]\n", ix_right, ix_left); }
					}
				} else if (subs[ix_right].matches == subs[ix].matches) {
					ix_effective = ix_right;
					if (subs[ix_left].matches > subs[ix].matches) {
						subs[ix_left].dont_care = true;
						subs[ix_left].pruned_by = ix_right;
						if (show_work) { printf("pruning peer [%d] because of &&>[%d]\n", ix_left, ix_right); }
					}
				}
			} else if (subs[ix].logic_op == 2) { // || 
			}

			if (ix_effective >= 0) {
				while(subs[ix_effective].ix_effective >= 0)
					ix_effective = subs[ix_effective].ix_effective;
				subs[ix].ix_effective = ix_effective;
				subs[ix].label.clear();
				if (show_work) { printf("pruning [%d] back to [%d] because %s\n", ix, ix_effective, reason); }
			} else {
				if (subs[ix_left].ix_effective >= 0) {
					subs[ix].ix_left = subs[ix_left].ix_effective;
					subs[ix].label.clear();
				}
				if (subs[ix_right].ix_effective >= 0) {
					subs[ix].ix_right = subs[ix_right].ix_effective;
					subs[ix].label.clear();
				}
			}
		}

		if (show_work) { 
			formatstr(linebuf, "%s %9d  %s%s\n", StepLbl(ix), subs[ix].matches, GetIndentPrefix(subs[ix].depth), subs[ix].Label());
			printf("%s", linebuf.c_str()); 
		}
	}

	if (fmt.detail_mask & detail_dump_intermediates) {
		printf("\nDump subs\n");
		classad::ClassAdUnParser unparser;
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			printf("[%3d] %2d %2d [%3d %3d %3d] %3d '%s' ", ix, subs[ix].depth, subs[ix].logic_op,
				subs[ix].ix_left, subs[ix].ix_right, subs[ix].ix_grip,
				subs[ix].ix_effective, subs[ix].label.empty() ? "" : subs[ix].label.c_str());
			std::string lbl;
			if (subs[ix].MakeLabel(lbl)) {
			} else if (subs[ix].ix_left < 0) {
				unparser.Unparse(lbl, subs[ix].tree);
			} else {
				lbl = "";
			}
			printf("%s\n", lbl.c_str());
		}
	}

	//
	// render final output
	//
	return_buf.clear();
	if (fmt.detail_mask & detail_show_all_subexprs) return_buf += "   ";
	return_buf.append(7+(9-1-strlen(fmt.target_type_name))/2, ' ');
	return_buf += fmt.target_type_name;
	return_buf += "s\n";
	if (fmt.detail_mask & detail_show_all_subexprs) return_buf += "   ";
	return_buf += "Step    Matched  Condition\n";
	if (fmt.detail_mask & detail_show_all_subexprs) return_buf += "   ";
	return_buf += "-----  --------  ---------\n";
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		bool skip_me = (subs[ix].ix_effective >= 0 || subs[ix].dont_care);
		const char * prefix = "";
		if (fmt.detail_mask & detail_show_all_subexprs) {
			if (fmt.detail_mask & detail_diagnostic) {
				prefix = " * ";
				if (subs[ix].dont_care) prefix = "n/a";
				else if (subs[ix].ix_effective >= 0) prefix = "   ";
			} else {
				prefix = " * ";
				if (subs[ix].dont_care) prefix = "   ";
				else if (subs[ix].ix_effective >= 0) prefix = "   ";
			}
		} else if (skip_me) {
			continue;
		}

		const char * pindent = GetIndentPrefix(subs[ix].depth);
		const char * const_val = "";
		if (subs[ix].constant && ( ! count_soft_matches || ! subs[ix].variable)) {
			const_val = subs[ix].hard_value ? "always" : "never";
			if (subs[ix].variable) const_val = subs[ix].hard_value ? "now" : "not now";
			formatstr(linebuf, "%s %9s  %s%s\n", StepLbl(ix), const_val, pindent, subs[ix].Label());
			return_buf += prefix;
			return_buf += linebuf;
			if (show_work) { printf("%s", linebuf.c_str()); }
			continue;
		}

		formatstr(linebuf, "%s %9d  %s%s", StepLbl(ix), subs[ix].matches, pindent, subs[ix].Label());

		bool append_pretty = false;
		if (subs[ix].logic_op) {
			append_pretty = (fmt.detail_mask & detail_smart_unparse_expr) != 0;
			if (subs[ix].ix_left == ix-2 && subs[ix].ix_right == ix-1)
				append_pretty = false;
			if ((fmt.detail_mask & detail_always_unparse_expr) != 0)
				append_pretty = true;
		}
			
		if (append_pretty) { 
			std::string treebuf;
			linebuf += " ( "; 
			PrettyPrintExprTree(subs[ix].tree, treebuf, (int)linebuf.size(), console_width); 
			linebuf += treebuf; 
			linebuf += " )";
		}

		linebuf += "\n";
		return_buf += prefix;
		return_buf += linebuf;
	}

	// chase zeros back up the expression tree
#if 0
	show_work = (fmt.detail_mask & 8); // temporary
	if (show_work) {
		printf("\nCurrent Table:\nStep  -> Effec Depth Leaf D/C Matches\n");
		for (int ix = 0; ix < (int)subs.size(); ++ix) {

			int leaf = subs[ix].logic_op == 0;
			printf("[%3d] -> [%3d] %5d %4d %3d %7d %s\n", ix, subs[ix].ix_effective, subs[ix].depth, leaf, subs[ix].dont_care, subs[ix].matches, subs[ix].Label());
		}
		printf("\n");
		printf("\nChase Zeros:\nStep  -> Effec Depth Leaf Matches\n");
	}
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		if (subs[ix].ix_effective >= 0 || subs[ix].dont_care)
			continue;

		int leaf = subs[ix].logic_op == 0;
		if (leaf && subs[ix].matches == 0) {
			if (show_work) {
				printf("[%3d] -> [%3d] %5d %4d %7d %s\n", ix, subs[ix].ix_effective, subs[ix].depth, leaf, subs[ix].matches, subs[ix].Label());
			}
			for (int jj = ix+1; jj < (int)subs.size(); ++jj) {
				if (subs[jj].matches != 0)
					continue;
				if (subs[jj].reported)
					continue;
				if ((subs[jj].ix_effective == ix) || subs[jj].ix_left == ix || subs[jj].ix_right == ix || subs[jj].ix_grip == ix) {
					const char * pszop = "  \0 !\0||\0&&\0?:\0  \0"+subs[jj].logic_op*3;
					printf("[%3d] -> [%3d] %5d %4s %7d %s\n", jj, subs[jj].ix_effective, subs[jj].depth, pszop, subs[jj].matches, subs[jj].Label());
					subs[jj].reported = true;
				}
			}
		}
	}
	if (show_work) {
		printf("\n");
	}
#endif

}

#if 0 // experimental code

int EvalThisSubExpr(int & index, classad::ExprTree* expr, ClassAd *request, ClassAd *offer, std::vector<ExprTree*> & clauses, bool must_store)
{
	++index;

	classad::ExprTree::NodeKind kind = expr->GetKind( );
	classad::ClassAdUnParser unp;

	bool evaluate = true;
	bool evaluate_logical = false;
	bool push_it = must_store;
	bool chatty = false;
	const char * pop = "??";
	int ix_me = -1, ix_left = -1, ix_right = -1, ix_grip = -1;

	classad::Operation::OpKind op = classad::Operation::__NO_OP__;
	classad::ExprTree *left=NULL, *right=NULL, *gripping=NULL;
	switch(kind) {
		case classad::ExprTree::LITERAL_NODE: {
			if (chatty) {
				classad::Value val;
				classad::Value::NumberFactor factor;
				((classad::Literal*)expr)->GetComponents(val, factor);
				std::string str;
				unp.UnparseAux(str, val, factor);
				printf("     %d:const : %s\n", kind, str.c_str());
			}
			evaluate = false;
			break;
		}

		case classad::ExprTree::ATTRREF_NODE: {
			bool	absolute;
			std::string attrref;
			((classad::AttributeReference*)expr)->GetComponents(left, attrref, absolute);
			if (chatty) {
				printf("     %d:attr  : %s %s at %p\n", kind, absolute ? "abs" : "ref", attrref.c_str(), left);
			}
			if (absolute) {
				left = NULL;
			}
			evaluate = false;
			break;
		}

		case classad::ExprTree::OP_NODE: {
			((classad::Operation*)expr)->GetComponents(op, left, right, gripping);
			pop = "??";
			if (op <= classad::Operation::__LAST_OP__) 
				pop = unp.opString[op];
			if (chatty) {
				printf("     %d:op    : %2d:%s %p %p %p\n", kind, op, pop, left, right, gripping);
			}
			if (op >= classad::Operation::__COMPARISON_START__ && op <= classad::Operation::__COMPARISON_END__) {
				evaluate = true;
				evaluate_logical = false;
				push_it = true;
			} else if (op >= classad::Operation::__LOGIC_START__ && op <= classad::Operation::__LOGIC_END__) {
				evaluate = true;
				evaluate_logical = true;
				push_it = true;
			} else {
				evaluate = false;
			}
			break;
		}

		case classad::ExprTree::FN_CALL_NODE: {
			std::string strName;
			std::vector<ExprTree*> args;
			((classad::FunctionCall*)expr)->GetComponents(strName, args);
			if (chatty) {
				printf("     %d:call  : %s() %d args\n", kind, strName.c_str(), args.size());
			}
			break;
		}

		case classad::ExprTree::CLASSAD_NODE: {
			vector< std::pair<string, classad::ExprTree*> > attrs;
			((classad::ClassAd*)expr)->GetComponents(attrs);
			if (chatty) {
				printf("     %d:ad    : %d attrs\n", kind, attrs.size());
			}
			//clauses.push_back(expr);
			break;
		}

		case classad::ExprTree::EXPR_LIST_NODE: {
			vector<classad::ExprTree*> exprs;
			((classad::ExprList*)expr)->GetComponents(exprs);
			if (chatty) {
				printf("     %d:list  : %d items\n", kind, exprs.size());
			}
			//clauses.push_back(expr);
			break;
		}
		
		case classad::ExprTree::EXPR_ENVELOPE: {
			// recurse b/c we indirect for this element.
			left = ((classad::CachedExprEnvelope*)expr)->get();
			if (chatty) {
				printf("     %d:env  :     %p\n", kind, left);
			}
			break;
		}
	}

	if (left) ix_left = EvalThisSubExpr(index, left, request, offer, clauses, evaluate_logical);
	if (right) ix_right = EvalThisSubExpr(index, right, request, offer, clauses, evaluate_logical);
	if (gripping) ix_grip = EvalThisSubExpr(index, gripping, request, offer, clauses, evaluate_logical);

	if (push_it) {
		if (left && ! right && ix_left >= 0) {
			ix_me = ix_left;
		} else {
			ix_me = (int)clauses.size();
			clauses.push_back(expr);
			// TJ: for now, so I can see what's happening.
			evaluate = true;
		}
	} else if (left && ! right) {
		ix_me = ix_left;
	}

	if (evaluate) {

		classad::Value eval_result;
		bool           bool_val;
		bool matches = false;
		if (EvalExprToBool(expr, request, offer, eval_result) && eval_result.IsBooleanValue(bool_val) && bool_val) {
			matches = true;
		}

		std::string strExpr;
		unp.Unparse(strExpr, expr);
		if (evaluate_logical) {
			//if (ix_left < 0)  { ix_left = (int)clauses.size(); clauses.push_back(left); }
			//if (ix_right < 0) { ix_right = (int)clauses.size(); clauses.push_back(right); }
			printf("[%3d] %5s : [%3d] %s [%3d]\n", ix_me, matches ? "1" : "0", 
				   ix_left, pop, ix_right);
			if (chatty) {
				printf("\t%s\n", strExpr.c_str());
			}
		} else {
			printf("[%3d] %5s : %s\n", ix_me, matches ? "1" : "0", strExpr.c_str());
		}
	}

	return ix_me;
}

static void EvalRequirementsExpr(ClassAd *request, ClassAdList & offers, std::string & return_buf)
{
	classad::ExprTree* exprReq = request->LookupExpr(ATTR_REQUIREMENTS);
	if (exprReq) {

		std::vector<ExprTree*> clauses;

		offers.Open();
		while (ClassAd *offer = offers.Next()) {
			int counter = 0;
			EvalThisSubExpr(counter, exprReq, request, offer, clauses, true);

			static bool there_can_be_only_one = true;
			if (there_can_be_only_one)
				break; // for now only do the first offer.
		}
		offers.Close();
	}
}

#endif // experimental code above


void AddReferencedAttribsToBuffer(
	ClassAd * request,
	const char * expr_string, // expression string or attribute name
	classad::References & hidden_refs, // don't print these even if they appear in the trefs list
	classad::References & trefs, // out, returns target refs
	bool raw_values, // unparse referenced values if true, print evaluated referenced values if false
	const char * pindent,
	std::string & return_buf)
{
	classad::References refs;
	trefs.clear();

	GetExprReferences(expr_string,*request,&refs,&trefs);
	if (refs.empty() && trefs.empty())
		return;

	if ( ! pindent) pindent = "";

	classad::References::iterator it;
	AttrListPrintMask pm;
	pm.SetAutoSep(NULL, "", "\n", "\n");
	for ( it = refs.begin(); it != refs.end(); it++ ) {
		if (hidden_refs.find(*it) != hidden_refs.end())
			continue;
		std::string label;
		formatstr(label, raw_values ? "%s%s = %%r" : "%s%s = %%V", pindent, it->c_str());
		pm.registerFormat(label.c_str(), 0, FormatOptionNoTruncate, it->c_str());
	}
	if ( ! pm.IsEmpty()) {
		pm.display(return_buf, request);
	}
}

 void AddTargetAttribsToBuffer (
	classad::References & trefs, // in, target refs (probably returned by AddReferencedAttribsToBuffer)
	ClassAd * request,
	ClassAd * target,
	bool raw_values, // unparse referenced values if true, print evaluated referenced values if false
	const char * pindent,
	std::string & return_buf)
{
	classad::References::iterator it;

	AttrListPrintMask pm;
	pm.SetAutoSep(NULL, "", "\n", "\n");
	for ( it = trefs.begin(); it != trefs.end(); it++ ) {
		std::string label;
		formatstr(label, raw_values ? "%sTARGET.%s = %%r" : "%sTARGET.%s = %%V", pindent, it->c_str());
		if (target->LookupExpr(*it)) {
			pm.registerFormat(label.c_str(), 0, FormatOptionNoTruncate, it->c_str());
		}
	}
	if (pm.IsEmpty())
		return;

	std::string temp;
	if (pm.display(temp, request, target) > 0) {
		//return_buf += "\n";
		//return_buf += pindent;
		std::string name;
		if ( ! target->LookupString(ATTR_NAME, name)) {
			int cluster=0, proc=0;
			if (target->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
				target->LookupInteger(ATTR_PROC_ID, proc);
				formatstr(name, "Job %d.%d", cluster, proc);
			} else {
				name = "Target";
			}
		}
		return_buf += name;
		return_buf += " has the following attributes:\n\n";
		return_buf += temp;
	}
}

size_t AddClassadMemoryUse (const classad::ExprList* list, QuantizingAccumulator & accum, int & num_skipped)
{
	accum += sizeof(classad::ExprList);
	classad::ExprList::const_iterator it;
	for (it = list->begin(); it != list->end(); ++it) {
		AddExprTreeMemoryUse(*it, accum, num_skipped);
	}
	return accum.Value();
}

size_t AddClassadMemoryUse (const classad::ClassAd* cad, QuantizingAccumulator & accum, int & num_skipped)
{
	accum += sizeof(classad::ClassAd);
	classad::ClassAd::const_iterator it;
	for (it = cad->begin(); it != cad->end(); ++it) {
		accum += it->first.length() * sizeof(char); // TODO: account for std::string overhead.
		AddExprTreeMemoryUse(it->second, accum, num_skipped);
	}
	return accum.Value();
}

size_t AddExprTreeMemoryUse (const classad::ExprTree* expr, QuantizingAccumulator & accum, int & num_skipped)
{
	classad::ExprTree::NodeKind kind = expr->GetKind( );

	classad::ExprTree *left=NULL, *right=NULL, *gripping=NULL;
	switch(kind) {
		case classad::ExprTree::LITERAL_NODE: {
			classad::Value val;
			classad::Value::NumberFactor factor;
			((const classad::Literal*)expr)->GetComponents(val, factor);
			accum += sizeof(classad::Literal);
			const char * s = NULL;
			classad::ExprList * lst = NULL;
			classad::ClassAd * cad = NULL;
			if (val.IsStringValue(s) && s) { accum += (strlen(s)+1) * sizeof(*s); }
			else if (val.IsListValue(lst) && lst) { AddClassadMemoryUse(lst, accum, num_skipped); }
			else if (val.IsClassAdValue(cad) && lst) { AddClassadMemoryUse(cad, accum, num_skipped); }
			break;
		}

		case classad::ExprTree::ATTRREF_NODE: {
			bool absolute;
			std::string strAttr; // simple attr, need unparse to get TARGET. or MY. prefix.
			((const classad::AttributeReference*)expr)->GetComponents(left, strAttr, absolute);
			accum += sizeof(classad::AttributeReference);
			break;
		}

		case classad::ExprTree::OP_NODE: {
			classad::Operation::OpKind op = classad::Operation::__NO_OP__;
			((const classad::Operation*)expr)->GetComponents(op, left, right, gripping);
			if (op == classad::Operation::PARENTHESES_OP) {
				accum += sizeof(classad::OperationParens);
			} else if (op == classad::Operation::TERNARY_OP) {
				accum += sizeof(classad::Operation3);
			} else if (op == classad::Operation::UNARY_PLUS_OP ||
						op == classad::Operation::UNARY_MINUS_OP ||
						op == classad::Operation::LOGICAL_NOT_OP) {
				accum += sizeof(classad::Operation1);
			} else {
				accum += sizeof(classad::Operation2);
			}
			break;
		}

		case classad::ExprTree::FN_CALL_NODE: {
			std::vector<classad::ExprTree*> args;
			std::string strLabel;
			((const classad::FunctionCall*)expr)->GetComponents(strLabel, args);
			accum += sizeof(classad::FunctionCall);
			if ( ! strLabel.empty()) {
				accum += strLabel.size() * sizeof(char);
			}
			for (size_t ii = 0; ii < args.size(); ++ii) {
				if (args[ii]) { AddExprTreeMemoryUse(args[ii], accum, num_skipped); }
			}
			break;
		}

		case classad::ExprTree::CLASSAD_NODE: {
			std::vector< std::pair<std::string, classad::ExprTree*> > attrsT;
			((const classad::ClassAd*)expr)->GetComponents(attrsT);
			accum += sizeof(classad::ClassAd);
			if (attrsT.size()) {
				std::vector< std::pair<std::string, classad::ExprTree*> >::const_iterator it;
				for (it = attrsT.begin(); it != attrsT.end(); ++it) {
					accum += it->first.length() * sizeof(char); // TODO: account for std::string overhead.
					AddExprTreeMemoryUse(it->second, accum, num_skipped);
				}
			}
			break;
		}

		case classad::ExprTree::EXPR_LIST_NODE: {
			std::vector<classad::ExprTree*> exprs;
			((const classad::ExprList*)expr)->GetComponents(exprs);
			accum += sizeof(classad::ExprList);
			if (exprs.size()) {
				std::vector<classad::ExprTree*>::const_iterator it;
				for (it = exprs.begin(); it != exprs.end(); ++it) {
					AddExprTreeMemoryUse(*it, accum, num_skipped);
				}
			}
			break;
		}
		
		case classad::ExprTree::EXPR_ENVELOPE: {
			// recurse b/c we indirect for this element.
			const classad::CachedExprEnvelope* pcenv = (const classad::CachedExprEnvelope*)expr;
			left = const_cast<classad::CachedExprEnvelope*>(pcenv)->get();
			accum += sizeof(classad::CachedExprEnvelope);
			break;
		}
	}

	if (left) AddExprTreeMemoryUse(left, accum, num_skipped);
	if (right) AddExprTreeMemoryUse(right, accum, num_skipped);
	if (gripping) AddExprTreeMemoryUse(gripping, accum, num_skipped);

	return (int)accum.Value();
}
