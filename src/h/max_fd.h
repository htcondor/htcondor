#ifndef CONDOR_MAX_FD
#define CONDOR_MAX_FD

#if defined(__cplusplus)
extern "C" int max_fd(void);
#else 
extern int max_fd(void);
#endif

#endif /* CONDOR_MAX_FD */
