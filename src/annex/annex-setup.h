#ifndef _CONDOR_ANNEX_SETUP_H
#define _CONDOR_ANNEX_SETUP_H

int check_setup();
int setup(	const char * pukf, const char * prkf,
			const char * cloudFormationURL, const char * serviceURL );

#endif /* _CONDOR_ANNEX_SETUP_H */
