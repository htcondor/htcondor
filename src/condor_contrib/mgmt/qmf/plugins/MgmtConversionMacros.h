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

#define OPT_STRING(attr)												\
if (ad.LookupString(#attr, &str)) {									\
	mgmtObject->set_##attr(str);									\
	free(str);														\
} else {															\
	mgmtObject->clr_##attr();										\
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
