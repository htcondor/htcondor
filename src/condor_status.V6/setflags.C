#include "condor_api.h"
#include "status_types.h"
#include "totals.h"

extern AdTypes	type;
extern Mode		mode;
extern ppOption	ppStyle;

int matchPrefix (const char *, const char *);

char *
getPPStyleStr ()
{
	switch (ppStyle)
	{
    	case PP_NOTSET:			return "<Not set>";
    	case PP_STARTD_NORMAL:	return "Normal (Startd)";
    	case PP_SCHEDD_NORMAL:	return "Normal (Schedd)";
    	case PP_MASTER_NORMAL:	return "Normal (Master)";
    	case PP_CKPT_SRVR_NORMAL:return"Normal (CkptSrvr)";
    	case PP_STARTD_SERVER:	return "Server";
    	case PP_STARTD_RUN:		return "Run";
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
        if (matchPrefix (dtype, "STARTD")) {
            type = STARTD_AD;
        } else
        if (matchPrefix (dtype, "SCHEDD")) {
            type = SCHEDD_AD;
        } else
        if (matchPrefix (dtype, "MASTER")) {
            type = MASTER_AD;
        } else
        if (matchPrefix (dtype, "CKPT_SRVR")) {
            type = CKPT_SRVR_AD;
        } else
        if (matchPrefix (dtype, "GATEWAYS")) {
            type = GATEWAY_AD;
        } else {
            fprintf (stderr, "Error:  Unknown entity type: %s\n", dtype);
            exit (1);
        }
        setBy = i;
        setArg = argv;
    } else {
        fprintf (stderr,
        "Error:  Daemon type implied by arg %d (%s) contradicts arg %d (%s)\n",
        	i, argv, setBy, setArg);
    }
}

char *
getModeStr()
{
	switch (mode)
	{
		case MODE_NOTSET:				return "Not set";
		case MODE_STARTD_NORMAL:		return "Normal (Startd)";
		case MODE_STARTD_AVAIL:			return "Available (Startd)";
		case MODE_STARTD_RUN:			return "Run (Startd)";
		case MODE_SCHEDD_NORMAL:		return "Normal (Schedd)";
		case MODE_MASTER_NORMAL:		return "Normal (Master)";
		case MODE_CKPT_SRVR_NORMAL:		return "Normal (CkptSrvr)";
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
          case MODE_STARTD_NORMAL:
            setType ("STARTD", i, argv);
            setPPstyle (PP_STARTD_NORMAL, i, argv);
            break;

          case MODE_STARTD_AVAIL:
            setType ("STARTD", i, argv);
            setPPstyle (PP_STARTD_NORMAL, i, argv);
            break;

          case MODE_STARTD_RUN:
            setType ("STARTD", i, argv);
            setPPstyle (PP_STARTD_RUN, i, argv);
            break;

		  case MODE_SCHEDD_NORMAL:
            setType ("SCHEDD", i, argv);
            setPPstyle (PP_SCHEDD_NORMAL, i, argv);
            break;

		  case MODE_SCHEDD_SUBMITTORS:
            setType ("SCHEDD", i, argv);
            setPPstyle (PP_SCHEDD_SUBMITTORS, i, argv);
            break;

		  case MODE_MASTER_NORMAL:
			setType ("MASTER", i, argv);
			setPPstyle (PP_MASTER_NORMAL, i, argv);
			break;

		  case MODE_CKPT_SRVR_NORMAL:
			setType ("CKPT_SRVR", i, argv);
			setPPstyle (PP_CKPT_SRVR_NORMAL, i, argv);
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

