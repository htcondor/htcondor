/*
**	This is a simple program that may be used as a skeleton for the HTCB.
**	The program creates a number of "jobs" (by creating classads for them)
**	and places them in the STAGING state.  It then randomly moves each job
**	to the job's next state, till all jobs are in the COMPLETED state.  The
**	changes to the classads are maintained in a persistent and fault tolerant
**	manner so that crashes do not affect correctness in operation.
**
**	Authors:  Rajesh Raman, Adiel Yoaz (for Condor Team)
*/

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "classad_package.h"

//-------------------------------------------------------------

// Enumeration of the different states of a job
enum TaskState {
    TASK_STAGING,
    TASK_SUBMITTING,
    TASK_IN_PROGRESS,
    TASK_CLEAN_UP,
    TASK_COMPLETE,

    _THRESHOLD_
};

//-------------------------------------------------------------

// Job state names (must correspond with the above enumeration)
static const char TaskStateStr[][16] = {
    "Staging",
    "Submitting",
    "In Progress",
    "Clean Up",
    "Complete"
};

//-------------------------------------------------------------

// internal prototypes
static bool      isPrefix( const char*, const char * );
static bool      initialize( );
static ClassAd*  createJob( char* );
static bool      getProb( int&, int, char *[], double& );
static bool      changeStates( ClassAd &, const char *, const char *, double );
static void      usage( const char* );
static bool      insertJobs( );
static void      PrintColl(ClassAdCollection*);

//-------------------------------------------------------------

// globals
ClassAdCollection   *jobCollection;              // the collection object
int                 collID;                      // the partition ID number
int                 numInitialJobs=3;            // number of jobs to generate
char                logFile[200] = "adlog.log";  // name of the log file

//-------------------------------------------------------------
// create a job class-ad
//-------------------------------------------------------------

static ClassAd*
createJob( char *key )
{
    static int lastId = -1;

    ClassAd* job=new ClassAd();

        // the second argument in these calls converted to the corresponding
        // types of classad literals 

    job->InsertAttr( "TaskState", TaskStateStr[ TASK_STAGING ] );
    job->InsertAttr( "JobID",     ++lastId );
    job->InsertAttr( "JobType",   "Batch" );
    job->InsertAttr( "Exe",       "a.out" );
    job->InsertAttr( "StdIn",     "/home/file" );
    job->InsertAttr( "Out",       "/dev/null" );
    job->InsertAttr( "InTemp",    true );

        // the second argument in these calls are string representations
        // of expressions

    job->Insert( "Args",          "{ \"-foo\", 10 }" );
    job->Insert( "InPerm",        "undefined" );
    job->Insert( "CN",            "undefined" );

        // Literal::MakeAbsTime() creates a literal that represents the
        // time at which the expression was created

    job->Insert( "StartedStaging", Literal::MakeAbsTime( ) ); 
    sprintf( key, "%d",            lastId );
    return job;
}


//-------------------------------------------------------------------
// insert job ads into the collection
//-------------------------------------------------------------------

static bool
insertJobs( )
{
    ClassAd*    job;
    char        key[10];

        // populate the collection with a bunch of jobs
    for( int i = 0 ; i < numInitialJobs ; i++ ) {
        job = createJob( key );
        jobCollection->NewClassAd( key, job );
    }   

    return( true );
}


//------------------------------------------------------------------
// this function iterates through all the job ads in a particular
// partition (e.g. for SUBMITTING state) and changes each job
// to the next state (specified by newState) with probability prob.
// Note that changing a job's state automatically moves it to another
// partition in the partition-group, because the partitions are
// partitioned based on the "state" attribute of the class-ad.
//------------------------------------------------------------------

static bool
changeStates( ClassAd &rep, const char *newState, const char *timeStampAttr, 
                double prob )
{
    int                 parID;
    char                newStateString[64];
    char                key[64];
    bool                empty = true;
    ClassAd             *updAd;
    CollContentIterator itor;
    char                *oldState=NULL;

        // make the string representation of the literal expression that
        // evaluates to the new state of the job

    sprintf( newStateString, "\"%s\"", newState );

        // find the partition id of jobs in state described by rep

    rep.EvaluateAttrString("TaskState",oldState);
    parID = jobCollection->FindPartition( collID, &rep );
    if( parID == -1 ) {
            // no jobs in this state
        return( false );
    }
    
        // initialize an iterator over jobs in the partition

    jobCollection->InitializeIterator( parID, &itor );
    while( !itor.AtEnd( ) ) {

		empty = false;
        itor.CurrentAdKey( key );
        printf( "Considering %s\n", key );

            // with probability prob, change states
        if( drand48( ) <= prob ) {
                // create update classad with new state and state change time
            updAd = new ClassAd( );
            updAd->Insert( "TaskState", newStateString );
            updAd->Insert( timeStampAttr, Literal::MakeAbsTime( ) );
            jobCollection->UpdateClassAd(key,updAd);

            printf( "Moved %s to state %s\n", key, newState );
        }

            // check if we should explicitly move the itorator, or if the
            // itorator was moved automatically (which occurs if the ad
            // under the itorator was removed from the partition due to update)

        if( !itor.IteratorMoved( ) ) {
            itor.NextAd( );
        }
    }

    return( !empty );
}

//------------------------------------------------------------------------

int 
main( int argc, char *argv[] )
{
    double  stageToSubmit, submitToInProg, inProgToCleanUp, cleanUpToFinish;
    ClassAd stageRep, submitRep, inProgRep, cleanUpRep;
    bool    someCleaningUp, someInProg, someSubmitted, someStaging;
    int     sleepTime = 1;
    int     RandSeed=0;
    bool    printColl=false;

        // set default probabilities of changing job states
    stageToSubmit = submitToInProg = inProgToCleanUp = cleanUpToFinish = 0.5;

        // process command line options
    for( int i = 1; i < argc; i++ ) {
        if( isPrefix( "-help", argv[i] ) ) {
            usage( argv[0] );
            exit( 1 );
        } else if( isPrefix( "-sleepTime", argv[i] ) ) {
            if( argc > i+1 ) {
                sleepTime = strtol( argv[i+1], (char**)NULL, 10 );
                i++;
            } else {
                fprintf( stderr, "Option -sleepTime needs an argument\n" );
                exit( 1 );
            }
        } else if( isPrefix( "-numInitialJobs", argv[i] ) ) {
            if( argc > i+1 ) {
                numInitialJobs = strtol( argv[i+1], (char**)NULL, 10 );
                i++;
            } else {
                fprintf( stderr, "Option -numInitialJobs needs an argument\n" );
                exit( 1 );
            }
        } else if( isPrefix( "-stageToSubmit", argv[i] ) ) {
            getProb( i, argc, argv, stageToSubmit );
        } else if( isPrefix( "-submitToInProg", argv[i] ) ) {
            getProb( i, argc, argv, submitToInProg );
        } else if( isPrefix( "-inProgToCleanUp", argv[i] ) ) {
            getProb( i, argc, argv, inProgToCleanUp );
        } else if( isPrefix( "-cleanUpToFinish", argv[i] ) ) {
            getProb( i, argc, argv, cleanUpToFinish );
        } else if( isPrefix( "-print", argv[i] ) ) {
            printColl=true;
        } else if( isPrefix( "-randSeed", argv[i] ) ) {
            RandSeed=atoi(argv[i+1]);
            i++;
        } else if( isPrefix( "-log", argv[i] ) ) {
            if( argc > i+1 ) {
                strcpy( logFile, argv[i+1] );
                i++;
            } else {
                fprintf( stderr, "Option -log needs an argument\n" );
                exit( 1 );
            }
        } else {
            fprintf( stderr, "Unknown option: %s\n", argv[i] );
            usage( argv[0] );
            fprintf( stderr, "Exitting\n" );
            exit( 1 );
        }
    }

        // set the random generator seed
    srand48(RandSeed);

        // Insert initial job ads into the collection
    initialize( );
    printf("Running update test\n");

        // create classads that represent each partition
    stageRep.InsertAttr  ( "TaskState", TaskStateStr[ TASK_STAGING ] );
    submitRep.InsertAttr ( "TaskState", TaskStateStr[ TASK_SUBMITTING ] );
    inProgRep.InsertAttr ( "TaskState", TaskStateStr[ TASK_IN_PROGRESS ] );
    cleanUpRep.InsertAttr( "TaskState", TaskStateStr[ TASK_CLEAN_UP ] );

        // the main loop - continue until all jobs are in COMPLETE state
    while(1) {
            // slow down ... 
        sleep( sleepTime );

            // shift states in reverse order so that no task makes more than
            // one state transition in a given time step
        someCleaningUp=changeStates( cleanUpRep, TaskStateStr[ TASK_COMPLETE ], 
            "CompletionTime", cleanUpToFinish );
        someInProg=changeStates( inProgRep, TaskStateStr[ TASK_CLEAN_UP ], 
            "MovedToCleanUp", inProgToCleanUp );
        someSubmitted=changeStates( submitRep,TaskStateStr[ TASK_IN_PROGRESS ], 
            "MovedToInProgress", submitToInProg );
        someStaging=changeStates( stageRep, TaskStateStr[ TASK_SUBMITTING ], 
            "MovedToSubmit", stageToSubmit );
    
            // check if all of them are in completed state
        if( !someCleaningUp && !someInProg && !someSubmitted && !someStaging ) {
                // yes ... done!
            break;
        }
    }

        // if asked to print the collection, display it
    if (printColl) PrintColl(jobCollection);

    return( 0 );
}

//--------------------------------------------------------------------------
// print the collection
//--------------------------------------------------------------------------

void PrintColl(ClassAdCollection* adColl) 
{
  printf("Printing collections\n");
  adColl->Print();
  printf("Printing Classads\n");
  adColl->PrintAllAds();
}

//--------------------------------------------------------------------------
// Initialize - read the log file, or create a new collection
//--------------------------------------------------------------------------

static bool
initialize( )
{
    struct stat statBuf;
    StringSet   strSet;
    bool        newLog=true;

        // check if a log file already exists
    newLog = ( stat( logFile, &statBuf ) < 0 );
    if ( !newLog) printf("Reading log file\n");
    
        // if the log file exists, the collection will recover its state
        // otherwise, we have a new empty collection (ordered by JobIDs)
    if( ( jobCollection = new ClassAdCollection( logFile , "JobID" ))==NULL ) {
        fprintf( stderr, "Error:  Unable to instantiate collection with"
            "log file %s\n", logFile );
        exit( 1 );
    }

        // create the required sub-collections; we use a single partitioned
        // collection, partitioned on the TaskState (staging, running, etc.)

    strSet.Insert( "TaskState" );
    collID = jobCollection->CreatePartition( 0, "JobID", strSet );
    if( collID < 0 ) {
        fprintf( stderr, "Failed to create partition\n" );
        return( false );
    }

        // if we have a new log, populate the collection with random jobs

    bool status = true;
    if( newLog ) {
        printf("Inserting %d jobs\n",numInitialJobs);
        status = insertJobs( );
    }

    return( status );
}


//--------------------------------------------------------------------------
// print the usage statement
//--------------------------------------------------------------------------

static void
usage( const char *name )
{
    fprintf(stderr,"Usage: %s [-help] [-stageToSubmit prb] ",name);
    fprintf( stderr,"[-submitToInProgress prb]\n\t[-inProgressToCleanUp prb]");
    fprintf( stderr,"[-cleanUpToDone prb] [-numInitialJobs num]\n\t" );
    fprintf( stderr,"[-sleepTime sec] [-randSeed seed] [-print] " );
    fprintf( stderr,"[-log filename]\n\t" );
    fprintf( stderr,"( Any prefix will suffice )\n" );
}

//--------------------------------------------------------------------------
// function to get a probability from the command line
//--------------------------------------------------------------------------

static bool
getProb( int &i, int argc, char *argv[], double &prob )
{
    if( i+1 >= argc ) {
        fprintf( stderr,"Error: Option %s needs probability value\n", argv[i] );
        usage( argv[0] );
        exit( 1 );
    }
    i++;
    prob = strtod( argv[i], (char**) NULL );
    if( prob < 0.0 ) prob = 0.0;
    if( prob > 1.0 ) prob = 1.0;
    return( true );
}


//--------------------------------------------------------------------------
// check if a s1 is a prefix of s2
//--------------------------------------------------------------------------

static bool
isPrefix( const char *s1, const char *s2 )
{
    return( strncmp( s1, s2, strlen( s2 ) ) == 0 );
}
