/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AVIARY_CONVERSION_MACROS_H
#define AVIARY_CONVERSION_MACROS_H

#define MGMT_DECLARATIONS	\
ExprTree *expr;				\
int num;					\
float flt;					\
char *str;					\
(void)expr;(void)num;(void)flt;(void)str;


#define BASE(attr,type,lookup_var,set_var,extra)					\
if (ad.Lookup##type(#attr, lookup_var)) {							\
	m_stats.attr = ((set_var) extra);						\
} else {															\
	dprintf(D_FULLDEBUG, "Warning: Could not find " #attr "\n");		\
}

#define OPT_BASE(attr,type,lookup_var,set_var,extra)				\
if (ad.Lookup##type(#attr, lookup_var)) {							\
	m_stats.attr = ((set_var) extra);						\
} else {															\
	m_stats.attr = "";										\
}

#define STRING(attr)												\
if (ad.LookupString(#attr, &str)) {									\
	m_stats.attr = str;									\
	free(str);														\
} else {															\
	dprintf(D_FULLDEBUG, "Warning: Could not find " #attr "\n");		\
}

#define OPT_STRING(attr)												\
if (ad.LookupString(#attr, &str)) {									\
	m_stats.attr = str;									\
	free(str);														\
} else {															\
	m_stats.attr = "";										\
}

#define INTEGER(attr) BASE(attr,Integer,num,(uint32_t) num,)
#define OPT_INTEGER(attr) OPT_BASE(attr,Integer,num,(uint32_t) num,)
#define DOUBLE(attr) BASE(attr,Float,flt,(double) flt,)
#define OPT_DOUBLE(attr) OPT_BASE(attr,Float,flt,(double) flt,)
#define TIME_INTEGER(attr) BASE(attr,Integer,num,(uint64_t) num,* 1000000000)
#define OPT_TIME_INTEGER(attr) OPT_BASE(attr,Integer,num,(uint64_t) num,* 1000000000)

#define EXPR(attr)													\
	EXPR_BASE(attr,													\
			  dprintf(D_FULLDEBUG, "Warning: " #attr " not found\n"))

#define OPT_EXPR(attr)											 	\
	EXPR_BASE(attr,													\
			  m_stats.attr = "")

#define EXPR_BASE(attr,else_action)									\
expr = ad.Lookup(#attr);											\
if (expr) {                                                         \
      str = const_cast<char*>(ExprTreeToString(expr));              \
      m_stats.attr = str;                             \
} else {                                                         \
      dprintf(D_FULLDEBUG, "Warning: " #attr " has no value\n");    	\
}

#endif /* AVIARY_CONVERSION_MACROS_H */
