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
#include "ad_printmask.h"
#include "condor_adtypes.h"
#include "status_types.h"
#include "totals.h"

#include "condor_state.h"
#include "prettyPrint.h"

extern int  sdo_mode;

const char *
getPPStyleStr (ppOption pps)
{
	switch (pps)
	{
		case PP_NOTSET:			return "<Not set>";
		case PP_GENERIC:		return "Generic";
		case PP_SLOTS_NORMAL:	return "Normal (Slots)";
		case PP_STARTDAEMON:    return "Daemon (StartD)";
		case PP_STARTD_GPUS:    return "GPUs (StartD)";
		case PP_STARTD_BROKEN:    return "Broken (StartD)";

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
		case PP_SLOTS_SERVER:	return "Server";
		case PP_SLOTS_RUN:		return "Run";
		case PP_SLOTS_COD:		return "COD";
		case PP_SLOTS_GPUS:		return "GPUs (Slots)";
		case PP_SLOTS_STATE:	return "State";
		case PP_SLOTS_BROKEN:	return "Broken (Slots)";
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

void PrettyPrinter::reportPPconflict(const char * argv, const char * more)
{
	fprintf (stderr, "Error:  arg (%s) contradicts arg %d (%s) %s\n",
		argv, setby.ppArgIndex, setby.ppArg ? setby.ppArg : "DEFAULT", more ? more : "");
}


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
	SDO(SDO_Slots,         SLOT_AD,   PP_SLOTS_NORMAL),
	SDO(SDO_Slots_Avail,   SLOT_AD,   PP_SLOTS_NORMAL),
	SDO(SDO_Slots_Claimed, SLOT_AD,   PP_SLOTS_RUN),
	SDO(SDO_Slots_Cod,     SLOT_AD,   PP_SLOTS_COD),
	SDO(SDO_Slots_GPUs,    SLOT_AD,   PP_SLOTS_GPUS),
	SDO(SDO_Slots_Broken,  SLOT_AD,   PP_SLOTS_BROKEN),
	SDO(SDO_StartDaemon,   STARTDAEMON_AD, PP_STARTDAEMON),
	SDO(SDO_StartD_GPUs,   STARTDAEMON_AD, PP_STARTD_GPUS),
	SDO(SDO_StartD_Broken, STARTDAEMON_AD, PP_STARTD_BROKEN),
	SDO(SDO_Schedd,        SCHEDD_AD, PP_SCHEDD_NORMAL),
	SDO(SDO_Schedd_Data,   SCHEDD_AD, PP_SCHEDD_DATA),
	SDO(SDO_Schedd_Run,    SCHEDD_AD, PP_SCHEDD_RUN),
	SDO(SDO_Submitters, SUBMITTOR_AD, PP_SUBMITTER_NORMAL),
	SDO(SDO_Master,        MASTER_AD, PP_MASTER_NORMAL),
	SDO(SDO_Collector,  COLLECTOR_AD, PP_COLLECTOR_NORMAL),
	SDO(SDO_CkptSvr,    CKPT_SRVR_AD, PP_CKPT_SRVR_NORMAL),
	SDO(SDO_Grid,            GRID_AD, PP_GRID_NORMAL),
	SDO(SDO_License,      LICENSE_AD, PP_LONG),
	SDO(SDO_Storage,      STORAGE_AD, PP_STORAGE_NORMAL),
	SDO(SDO_Negotiator,NEGOTIATOR_AD, PP_NEGOTIATOR_NORMAL),
	SDO(SDO_Defrag,        DEFRAG_AD, PP_DEFRAG_NORMAL),
	SDO(SDO_Accounting,ACCOUNTING_AD, PP_ACCOUNTING_NORMAL),
	SDO(SDO_Generic,      GENERIC_AD, PP_GENERIC_NORMAL),
	SDO(SDO_Any,              ANY_AD, PP_GENERIC),
	SDO(SDO_Other,        GENERIC_AD, PP_GENERIC),
	SDO(SDO_HAD,              HAD_AD, PP_GENERIC),
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



