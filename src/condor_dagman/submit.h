#ifndef CONDOR_SUBMIT_H
#define CONDOR_SUBMIT_H

#include "types.h"

/** Submits a job to condor using popen().  This is a very primitive method
    to submitting a job, and SHOULD be replacable by a Condor Submit API.

    In the mean time, this function executes the condor_submit command
    via popen() and parses the output, sniffing for the CondorID assigned
    to the submitted job.

    Parsing the condor_submit output successfully depends on the current
    version of Condor, and how it's condor_submit outputs results on the
    command line.
    
    @error contain a description of the error that occurred
    @param condorID will hold the ID for the submitted job (if successful)
    @return true on success, false on failure
*/

bool submit_submit (const char * cmdFile, CondorID & condorID);

#endif /* #ifndef CONDOR_SUBMIT_H */
