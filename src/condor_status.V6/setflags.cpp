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
#include "condor_config.h"
#include "condor_api.h"
#include "condor_adtypes.h"
#include "status_types.h"
#include "totals.h"

#define USE_OLD_DIAGNOSTIC_NAMES 1
#define USE_PROJECTION_SET 1


//extern AdTypes	type;
//extern Mode		mode;
extern int  sdo_mode;
extern ppOption	ppStyle;
extern bool explicit_format;
extern bool using_print_format;
extern bool disable_user_print_files; // allow command line to defeat use of default user print files.
extern const char * mode_constraint; // constraint expression set by setMode
extern int set_status_print_mask_from_stream (const char * streamid, bool is_filename, const char ** pconstraint);

static struct {
	AdTypes      adType;
	int          argIndex;
	const char * Arg;
	int          ppArgIndex;
	const char * ppArg;
} setby = {NO_AD, 0, NULL, 0, NULL};

#if 0
const char * getTypeStr ();
#endif

// lookup table to convert condor AD type constants into strings.
static const char * const adtype_names[] = {
	QUILL_ADTYPE,
	STARTD_ADTYPE,
	SCHEDD_ADTYPE,
	MASTER_ADTYPE,
	GATEWAY_ADTYPE,
	CKPT_SRVR_ADTYPE,
	STARTD_PVT_ADTYPE,
	SUBMITTER_ADTYPE,
	COLLECTOR_ADTYPE,
	LICENSE_ADTYPE,
	STORAGE_ADTYPE,
	ANY_ADTYPE,
	BOGUS_ADTYPE,		// placeholder: NUM_AD_TYPES used wrongly to be here
	CLUSTER_ADTYPE,
	NEGOTIATOR_ADTYPE,
	HAD_ADTYPE,
	GENERIC_ADTYPE,
	CREDD_ADTYPE,
	DATABASE_ADTYPE,
	DBMSD_ADTYPE,
	TT_ADTYPE,
	GRID_ADTYPE,
	XFER_SERVICE_ADTYPE,
	LEASE_MANAGER_ADTYPE,
	DEFRAG_ADTYPE,
	ACCOUNTING_ADTYPE,
};
const char * getAdTypeStr (AdTypes type) {
	if (type < 0 || type >= (int)COUNTOF(adtype_names)) return "<Unknown type!>";
	return adtype_names[type];
}


const char *
getPPStyleStr (ppOption pps)
{
	switch (pps)
	{
    	case PP_NOTSET:			return "<Not set>";
		case PP_GENERIC:		return "Generic";
    	case PP_STARTD_NORMAL:	return "Normal (Startd)";
    	case PP_SCHEDD_NORMAL:	return "Normal (Schedd)";
    	case PP_SUBMITTER_NORMAL:return "Normal (Submitter)";
    	case PP_MASTER_NORMAL:	return "Normal (Master)";
    	case PP_CKPT_SRVR_NORMAL:return"Normal (CkptSrvr)";
		case PP_COLLECTOR_NORMAL:return"Normal (Collector)";
	    case PP_NEGOTIATOR_NORMAL: return "Normal (Negotiator)";
		case PP_DEFRAG_NORMAL:  return "Normal (Defrag)";
		case PP_ACCOUNTING_NORMAL:  return "Normal (Accounting)";
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
	return "<Unknown!>";
}

#if 1
int setPPstyle (ppOption pps, int arg_index, const char * argv)
{
	// if the style has already been set, and is trying to be reset by default
	// rules, override the default  (i.e., don't make a change)
	if (setby.ppArgIndex != 0 && ! arg_index)
		return 0;

	// If -long or -xml or -format are specified, do not reset to
	// "normal" style when followed by a flag such as -startd.
	if( ppStyle == PP_XML || ppStyle == PP_VERBOSE || ppStyle == PP_CUSTOM )
	{
		if( pps != PP_XML && pps != PP_VERBOSE && pps != PP_CUSTOM ) {
				// ignore this style setting and keep our existing setting
			return 0;
		}
	}

	// If setting a 'normal' output, check to see if there is a user-defined normal output
	if ( ! disable_user_print_files && ! explicit_format
		&& pps != PP_XML && pps != PP_VERBOSE && pps != PP_CUSTOM && pps != ppStyle) {
		MyString param_name("STATUS_DEFAULT_"); param_name += getAdTypeStr(setby.adType); param_name += "_PRINT_FORMAT_FILE";
		auto_free_ptr pf_file(param(param_name.c_str()));
		if (pf_file) {
			struct stat stat_buff;
			if (0 != stat(pf_file.ptr(), &stat_buff)) {
				// do nothing, this is not an error.
			} else if (set_status_print_mask_from_stream(pf_file, true, &mode_constraint) < 0) {
				fprintf(stderr, "Warning: default %s select file '%s' is invalid\n", getAdTypeStr(setby.adType), pf_file.ptr());
			} else {
				using_print_format = true;
			}
		}
	}

	if ( (PP_XML == pps) || PP_VERBOSE == pps || (ppStyle <= pps || setby.ppArgIndex == 0) ) {
		ppStyle = pps;
		setby.ppArgIndex = arg_index;
		setby.ppArg = argv;
		return 1;
	} else {
		fprintf (stderr, "Error:  arg %d (%s) contradicts arg %d (%s)\n",
			arg_index, argv, setby.ppArgIndex, setby.ppArg ? setby.ppArg : "DEFAULT");
		return -1;
	}
}
#else
void
setPPstyle (ppOption pps, int i, const char *argv)
{
    static int setBy = 0;
    static const char *setArg = NULL;

	if (argv == NULL && i < 0) {
		fprintf (i==-2?stderr:stdout,"PrettyPrint: %s   (Set by arg %d '%s')\nTotals: %s\n",
				getPPStyleStr(ppStyle), setBy, setArg, getPPStyleStr(pps));
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

	// If setting a 'normal' output, check to see if there is a user-defined normal output
	if ( ! disable_user_print_files && ! explicit_format
		&& pps != PP_XML && pps != PP_VERBOSE && pps != PP_CUSTOM && pps != ppStyle) {
		MyString param_name("STATUS_DEFAULT_"); param_name += getAdTypeStr(setby.adType); param_name += "_PRINT_FORMAT_FILE";
		char * pf_file = param(param_name.c_str());
		if (pf_file) {
			struct stat stat_buff;
			if (0 != stat(pf_file, &stat_buff)) {
				// do nothing, this is not an error.
			} else if (set_status_print_mask_from_stream(pf_file, true, &mode_constraint) < 0) {
				fprintf(stderr, "Warning: default %s select file '%s' is invalid\n", getAdTypeStr(setby.adType), pf_file);
			} else {
				using_print_format = true;
			}
			free(pf_file);
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
#endif

#ifdef USE_OLD_DIAGNOSTIC_NAMES
const char * getOldModeStr(int sdo_mode)
{
	switch (sdo_mode)
	{
		case SDO_NotSet:		return "Not set";
		case SDO_Defrag:	return "Normal (Defrag)";
		case SDO_Startd:	return "Normal (Startd)";
		case SDO_Startd_Avail:		return "Available (Startd)";
		case SDO_Startd_Run:		return "Run (Startd)";
		case SDO_Startd_Cod:		return "COD (Startd)";
		case SDO_Quill:		return "Normal (Quill)";
		case SDO_Schedd:	return "Normal (Schedd)";
		case SDO_Submitters:	return "Submittors (Schedd)";
		case SDO_Master:	return "Normal (Master)";
		case SDO_CkptSvr:	return "Normal (CkptSrvr)";
		case SDO_Collector:	return "Normal (Collector)";
		case SDO_Negotiator:	return "Normal (Negotiator)";
		case SDO_Grid:          return "Normal (Grid)";
		case SDO_Storage:	return "Normal (Storage)";
		case SDO_Generic:	return "Normal (Generic)";
		case SDO_Other:		return "Generic";
		case SDO_HAD:		return "Had";
		case SDO_Any:		return "Normal (Any)";
		default: break;
	}
	return "<Unknown!>";
}
const char * getOldAdTypeStr (AdTypes type)
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
		default: break;
	}
	return "<Unknown type!>";
}
#endif


#if 1

// this table defines the default ad type and output format for each
// of the command line arguments that defines such.
// for instance  -schedd sets the
//   mode to SDO_Schedd,
//   adType to SCHEDD_AD,
//   ppStyle to PP_SCHEDD_NORMAL
//
static const struct _sdo_mode_info {
	int      mode;
	AdTypes  adType;
	ppOption pps;
	const char * str; // label for debugging/dry-run code
} sdo_modes[] = {
#define SDO(m, adt, pp) {m, adt, pp, #m}
	SDO(SDO_NotSet, NO_AD, PP_NOTSET),
	SDO(SDO_Startd,        STARTD_AD, PP_STARTD_NORMAL),//  MODE_STARTD_NORMAL,
	SDO(SDO_Startd_Avail,  STARTD_AD, PP_STARTD_NORMAL),//  MODE_STARTD_AVAIL,
	SDO(SDO_Startd_Run,    STARTD_AD, PP_STARTD_RUN),	//  MODE_STARTD_RUN,
	SDO(SDO_Startd_Cod,    STARTD_AD, PP_STARTD_COD),	//  MODE_STARTD_COD,
	SDO(SDO_Quill,          QUILL_AD, PP_QUILL_NORMAL),	//  MODE_QUILL_NORMAL,
	SDO(SDO_Schedd,        SCHEDD_AD, PP_SCHEDD_NORMAL),//  MODE_SCHEDD_NORMAL,
	SDO(SDO_Submitters, SUBMITTOR_AD, PP_SUBMITTER_NORMAL),//  MODE_SCHEDD_SUBMITTORS,
	SDO(SDO_Master,        MASTER_AD, PP_MASTER_NORMAL),	//  MODE_MASTER_NORMAL,
	SDO(SDO_Collector,  COLLECTOR_AD, PP_COLLECTOR_NORMAL),	//  MODE_COLLECTOR_NORMAL,
	SDO(SDO_CkptSvr,    CKPT_SRVR_AD, PP_CKPT_SRVR_NORMAL),	//  MODE_CKPT_SRVR_NORMAL,
	SDO(SDO_Grid,            GRID_AD, PP_GRID_NORMAL),		//  MODE_GRID_NORMAL,
	SDO(SDO_License,      LICENSE_AD, PP_VERBOSE),			//  MODE_LICENSE_NORMAL,
	SDO(SDO_Storage,      STORAGE_AD, PP_STORAGE_NORMAL),	//  MODE_STORAGE_NORMAL,
	SDO(SDO_Negotiator,NEGOTIATOR_AD, PP_NEGOTIATOR_NORMAL),//  MODE_NEGOTIATOR_NORMAL,
	SDO(SDO_Defrag,        DEFRAG_AD, PP_DEFRAG_NORMAL),	//  MODE_DEFRAG_NORMAL,
	SDO(SDO_Accounting,ACCOUNTING_AD, PP_ACCOUNTING_NORMAL),
	SDO(SDO_Generic,      GENERIC_AD, PP_GENERIC_NORMAL),	//  MODE_GENERIC_NORMAL,
	SDO(SDO_Any,              ANY_AD, PP_GENERIC),			//  MODE_ANY_NORMAL,
	SDO(SDO_Other,        GENERIC_AD, PP_GENERIC),			//  MODE_OTHER,
	SDO(SDO_HAD,              HAD_AD, PP_GENERIC),			//  MODE_HAD_NORMAL
#undef SDO
};

const char * getSDOModeStr(int sm) {
	for (int ii = 0; ii < (int)COUNTOF(sdo_modes); ++ii) {
		if (sdo_modes[ii].mode == sm) {
			return sdo_modes[ii].str;
		}
	}
	return "<Unknown mode!>";
}

#else
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
#endif

#if 0

static void
setType (const char *dtype, int i, const char *argv)
{
#if 1
	int setBy = setby.typeIndex;
	const char * setArg = setby.typeArg;
#else
    static int setBy = 0;
    static const char *setArg = NULL;

	if (argv == NULL) {
		fprintf (i==-2?stderr:stdout,"Query type: %s   (Set by arg %d '%s')\n", getTypeStr(), setBy, setArg);
		return;
	}
#endif

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
#if 1
        setby.typeIndex = i;
        setby.typeArg = argv;
#else
        setBy = i;
        setArg = argv;
#endif
    } else {
#if 1
        fprintf (stderr, "Error:  Daemon type implied by arg %d (%s) contradicts arg %d (%s)\n",
                 i, argv, setby.typeIndex, setby.typeArg);
#else
        fprintf (stderr,
        "Error:  Daemon type implied by arg %d (%s) contradicts arg %d (%s)\n",
        	i, argv, setBy, setArg);
#endif
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
		case SDO_Other:		return "Generic";
		case MODE_HAD_NORMAL:		return "Had";
		case MODE_ANY_NORMAL:		return "Normal (Any)";
		default:			return "<Unknown!>";
	}
	// should never get here
	exit (1);
}
#endif

AdTypes setMode (int sm, int i, const char *argv)
{
#if 1
	if ( ! setby.argIndex) { // if not yet set. 
		AdTypes adType = NO_AD;
		ppOption pps = PP_NOTSET;

		// look for a matching mode in the modes table
		for (int ii = 0; ii < (int)COUNTOF(sdo_modes); ++ii) {
			if (sdo_modes[ii].mode == sm) {
				adType = sdo_modes[ii].adType;
				pps = sdo_modes[ii].pps;
			}
		}
		// if not matching mode, print error
		if (adType == NO_AD) {
			fprintf (stderr, "Error:  Illegal mode %d\n", sm);
			return setby.adType;
		}
		sdo_mode = sm;
		setby.adType = adType;
		setby.argIndex = i;
		setby.Arg = argv;
		setPPstyle (pps, i, argv);
	} else if (i && argv) {
		// an arg can be repeated, but a conflicting arg may not be used...
		if( strcmp(argv,setby.Arg)!=0 ) {
			fprintf (stderr, "Error:  Arg %d (%s) contradicts arg %d (%s)\n",
					 i, argv, setby.argIndex, setby.Arg);
			exit (1);
		}
	}
	return setby.adType;
#else
    static int setBy = 0;
    static const char *setArg = NULL;

	if (argv == NULL) {
		fprintf(i==-2?stderr:stdout,"Mode: %s   (Set by arg %d '%s')\n", getModeStr(), setBy, setArg);
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
#endif
}

void dumpPPMode(FILE* out)
{
	const char * sdo_str = getSDOModeStr(sdo_mode);
	const char * adtype_str = getAdTypeStr(setby.adType);
	const char * style_str = getPPStyleStr(ppStyle);
#ifdef USE_OLD_DIAGNOSTIC_NAMES
	sdo_str = getOldModeStr(sdo_mode);
	adtype_str = getOldAdTypeStr(setby.adType);
#endif
	fprintf(out,  "Mode: %s   (Set by arg %d '%s')\n", sdo_str, setby.argIndex, setby.Arg);
	fprintf (out, "Query type: %s   (Set by arg %d '%s')\n", adtype_str, setby.argIndex, setby.Arg);
	fprintf (out, "PrettyPrint: %s   (Set by arg %d '%s')\n", style_str, setby.ppArgIndex, setby.ppArg ? setby.ppArg : "NULL");
}

#ifdef USE_PROJECTION_SET
#else
const CustomFormatFnTable * getCondorStatusPrintFormats();
void dumpPPMask (std::string & out, AttrListPrintMask & mask)
{
	extern List<const char> pm_head; // The list of headings for the mask entries
	extern void prettyPrintInitMask();
	prettyPrintInitMask();

	List<const char> * pheadings = NULL;
	if ( ! mask.has_headings()) {
		if (pm_head.Length() > 0) pheadings = &pm_head;
	}

	mask.dump(out, getCondorStatusPrintFormats(), pheadings);
}
#endif

