#ifndef _STATUS_H
#define _STATUS_H


#include "condor_constants.h"
#include "condor_xdr.h"

typedef struct {
	char	*name;
	int		run;
	int		tot;
	int		prio;
	char	*state;
	float	load_avg;
	int		kbd_idle;
	char	*arch;
	char	*op_sys;
} STATUS_LINE;

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus) /* ANSI style prototypes */
BOOLEAN xdr_status_line( XDR *, STATUS_LINE * );


#else	/* Non-ANSI style prototypes */
BOOLEAN xdr_status_line();


#endif /* non-ANSI prototypes */


#if defined(__cplusplus)
}
#endif

#endif /* _STATUS_H */
