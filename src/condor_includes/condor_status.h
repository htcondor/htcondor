#ifndef _STATUS_H
#define _STATUS_H


#include "condor_constants.h"

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


#endif /* _STATUS_H */
