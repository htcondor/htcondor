/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "condor_api.h"
#include "condor_adtypes.h"
#include "status_types.h"
#include "totals.h"

extern AdTypes	type;
extern Mode		mode;
extern ppOption	ppStyle;

const char *
getPPStyleStr ()
{
	switch (ppStyle)
	{
    	case PP_NOTSET:			return "<Not set>";
		case PP_GENERIC:		return "Generic";
    	case PP_STARTD_NORMAL:	return "Normal (Startd)";
    	case PP_SCHEDD_NORMAL:	return "Normal (Schedd)";
    	case PP_SCHEDD_SUBMITTORS:return "Normal (Schedd)";
    	case PP_MASTER_NORMAL:	return "Normal (Master)";
    	case PP_CKPT_SRVR_NORMAL:return"Normal (CkptSrvr)";
		case PP_COLLECTOR_NORMAL:return"Normal (Collector)";
	    case PP_NEGOTIATOR_NORMAL: return "Normal (Negotiator)";
    	case PP_GRID_NORMAL:    return "Grid";
        case PP_STARTD_SERVER:	return "Server";
    	case PP_STARTD_RUN:		return "Run";
    	case PP_STARTD_COD:		return "COD";
		case PP_STARTD_STATE:	return "State";
		case PP_STORAGE_NORMAL:	return "Storage";
		case PP_GENERIC_NORMAL:	return "Generic";
		case PP_ANY_NORMAL:		return "Any";
    	case PP_VERBOSE:		return "Verbose";
    	case PP_XML:		    return "XML";
    	case PP_CUSTOM:			return "Custom";
        default:				return "<Unknown!>";
	}
	// should not reach here
	exit (1);
}


void
setPPstyle (ppOption pps, int i, const char *argv)
{
    static int setBy = 0;
    static const char *setArg = NULL;

	if (argv == NULL) {
		printf ("Set by arg %d (%-10s), PrettyPrint style = %s\n",
				setBy, setArg, getPPStyleStr());
		return;
	}
		
	// if the style has already been set, and is trying to be reset by default
	// rules, override the default  (i.e., don't make a change)
	if (setBy != 0 && i == 0)
		return;

	// If -long or -xml or -format are specified, do not reset to
	// "normal" style when followed by a flag such as -startd.
	if( ppStyle == PP_XML || ppStyle == PP_VERBOSE || ppStyle == PP_CUSTOM )
	{
		if( pps != PP_XML && pps != PP_VERBOSE && pps != PP_CUSTOM ) {
				// ignore this style setting and keep our existing setting
			return;
		}
	}

    if ( (PP_XML == pps) || PP_VERBOSE == pps || (ppStyle <= pps || setBy == 0) ) {
        ppStyle = pps;
        setBy = i;
        setArg = argv;
    } else {
        fprintf (stderr, "Error:  arg %d (%s) contradicts arg %d (%s)\n",
                            i, argv, setBy, setArg);
        exit (1);
    }
}


const char *
getTypeStr ()
{
	switch (type)
	{
		case DEFRAG_AD:		return "DEFRAG";
		case STARTD_AD:		return "STARTD";
		case SCHEDD_AD:		return "SCHEDD";
		case SUBMITTOR_AD:	return "SUBMITTOR";
		case MASTER_AD:		return "MASTER";
		case CKPT_SRVR_AD:	return "CKPT_SRVR";
		case GATEWAY_AD:	return "GATEWAYS";
		case COLLECTOR_AD:	return "COLLECTOR";
	    case NEGOTIATOR_AD: return "NEGOTIATOR";
		case GRID_AD:       return "GRID";
		case LICENSE_AD:	return "LICENSE";
		case STORAGE_AD:	return "STORAGE";
		case ANY_AD:		return "ANY";
		case GENERIC_AD:	return "GENERIC";
        default: 			return "<Unknown type!>";
	}
	// should never get here
	exit (1);
}


void
setType (const char *dtype, int i, const char *argv)
{
    static int setBy = 0;
    static const char *setArg = NULL;

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
        if (strcmp (dtype, "LICENSE") == 0) {
            type = LICENSE_AD;
        } else

#ifdef HAVE_EXT_POSTGRESQL
        if (strcmp (dtype, "QUILL") == 0) {
            type = QUILL_AD;
        } else
#endif /* HAVE_EXT_POSTGRESQL */

        if (strcmp (dtype, "DEFRAG") == 0) {
            type = DEFRAG_AD;
        } else
        if (strcmp (dtype, "SCHEDD") == 0) {
            type = SCHEDD_AD;
        } else
		if (strcmp (dtype, "SUBMITTOR") == 0) {
			type = SUBMITTOR_AD;
		} else
        if (strcmp (dtype, "MASTER") == 0) {
            type = MASTER_AD;
        } else
        if (strcmp (dtype, "CKPT_SRVR") == 0) {
            type = CKPT_SRVR_AD;
        } else
        if (strcmp (dtype, "COLLECTOR") == 0) {
            type = COLLECTOR_AD;
		} else
        if (strcmp (dtype, "NEGOTIATOR") == 0) {
            type = NEGOTIATOR_AD;
        } else
        if (strcmp (dtype, "GATEWAYS") == 0) {
            type = GATEWAY_AD;
        } else
		if (strcmp(dtype, "GRID") == 0) {
			type = GRID_AD;
		} else
	    if (strcmp (dtype, "STORAGE") == 0) {
            type = STORAGE_AD;
        } else
        if (strcmp(dtype, "GENERIC") == 0) {
	        type = GENERIC_AD;
        } else
        if (strcmp(dtype, "ANY") == 0) {
	        type = ANY_AD;
        } else
        if (strcmp(dtype, "HAD") == 0) {
	        type = HAD_AD;
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

const char *
getModeStr()
{
	switch (mode)
	{
		case MODE_NOTSET:		return "Not set";
		case MODE_DEFRAG_NORMAL:	return "Normal (Defrag)";
		case MODE_STARTD_NORMAL:	return "Normal (Startd)";
		case MODE_STARTD_AVAIL:		return "Available (Startd)";
		case MODE_STARTD_RUN:		return "Run (Startd)";
		case MODE_STARTD_COD:		return "COD (Startd)";
#ifdef HAVE_EXT_POSTGRESQL
		case MODE_QUILL_NORMAL:		return "Normal (Quill)";
#endif /* HAVE_EXT_POSTGRESQL */

		case MODE_SCHEDD_NORMAL:	return "Normal (Schedd)";
		case MODE_SCHEDD_SUBMITTORS:	return "Submittors (Schedd)";
		case MODE_MASTER_NORMAL:	return "Normal (Master)";
		case MODE_CKPT_SRVR_NORMAL:	return "Normal (CkptSrvr)";
		case MODE_COLLECTOR_NORMAL:	return "Normal (Collector)";
		case MODE_NEGOTIATOR_NORMAL:	return "Normal (Negotiator)";
		case MODE_GRID_NORMAL:          return "Normal (Grid)";
		case MODE_STORAGE_NORMAL:	return "Normal (Storage)";
		case MODE_GENERIC_NORMAL:	return "Normal (Generic)";
		case MODE_OTHER:		return "Generic";
		case MODE_ANY_NORMAL:		return "Normal (Any)";
		default:			return "<Unknown!>";
	}
	// should never get here
	exit (1);
}


void
setMode (Mode mod, int i, const char *argv)
{
    static int setBy = 0;
    static const char *setArg = NULL;

	if (argv == NULL) {
		printf("Set by arg %d (%-10s), Mode = %s\n",setBy,setArg,getModeStr());
		return;
	}

    if (setBy == 0) {
        mode = mod;
        switch (mod) {
          case MODE_DEFRAG_NORMAL:
            setType ("DEFRAG", i, argv);
            setPPstyle (PP_GENERIC_NORMAL, i, argv);
            break;

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

          case MODE_STARTD_COD:
            setType ("STARTD", i, argv);
            setPPstyle (PP_STARTD_COD, i, argv);
            break;

		  case MODE_SCHEDD_NORMAL:
            setType ("SCHEDD", i, argv);
            setPPstyle (PP_SCHEDD_NORMAL, i, argv);
            break;

#ifdef HAVE_EXT_POSTGRESQL
		  case MODE_QUILL_NORMAL:
            setType ("QUILL", i, argv);
            setPPstyle (PP_QUILL_NORMAL, i, argv);
            break;
#endif /* HAVE_EXT_POSTGRESQL */

		  case MODE_LICENSE_NORMAL:
            setType ("LICENSE", i, argv);
            setPPstyle (PP_VERBOSE, i, argv);
            break;

		  case MODE_SCHEDD_SUBMITTORS:
            setType ("SUBMITTOR", i, argv);
            setPPstyle (PP_SCHEDD_SUBMITTORS, i, argv);
            break;

		  case MODE_MASTER_NORMAL:
			setType ("MASTER", i, argv);
			setPPstyle (PP_MASTER_NORMAL, i, argv);
			break;

		  case MODE_COLLECTOR_NORMAL:
			setType ("COLLECTOR", i, argv);
			setPPstyle (PP_COLLECTOR_NORMAL, i, argv);
			break;

		  case MODE_NEGOTIATOR_NORMAL:
			setType ("NEGOTIATOR", i, argv);
			setPPstyle (PP_NEGOTIATOR_NORMAL, i, argv);
			break;

		  case MODE_CKPT_SRVR_NORMAL:
			setType ("CKPT_SRVR", i, argv);
			setPPstyle (PP_CKPT_SRVR_NORMAL, i, argv);
			break;

		  case MODE_STORAGE_NORMAL:
			setType ("STORAGE", i, argv);
			setPPstyle (PP_STORAGE_NORMAL, i, argv);
			break;

          case MODE_GRID_NORMAL:
            setType ("GRID", i, argv);
            setPPstyle (PP_GRID_NORMAL, i, argv);
			break;

		  case MODE_GENERIC_NORMAL:
			setType ("GENERIC", i, argv);
			setPPstyle (PP_GENERIC_NORMAL, i, argv);
			break;

		  case MODE_ANY_NORMAL:
			setType ("ANY", i, argv);
			setPPstyle (PP_ANY_NORMAL, i, argv);
			break;

		  case MODE_OTHER:
			setType ("GENERIC", i, argv);
			setPPstyle (PP_GENERIC, i, argv);
			break;

		  case MODE_HAD_NORMAL:
			setType ("HAD", i, argv);
			setPPstyle (PP_GENERIC, i, argv);
			break;

          default:
            fprintf (stderr, "Error:  Illegal mode %d\n", mode);
            break;
        }
        setBy = i;
        setArg = argv;
    } else {
		if( strcmp(argv,setArg)!=0 ) {
			fprintf (stderr, "Error:  Arg %d (%s) contradicts arg %d (%s)\n",
					 i, argv, setBy, setArg);
			exit (1);
		}
    }
}

