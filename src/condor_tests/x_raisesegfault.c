#include <signal.h>
#include <unistd.h>

int  main(void) {raise(SIGSEGV); sleep(5); return(0); }
