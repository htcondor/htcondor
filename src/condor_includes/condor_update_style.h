#ifndef CONDOR_UPDATE_STYLE_H
#define CONDOR_UPDATE_STYLE_H

/* Some events (like a job termination event) can have multiple "modes"
	which dictate how the event is supposed to be handled. */

enum update_style_t {
	US_NORMAL,				/* handle the event via default means */
	US_TERMINATE_PENDING	/* This represents a termination event that happend
								but yet hasn't been recorded by the logfiles,
								etc, but has been recorded by the job queue. */	
};

#endif
