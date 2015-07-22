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

#ifndef __EXPR_ANALYZE_H__
#define __EXPR_ANALYZE_H__


enum {
	detail_analyze_each_sub_expr = 0x01, // analyze each sub expression, not just the top level ones.
	detail_smart_unparse_expr    = 0x02, // show unparsed expressions at analyze step when needed
	detail_always_unparse_expr   = 0x04, // always show unparsed expressions at each analyze step
	detail_always_analyze_req    = 0x10, // always analyze requirements, (better analize does this)
	detail_dont_show_job_attrs   = 0x20, // suppress display of job attributes in verbose analyze
	detail_show_all_subexprs     = 0x40, // show all sub expressions, with prefixes for pruned subs
	detail_better                = 0x80, // -better rather than  -analyze was specified.
	detail_diagnostic            = 0x100,// show steps of expression analysis to stdout
	detail_dump_intermediates    = 0x200,// dump intermediate work buffers to stdout.
};

typedef struct {
	int console_width;
	int detail_mask; // one or more of above detail_* flags
	const char * expr_label;
	const char * request_type_name;
	const char * target_type_name;
} anaFormattingOptions;

void AnalyzeRequirementsForEachTarget(
	ClassAd *request,
	const char * attrConstraint, // must be an attribute in the request ad
	ClassAdList & targets,
	std::string & return_buf,
	const anaFormattingOptions & fmt);

const char * PrettyPrintExprTree(classad::ExprTree *tree, std::string & temp_buffer, int indent, int width);

class AnalSubExpr;
int AnalyzeThisSubExpr(
	ClassAd *myad,
	classad::ExprTree* expr,
	std::vector<AnalSubExpr> & clauses,
	bool must_store,
	int depth,
	const anaFormattingOptions & fmt);

void AddReferencedAttribsToBuffer(
	ClassAd * request,
	const char * expr_string, // expression string or attribute name
	StringList & trefs, // out, returns target refs
	const char * pindent,
	std::string & return_buf);

void AddTargetAttribsToBuffer(
	StringList & trefs, // in, target refs (probably returned by AddReferencedAttribsToBuffer)
	ClassAd * request,
	ClassAd * target,
	const char * pindent,
	std::string & return_buf);


#endif//__EXPR_ANALYZE_H__
