
#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "condor_classad.h"

/* Locate the interpreter executable for the given language.  Copy the name of the executable into 'interpreter'.  Return true on success, false on failure.  */

int InterpreterFind( const char *language, char *interpreter );

/* Scan through the configuration parameters, looking for information about interpreters.  Insert this information into the given ClassAd.  Return true on success, false on failure. */

int InterpreterPublish( ClassAd *ad );

#endif
