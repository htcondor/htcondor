#include <setjmp.h>

#ifdef VAX
#include <frame.h>
#define _CJBLEN	4
typedef long condor_jmp_buf[_CJBLEN];
#else VAX
#if defined(MC68020) && defined(BSD43)
#define condor_jmp_buf			jmp_buf
#define condor_setjmp(cenv)		_setjmp(cenv)
#define condor_longjmp(cenv)	_longjmp((cenv), 1)
#else defined(MC68020) && defined(BSD43)
#define condor_jmp_buf			jmp_buf
#define condor_setjmp(cenv)		setjmp(cenv)
#define condor_longjmp(cenv)	longjmp((cenv), 1)
#endif defined(MC68020) && defined(BSD43)
#endif
