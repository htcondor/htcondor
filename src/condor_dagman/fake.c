
#include "user_log.c++.h"

// Required for linking with condor libs
extern "C" int SetSyscalls() { return 0; }

extern "C"
{
    long random();
    int srandom(unsigned seed);
}

int WriteEvent(char *filename, ULogEvent *e)
{
    int retval;
    UserLog log("tannenba", filename, e->cluster, e->proc, e->subproc);
    retval = log.writeEvent(e);
    return retval;
}

int
Random(int range)
{
    int retval;
    retval = random() % range;
    //printf("Random(%d) = %d\n", range, retval);
    return retval;
}

int main(int argc, char **argv)
{
    SubmitEvent sub;
    ExecuteEvent exec;
    JobTerminatedEvent term;
    JobImageSizeEvent img;
    int cluster, proc, subproc;
    char *username;
    char *logfile;
    char *submitHost;
    
    //
    // This system call tells the kernel that this process is not
    // interested in the exit status of its children. That way the
    // children don't become zombie processes when they exit.
    //
    signal(SIGCLD, SIG_IGN);

    cluster = getpid();
    proc = 0;
    subproc = 0;
    username = "tannenba";
    logfile = "condor.log";
    submitHost = "sol33";

    sub.cluster = cluster;
    sub.proc = 0;
    sub.subproc = 0;
    strcpy(sub.submitHost, submitHost);
    
    exec.cluster = cluster;
    exec.proc = 0;
    exec.subproc = 0;
    strcpy(exec.executeHost, submitHost);
    
    img.cluster = cluster;
    img.proc = 0;
    img.subproc = 0;
    img.size = 1000;
    
    term.cluster = cluster;
    term.proc = proc;
    term.subproc = subproc;
    term.normal = true;
    term.returnValue = 0;

    WriteEvent(logfile, &sub);
    
    printf("Proc %d.%d:\n", cluster, proc);
    printf("    junk\n");
    printf("    junk\n");
    printf("    junk\n");
    
    int child_pid;
    if ((child_pid = fork()) < 0)
    {
	fprintf(stderr, "ERROR: could not fork child process.\n");
	exit(1);
    }
    else if (!child_pid)
    {
	//
	// This is the child process
	//
	long SLEEP = 2;
	sleep(Random(SLEEP));
	WriteEvent(logfile, &exec);
	sleep(Random(SLEEP));
	WriteEvent(logfile, &img);
	sleep(Random(SLEEP));
	WriteEvent(logfile, &term);
    }

    //fprintf(stderr, "Process %ld exiting\n", getpid());
    return 0;

}

