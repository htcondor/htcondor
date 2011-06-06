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

#ifndef MGMT_CONVERSION_MACROS_H
#define MGMT_CONVERSION_MACROS_H

#define MGMT_DECLARATIONS	\
ExprTree *expr;				\
int num;					\
float flt;					\
char *str;					\
(void)expr;(void)num;(void)flt;(void)str;


#define BASE(attr,type,lookup_var,set_var,extra)					\
if (ad.Lookup##type(#attr, lookup_var)) {							\
	mgmtObject->set_##attr((set_var) extra);						\
} else {															\
	dprintf(D_FULLDEBUG, "Warning: Could not find " #attr "\n");		\
}

#define BASE2(src,dest,type,lookup_var,set_var,extra)				\
if (ad.Lookup##type(#src, lookup_var)) {							\
	mgmtObject->set_##dest((set_var) extra);						\
} else {															\
	dprintf(D_FULLDEBUG, "Warning: Could not find " #src "\n");			\
}

#define OPT_BASE(attr,type,lookup_var,set_var,extra)				\
if (ad.Lookup##type(#attr, lookup_var)) {							\
	mgmtObject->set_##attr((set_var) extra);						\
} else {															\
	mgmtObject->clr_##attr();										\
}

#define STRING(attr)												\
if (ad.LookupString(#attr, &str)) {									\
	mgmtObject->set_##attr(str);									\
	free(str);														\
} else {															\
	dprintf(D_FULLDEBUG, "Warning: Could not find " #attr "\n");		\
}

#define STRING2(src,dest)											\
if (ad.LookupString(#src, &str)) {									\
	mgmtObject->set_##dest(str);									\
	free(str);														\
} else {															\
	dprintf(D_FULLDEBUG, "Warning: Could not find " #src "\n");			\
}

#define OPT_STRING(attr)												\
if (ad.LookupString(#attr, &str)) {									\
	mgmtObject->set_##attr(str);									\
	free(str);														\
} else {															\
	mgmtObject->clr_##attr();										\
}

#define INTEGER(attr) BASE(attr,Integer,num,(uint32_t) num,)
#define INTEGER64(attr) BASE(attr,Float,flt,(uint64_t) flt,)
#define INTEGER2(src,dest) BASE2(src,dest,Integer,num,(uint32_t) num,)
#define OPT_INTEGER(attr) OPT_BASE(attr,Integer,num,(uint32_t) num,)
#define DOUBLE(attr) BASE(attr,Float,flt,(double) flt,)
#define DOUBLE2(src,dest) BASE2(src,dest,Float,flt,(double) flt,)
#define OPT_DOUBLE(attr) OPT_BASE(attr,Float,flt,(double) flt,)
#define TIME_INTEGER(attr) BASE(attr,Integer,num,(uint64_t) num,* 1000000000)
#define TIME_INTEGER2(src,dest) BASE2(src,dest,Integer,num,(uint64_t) num,* 1000000000)
#define OPT_TIME_INTEGER(attr) OPT_BASE(attr,Integer,num,(uint64_t) num,* 1000000000)

#define EXPR(attr)													\
	EXPR_BASE(attr,													\
			  dprintf(D_FULLDEBUG, "Warning: " #attr " not found\n"))

#define OPT_EXPR(attr)											 	\
	EXPR_BASE(attr,													\
			  mgmtObject->clr_##attr())

#define EXPR_BASE(attr,else_action)									\
expr = ad.Lookup(#attr);											\
if (expr) {                                                         \
      str = const_cast<char*>(ExprTreeToString(expr));              \
      mgmtObject->set_##attr(str);                             \
} else {                                                         \
      dprintf(D_FULLDEBUG, "Warning: " #attr " has no value\n");    	\
}

#endif /* MGMT_CONVERSION_MACROS_H */
