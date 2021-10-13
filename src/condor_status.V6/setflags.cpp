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

#include "condor_state.h"
#include "prettyPrint.h"

// use old diagnostic names when comparing v8.3 to v8.5
#define USE_OLD_DIAGNOSTIC_NAMES 1

extern int  sdo_mode;
#ifdef OLD_PF_SETUP
extern bool explicit_format;
extern bool using_print_format;
extern bool disable_user_print_files; // allow command line to defeat use of default user print files.
extern const char * mode_constraint; // constraint expression set by setMode
#endif

const char *
getPPStyleStr (ppOption pps)
{
	switch (pps)
	{
		case PP_NOTSET:			return "<Not set>";
		case PP_GENERIC:		return "Generic";
		case PP_STARTD_NORMAL:	return "Normal (Startd)";
		case PP_SCHEDD_NORMAL:	return "Normal (Schedd)";
		case PP_SCHEDD_DATA:	return "Data (Schedd)";
		case PP_SCHEDD_RUN:		return "Run (Schedd)";
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
		case PP_LONG:			return "long";
		case PP_XML:		    return "XML";
		case PP_JSON:		    return "JSON";
		case PP_NEWCLASSAD:	    return "NewClassad";
		case PP_CUSTOM:			return "Custom";
		default:				return "<Unknown!>";
	}
}

int PrettyPrinter::setPPstyle( ppOption pps, int arg_index, const char * argv )
{
	// if the style has already been set, and is trying to be reset by default
	// rules, override the default  (i.e., don't make a change)
	if (setby.ppArgIndex != 0 && ! arg_index)
		return 0;

	// If -long or -xml or -format are specified, do not reset to
	// "normal" style when followed by a flag such as -startd.
	if (PP_IS_LONGish(ppStyle) || ppStyle == PP_CUSTOM)
	{
		if ( ! PP_IS_LONGish(pps) && pps != PP_CUSTOM) {
				// ignore this style setting and keep our existing setting
			return 0;
		}
	}

#ifdef OLD_PF_SETUP // moving this code
	// If setting a 'normal' output, check to see if there is a user-defined normal output
	if ( ! disable_user_print_files && ! explicit_format
		&& !PP_IS_LONGish(pps) && pps != PP_CUSTOM && pps != ppStyle) {
		std::string param_name("STATUS_DEFAULT_"); param_name += AdTypeToString(setby.adType); param_name += "_PRINT_FORMAT_FILE";
		auto_free_ptr pf_file(param(param_name.c_str()));
		if (pf_file) {
			struct stat stat_buff;
			if (0 != stat(pf_file.ptr(), &stat_buff)) {
				// do nothing, this is not an error.
			} else if (set_status_print_mask_from_stream(pf_file, true, &mode_constraint) < 0) {
				fprintf(stderr, "Warning: default %s select file '%s' is invalid\n", AdTypeToString(setby.adType), pf_file.ptr());
			} else {
				using_print_format = true;
			}
		}
	}
#endif

	if ( PP_IS_LONGish(pps) || (ppStyle <= pps || setby.ppArgIndex == 0) ) {
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

#ifdef USE_OLD_DIAGNOSTIC_NAMES
const char * getOldModeStr(int sdo_mode)
{
	switch (sdo_mode)
	{
		case SDO_NotSet:		return "Not set";
		case SDO_Defrag:	return "Normal (Defrag)";
		case SDO_Startd:	return "Normal (Startd)";
		case SDO_Startd_Avail:		return "Available (Startd)";
		case SDO_Startd_Claimed:	return "Claimed (Startd)";
		case SDO_Startd_Cod:		return "COD (Startd)";
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
	SDO(SDO_Startd_Claimed,STARTD_AD, PP_STARTD_RUN),	//  MODE_STARTD_RUN,
	SDO(SDO_Startd_Cod,    STARTD_AD, PP_STARTD_COD),	//  MODE_STARTD_COD,
	SDO(SDO_Schedd,        SCHEDD_AD, PP_SCHEDD_NORMAL),//  MODE_SCHEDD_NORMAL,
	SDO(SDO_Schedd_Data,   SCHEDD_AD, PP_SCHEDD_DATA),     //  MODE_SCHEDD_NORMAL,
	SDO(SDO_Schedd_Run,    SCHEDD_AD, PP_SCHEDD_RUN),      //  MODE_SCHEDD_NORMAL,
	SDO(SDO_Submitters, SUBMITTOR_AD, PP_SUBMITTER_NORMAL),//  MODE_SCHEDD_SUBMITTORS,
	SDO(SDO_Master,        MASTER_AD, PP_MASTER_NORMAL),	//  MODE_MASTER_NORMAL,
	SDO(SDO_Collector,  COLLECTOR_AD, PP_COLLECTOR_NORMAL),	//  MODE_COLLECTOR_NORMAL,
	SDO(SDO_CkptSvr,    CKPT_SRVR_AD, PP_CKPT_SRVR_NORMAL),	//  MODE_CKPT_SRVR_NORMAL,
	SDO(SDO_Grid,            GRID_AD, PP_GRID_NORMAL),		//  MODE_GRID_NORMAL,
	SDO(SDO_License,      LICENSE_AD, PP_LONG),				//  MODE_LICENSE_NORMAL,
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

AdTypes PrettyPrinter::setMode (int sm, int i, const char *argv)
{
	if ( ! setby.argIndex) { // if not yet set. 
		AdTypes adType = NO_AD;
		ppOption pps = PP_NOTSET;

		// look for a matching mode in the modes table
		for (int ii = 0; ii < (int)COUNTOF(sdo_modes); ++ii) {
			if (sdo_modes[ii].mode == sm) {
				adType = sdo_modes[ii].adType;
				pps = sdo_modes[ii].pps;
				break;
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
}

AdTypes PrettyPrinter::resetMode(int sm, int arg_index, const char * arg)
{
	setby.argIndex = 0;
	return setMode(sm, arg_index, arg);
}

void PrettyPrinter::dumpPPMode(FILE* out) const
{
	const char * sdo_str = getSDOModeStr(sdo_mode);
	const char * adtype_str = AdTypeToString(setby.adType);
	const char * style_str = getPPStyleStr(ppStyle);
#ifdef USE_OLD_DIAGNOSTIC_NAMES
	sdo_str = getOldModeStr(sdo_mode);
	adtype_str = getOldAdTypeStr(setby.adType);
#endif
	fprintf(out,  "Mode: %s   (Set by arg %d '%s')\n", sdo_str, setby.argIndex, setby.Arg);
	fprintf (out, "Query type: %s   (Set by arg %d '%s')\n", adtype_str, setby.argIndex, setby.Arg);
	fprintf (out, "PrettyPrint: %s   (Set by arg %d '%s')\n", style_str, setby.ppArgIndex, setby.ppArg ? setby.ppArg : "NULL");
}

const char * PrettyPrinter::adtypeNameFromPPMode() const
{
	return AdTypeToString(setby.adType);
}

const char * paramNameFromPPMode(std::string &param_name)
{
	const char * p = getSDOModeStr(sdo_mode);
	if ( ! p) {
		formatstr(param_name, "mode %d", sdo_mode);
		return param_name.c_str();
	};

	param_name = "STATUS_";
	if ( ! strchr(p+4,'_')) { param_name += "DEFAULT_"; }
	param_name += p+4;
	param_name += "_PRINT_FORMAT_FILE";
	return param_name.c_str();
}



