#include "condor_api.h"
#include "status_types.h"
#include "totals.h"

extern AdTypes	type;
extern Mode		mode;
extern ppOption	ppStyle;
extern int 		summarySize;


char *
getPPStyleStr ()
{
	switch (ppStyle)
	{
    	case PP_NOTSET:			return "<Not set>";
    	case PP_NORMAL:			return "Normal";
    	case PP_SERVER:			return "Server";
    	case PP_AVAIL:			return "Avail";
    	case PP_RUN:			return "Run";
    	case PP_SUBMITTORS:		return "Submittors";
    	case PP_VERBOSE:		return "Verbose";
    	case PP_CUSTOM:			return "Custom";
		default:				return "<Unknown!>";
	}
	// should not reach here
	exit (1);
}


void
setPPstyle (ppOption pps, int i, char *argv)
{
    static int setBy = 0;
    static char *setArg = NULL;

	if (argv == NULL) {
		printf ("Set by arg %d (%-10s), PrettyPrint style = %s\n", 
				setBy, setArg, getPPStyleStr());
		return;
	}
		
	// if the style has already been set, and is trying to be reset by default
	// rules, override the default  (i.e., don't make a change)
	if (setBy != 0 && i == 0)
		return;

    if (ppStyle <= pps || setBy == 0) {
        ppStyle = pps;
        setBy = i;
        setArg = argv;
    } else {
        fprintf (stderr, "Error:  arg %d (%s) contradicts arg %d (%s)\n",
                            i, argv, setBy, setArg);
        exit (1);
    }
}


char *
getTypeStr ()
{
	switch (type)
	{
		case STARTD_AD:		return "STARTD";
		case SCHEDD_AD:		return "SCHEDD";
		case MASTER_AD:		return "MASTER";
		case CKPT_SRVR_AD:	return "CKPT_SRVR";
		case GATEWAY_AD:	return "GATEWAYS";
		default: 			return "<Unknown type!>";
	}
	// should never get here
	exit (1);
}


void
setType (char *dtype, int i, char *argv)
{
    static int setBy = 0;
    static char *setArg = NULL;

	if (argv == NULL) {
		printf ("Set by arg %d (%-10s), Query type = %s\n",
				setBy, setArg, getTypeStr());
		return;
	}
		 
	// if the type has already been set, and is trying to be reset by default
	// rules, override the default  (i.e., don't make a change)
	if (setBy != 0 && i == 0) 
		return;

    if (setArg == NULL) {
        if (strcmp (dtype, "STARTD") == 0) {
            type = STARTD_AD;
        } else
        if (strcmp (dtype, "SCHEDD") == 0) {
            type = SCHEDD_AD;
        } else
        if (strcmp (dtype, "MASTER") == 0) {
            type = MASTER_AD;
        } else
        if (strcmp (dtype, "CKPT_SRVR") == 0) {
            type = CKPT_SRVR_AD;
        } else
        if (strcmp (dtype, "GATEWAYS") == 0) {
            type = GATEWAY_AD;
        } else {
            fprintf (stderr, "Error:  Unknown entity type: %d\n", dtype);
            exit (1);
        }
        setBy = i;
        setArg = argv;
    } else {
        fprintf (stderr,
        "Error:  Daemon type implied by arg %d (%s) contradicts arg %d (%s)\n",
        i, setBy, setArg);
    }
}

char *
getModeStr()
{
	switch (mode)
	{
		case MODE_NOTSET:		return "Not set";
		case MODE_NORMAL:		return "Normal";
		case MODE_AVAIL:		return "Available";
		case MODE_SUBMITTORS:	return "Submittors";
		case MODE_RUN:			return "Run";
		case MODE_CUSTOM:		return "Custom";
		default:				return "<Unknown!>";
	}
	// should never get here
	exit (1);
}


void
setMode (Mode mod, int i, char *argv)
{
    static int setBy = 0;
    static char *setArg = NULL;

	if (argv == NULL) {
		printf("Set by arg %d (%-10s), Mode = %s\n",setBy,setArg,getModeStr());
		return;
	}

    if (setBy == 0) {
        mode = mod;
        switch (mod) {
          case MODE_SUBMITTORS:
            setType ("SCHEDD", i, argv);
            setPPstyle (PP_SUBMITTORS, i, argv);
			summarySize = sizeof(SubmittorsTotals);
            break;

          case MODE_NORMAL:
            setType ("STARTD", i, argv);
            setPPstyle (PP_NORMAL, i, argv);
			summarySize = sizeof(NormalTotals);
            break;

          case MODE_AVAIL:
            setType ("STARTD", i, argv);
            setPPstyle (PP_AVAIL, i, argv);
			summarySize = sizeof(AvailTotals);
            break;

          case MODE_RUN:
            setType ("STARTD", i, argv);
            setPPstyle (PP_RUN, i, argv);
			summarySize = sizeof(RunTotals);
            break;

          case MODE_CUSTOM:
			summarySize = 0;
			setPPstyle (PP_VERBOSE, i, argv);
            break;

          default:
            fprintf (stderr, "Error:  Illegal mode %d\n", mode);
            break;
        }
        setBy = i;
        setArg = argv;
    } else {
        fprintf (stderr, "Error:  Arg %d (%s) contradicts arg %d (%s)\n",
                            i, argv, setBy, setArg);
        exit (1);
    }
}

