#ifndef _CONDOR_SIG_INSTALL_H
#define _CONDOR_SIG_INSTALL_H

#include "condor_fix_signal.h"

#ifdef __cplusplus 
extern "C" {
#endif

typedef void (*SIG_HANDLER)();

void install_sig_handler( int sig, SIG_HANDLER handler );
void install_sig_handler_with_mask( int sig, sigset_t* set, SIG_HANDLER handler );
void block_signal( int sig );
void unblock_signal( int sig );

#ifdef __cplusplus 
}
#endif

#endif _CONDOR_SIG_INSTALL_H
