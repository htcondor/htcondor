#ifndef GTODC
#define GTODC

/* This structure is used in the caching of gettimeofday() information between
	the remote machine and the local machine so the user job doesn't have to
	make a network round trip for each call. This is to help performance
	for applications which call gettimeofday() hundreds of times a second
	for statistics reasons.

	When we resume from a checkpoint, we take care to mark this
	cache as unintialized and it will be lazily reinitialized in a
	subsequent gettineofday() call.
*/

typedef struct GTOD_Cache_s
{
	int initialized;
	struct timeval exe_machine;
	struct timeval sub_machine;
} GTOD_Cache;

BEGIN_C_DECLS 

/* initialize the cache to sentinal values */
void _condor_gtodc_init(GTOD_Cache *gtodc);

/* is the cache set up yet? */
int _condor_gtodc_is_initialized(GTOD_Cache *gtodc);

/* you can set the cache to true or false for initialized */
void _condor_gtodc_set_initialized(GTOD_Cache *gtodc, int truth);

/* record "now" as defined by both machines. A little bit of time is
	lost to network traffic, but only the first time. */
void _condor_gtodc_set_now(GTOD_Cache *gtodc, struct timeval *exe_machine,
				struct timeval *sub_machine);

/* Get the local time, then compute the difference since the last
	local time added to the remote time in the cache. This allows
	the absolute time to be gotten from the submission machine, but
	the passing of time to be recorded from the execute machine */
void _condor_gtodc_synchronize(GTOD_Cache *gtodc, 
	struct timeval *exe_machine_cooked, struct timeval *exe_machine_raw);

/* we want access to the cache object from elsewhere */
extern GTOD_Cache *_condor_global_gtodc;

END_C_DECLS 

#endif
