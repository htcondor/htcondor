#ifndef _CONDOR_ANNEX_SETUP_H
#define _CONDOR_ANNEX_SETUP_H

int setup(	const char * region, const char * pukf, const char * prkf,
			const char * cloudFormationURL, const char * serviceURL );
int check_setup( const char * cloudFormationURL, const char * serviceURL );

#endif /* _CONDOR_ANNEX_SETUP_H */
