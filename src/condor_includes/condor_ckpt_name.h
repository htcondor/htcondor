#ifndef _CKPT_NAME
#define _CKPT_NAME

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || (__cplusplus)
char *gen_ckpt_name ( const char *dir, int cluster, int proc, int subproc );
#else
char *gen_ckpt_name();
#endif

#if defined(__cplusplus)
}
#endif

#endif /* _CKPT_NAME */
