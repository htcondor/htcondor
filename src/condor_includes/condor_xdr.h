#ifndef _CONDOR_XDR
#define _CONDOR_XDR

#include "_condor_fix_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
  Cover up non-ANSI prototypes.
*/
#if defined(__STDC__) || defined(__cplusplus)
#define malloc hide_malloc
#define xdrrec_endofrecord  hide_xdrrec_endofrecord
#define xdrrec_skiprecord hide_xdrrec_skiprecord
#define xdr_bool hide_xdr_bool
#define xdr_char hide_xdr_char
#define xdr_double hide_xdr_double
#define xdr_enum hide_xdr_enum
#define xdr_float hide_xdr_float
#define xdr_free hide_xdr_free
#define xdr_int hide_xdr_int
#define xdr_long hide_xdr_long
#define xdr_short hide_xdr_short
#define xdr_u_char hide_xdr_u_char
#define xdr_u_int hide_xdr_u_int
#define xdr_u_long hide_xdr_u_long
#define xdr_u_short hide_xdr_u_short
#define xdr_void hide_xdr_void
#define xdr_string hide_xdr_string
#endif

#include <rpc/types.h>
#include <rpc/xdr.h>

/*
  Now uncover all the names
*/
#if defined(__STDC__) || defined(__cplusplus)
#undef malloc
#undef xdrrec_endofrecord
#undef xdrrec_skiprecord
#undef xdr_bool
#undef xdr_char
#undef xdr_double
#undef xdr_enum
#undef xdr_float
#undef xdr_free
#undef xdr_int
#undef xdr_long
#undef xdr_short
#undef xdr_u_char
#undef xdr_u_int
#undef xdr_u_long
#undef xdr_u_short
#undef xdr_void
#undef xdr_string
#endif /* __STDC__ || __cplusplus */

#if defined(OSF1)
#define mem_alloc(bsize)        malloc(bsize)
#endif



#if defined(__STDC__) || defined(__cplusplus)
XDR * xdr_Init( int *sock, XDR *xdrs );
#else
XDR * xdr_Init();
#endif

/*
  ANSI style prototypes for commonly used XDR routines.  These should be
  supplied in <rpc/xdr.h>, but generally are not.  This is by no means
  an exhaustive list of routines exported by the XDR package - Feel free
  to add more if you need 'em.
*/
#if defined(__STDC__) || defined(__cplusplus) /* ANSI Prototypes */
bool_t xdrrec_endofrecord(XDR *, int);
bool_t xdrrec_skiprecord(XDR *);
bool_t xdr_bool(XDR *, bool_t *);
bool_t xdr_char(XDR *, char *);
bool_t xdr_double(XDR *, double *);
bool_t xdr_enum(XDR *, enum_t *);
bool_t xdr_float(XDR *, float *);
void xdr_free(XDR *, char *);
bool_t xdr_int(XDR *, int *);
bool_t xdr_long(XDR *, long *);
bool_t xdr_short(XDR *, short *);
bool_t xdr_u_char(XDR *, unsigned char *);
bool_t xdr_u_int(XDR *, unsigned int *);
bool_t xdr_u_long(XDR *, unsigned long *);
bool_t xdr_u_short(XDR *, unsigned short *);
bool_t xdr_void();
bool_t xdr_string(XDR*, char **, unsigned int);
#else	/* ANSI Prototypes */
bool_t xdrrec_endofrecord();
bool_t xdrrec_skiprecord();
bool_t xdr_bool();
bool_t xdr_char();
bool_t xdr_double();
bool_t xdr_enum();
bool_t xdr_float();
void xdr_free();
bool_t xdr_int();
bool_t xdr_long();
bool_t xdr_short();
bool_t xdr_u_char();
bool_t xdr_u_int();
bool_t xdr_u_long();
bool_t xdr_u_short();
bool_t xdr_void();
bool_t xdr_string();
#endif


/*
	OSF1's cxx stops at non-std prototype declarations.
	xdr_destroy is a macro to a non-std defined routine.
	This makes things work transparently.
*/

#if defined(OSF1) && ( defined(__STDC__) || defined(__cplusplus) )
#undef  xdr_destroy
#define xdr_destroy				my_xdr_destroy
#undef  XDR_DESTROY
#define XDR_DESTROY				my_xdr_destroy
extern int my_xdr_destroy(XDR *);
#endif



#if defined(__cplusplus)
}
#endif


#endif /* _CONDOR_XDR */
