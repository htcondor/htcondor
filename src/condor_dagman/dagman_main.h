#ifndef DAGMAN_MAIN_H
#define DAGMAN_MAIN_H

#include "dag.h"

class Global {
  public:
    inline Global ():
        dag          (NULL),
        maxJobs      (0),
        maxPreScripts (0),
        maxPostScripts (0),
        rescue_file  (NULL),
		paused (false),
        datafile     (NULL) {}
    inline void CleanUp () { delete dag; }
    Dag * dag;
    int maxJobs;  // Maximum number of Jobs to run at once
    int maxPreScripts;  // max. number of PRE scripts to run at once
    int maxPostScripts;  // max. number of POST scripts to run at once
    char *rescue_file;
	bool paused;
    char *datafile;
	StringList condorLogFiles;
};

extern Global G;

#endif	// ifndef DAGMAN_MAIN_H
