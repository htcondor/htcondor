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
#include "condor_config.h"
#include "condor_state.h"
#include "status_types.h"
#include "totals.h"
#include "format_time.h"
#include "string_list.h"
#include "metric_units.h"
#include "console-utils.h"
#include "prettyPrint.h"
#include "setflags.h"

// global mode bits
extern bool javaMode;
extern bool vmMode;
extern bool absentMode;
extern bool offlineMode;
extern bool compactMode;

static time_t stashed_now = 0;
const int cchReserveForPrintingAds = 16384;

void printCOD    		(ClassAd *);
void printVerbose   	(ClassAd &, classad::References * attrs=NULL);
void printXML       	(ClassAd &, classad::References * attrs=NULL);
void printJSON       	(ClassAd &, bool first_ad, classad::References * attrs=NULL);
void printNewClassad	(ClassAd &, bool first_ad, classad::References * attrs=NULL);
void printCustom    	(ClassAd *);


int PrettyPrinter::getDisplayWidth(bool * is_piped) const {
	if (is_piped) *is_piped = false;
	if (forced_display_width <= 0) {
		int width = getConsoleWindowSize();
		if (width <= 0) {
			if (is_piped) *is_piped = true;
			return wide_display ? 1024 : 80;
		}
		return width;
	}
	return forced_display_width;
}

void PrettyPrinter::setPPwidth () {
	bool is_piped = false;
	int display_width = getDisplayWidth(&is_piped)-1;
	if (forced_display_width || ( ! wide_display && ! is_piped) ) {
		pm.SetOverallWidth(display_width);
	}
}

static int ppWidthOpts(int width, int truncate)
{
	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	return opts;
}

constexpr const int	FormatOptionSpecial001 = FormatOptionSpecialBase;
constexpr const int	FormatOptionSpecial002 = FormatOptionSpecialBase<<1;
//constexpr const int	FormatOptionSpecial004 = FormatOptionSpecialBase<<2;
//constexpr const int	FormatOptionSpecial008 = FormatOptionSpecialBase<<3;

static int ppAltOpts(ivfield alt_in)
{
	ivfield alt = (ivfield)(alt_in & 3);
	int opts = 0;
	if (alt == WideInvalidField) {
		opts |= AltQuestion | AltWide;
	} else if (alt == ShortInvalidField) {
		opts |= AltQuestion;
	} else if (alt == CallInvalidField) {
		opts |= FormatOptionAlwaysCall;
	} else if (alt != BlankInvalidField) {
		//opts |= AltFixMe;
	}
	if (alt_in & FitToName) { opts |= FormatOptionFitToData | FormatOptionSpecial001; }
	if (alt_in & FitToMachine) { opts |= FormatOptionFitToData | FormatOptionSpecial002; }
	return opts;
}

void PrettyPrinter::ppSetColumnFormat(const char * print, int width, bool truncate, ivfield alt, const char * attr)
{
	int opts = ppWidthOpts(width, truncate) | ppAltOpts(alt);
	pm.registerFormat(print, width, opts, attr);
}

void PrettyPrinter::ppSetColumnFormat(const CustomFormatFn & fmt, const char * print, int width, bool truncate, ivfield alt, const char * attr)
{
	int opts = ppWidthOpts(width, truncate) | ppAltOpts(alt);
	if (width == 11 && fmt.IsNumber() && (fmt.Is(render_elapsed_time) || fmt.Is(format_real_time))) {
		opts |= FormatOptionNoPrefix;
		width = 12;
	}
	pm.registerFormat(print, width, opts, fmt, attr);
}

void PrettyPrinter::ppSetColumn(const char * attr, const Lbl & label, int width, bool truncate, ivfield alt)
{
	pm_head.emplace_back(label);
	ppSetColumnFormat("%v", width, truncate, alt, attr);
}

void PrettyPrinter::ppSetColumn(const char * attr, int width, bool truncate, ivfield alt)
{
	pm_head.emplace_back(attr);
	ppSetColumnFormat("%v", width, truncate, alt, attr);
}

void PrettyPrinter::ppSetColumn(const char * attr, const Lbl & label, const char * print, bool truncate, ivfield alt)
{
	pm_head.emplace_back(label);
	ppSetColumnFormat(print, 0, truncate, alt, attr);
}

void PrettyPrinter::ppSetColumn(const char * attr, const char * print, bool truncate, ivfield alt)
{
	pm_head.emplace_back(attr);
	ppSetColumnFormat(print, 0, truncate, alt, attr);
}

void PrettyPrinter::ppSetColumn(const char * attr, const Lbl & label, const CustomFormatFn & fmt, int width, bool truncate, ivfield alt)
{
	pm_head.emplace_back(label);
	ppSetColumnFormat(fmt, NULL, width, truncate, alt, attr);
}

void PrettyPrinter::ppSetColumn(const char * attr, const CustomFormatFn & fmt, int width, bool truncate, ivfield alt)
{
	pm_head.emplace_back(attr);
	ppSetColumnFormat(fmt, NULL, width, truncate, alt, attr);
}

void PrettyPrinter::ppSetColumn(const char * attr, const Lbl & label, const CustomFormatFn & fmt, const char * print, int width, bool truncate, ivfield alt)
{
	pm_head.emplace_back(label);
	ppSetColumnFormat(fmt, print, width, truncate, alt, attr);
}

void PrettyPrinter::ppDisplayHeadings(FILE* file, ClassAd *ad, const char * pszExtra)
{
	if (ad) {
		// render the first ad to a string so the column widths update
		std::string tmp;
		pm.display(tmp, ad, NULL);
	}
	if (pm.has_headings()) {
		pm.display_Headings(file);
	} else {
		pm.display_Headings(file, pm_head);
	}
	if (pszExtra)
		printf("%s", pszExtra);
}

void PrettyPrinter::prettyPrintInitMask(classad::References & proj, const char * & constr, bool no_pr_files)
{
	bool long_form = PP_IS_LONGish(ppStyle);
	bool custom = (ppStyle == PP_CUSTOM);
	if ( ! using_print_format && ! wantOnlyTotals && ! custom && ! long_form) {
		ppInitPrintMask(ppStyle, proj, constr, no_pr_files);
	}
}

// this struct used pass args to the ppAdjustProjection or ppAdjustNameWidth callback
struct _adjust_widths_info {
	int name_width;
	int name_flags;
	int machine_width;
	int machine_flags;
	classad::References * proj;

	bool wide_display;
	int & ixCol_Name;
	int & ixCol_Machine;
};

int PrettyPrinter::ppAdjustNameWidth( void * pv, Formatter * fmt ) const
{
	struct _adjust_widths_info * pi = (struct _adjust_widths_info *)pv;
	if (fmt->options & FormatOptionSpecial001) {
		const int max_allowable = wide_display ? 128 : 50;
		if (pi->name_width) {
			int max_width = pi->name_width;
			if (fmt->width > max_width+4) fmt->width = max_width+4; // shrink
			else if (wide_display && fmt->width < max_width && fmt->width < max_allowable) {
				fmt->width = MIN(max_allowable, max_width); // grow column
			}
			fmt->options &= ~FormatOptionAutoWidth;
		}
	}
	if (fmt->options & FormatOptionSpecial002) {
		const int max_allowable = wide_display ? 128 : 50;
		if (pi->machine_width) {
			int max_width = pi->machine_width;
			if (fmt->width > max_width+4) fmt->width = max_width+4; // shrink
			else if (wide_display && fmt->width < max_width && fmt->width < max_allowable) {
				fmt->width = MIN(max_allowable, max_width); // grow column
			}
			fmt->options &= ~FormatOptionAutoWidth;
		}
	}
	return 0;
}

int ppCropNameWidth(void*pv, int /*index*/, Formatter * fmt, const char * /*attr*/)
{
	struct _adjust_widths_info * pi = (struct _adjust_widths_info *)pv;
	if (fmt->options & FormatOptionSpecial001) {
		if (pi->name_width) {
			fmt->width = pi->name_width;
			fmt->options &= ~FormatOptionAutoWidth;
		}
	}
	if (fmt->options & FormatOptionSpecial002) {
		if (pi->machine_width) {
			fmt->width = pi->machine_width;
			fmt->options &= ~FormatOptionAutoWidth;
		}
	}
	return 0;
}

int ppFetchColumnWidths(void*pv, int /*index*/, Formatter * fmt, const char * /*attr*/)
{
	struct _adjust_widths_info * pi = (struct _adjust_widths_info *)pv;
	if (fmt->options & FormatOptionSpecial001) {
		pi->name_width = fmt->width;
	}
	if (fmt->options & FormatOptionSpecial002) {
		pi->machine_width = fmt->width;
	}
	return 0;
}

ppOption PrettyPrinter::prettyPrintHeadings (bool any_ads)
{
	ppOption pps = ppStyle;
	bool no_headings = wantOnlyTotals || ! any_ads;
	bool old_headings = (pps == PP_SLOTS_COD);
	bool long_form = PP_IS_LONGish(pps);
	const char * newline_after_headings = "\n";
	if ((pps == PP_CUSTOM) || using_print_format) {
		pps = PP_CUSTOM;
		no_headings = true;
		newline_after_headings = NULL;
		if ( ! wantOnlyTotals) {
			bool fHasHeadings = pm.has_headings() || (pm_head.size() > 0);
			if (fHasHeadings) {
				no_headings = (pmHeadFoot & HF_NOHEADER);
			}
		}
	} else if (old_headings || long_form) {
		no_headings = true;
	} else {
		// before we print headings, adjust the width of the name column
		bool is_piped = false;
		int display_width = getDisplayWidth(&is_piped)-1;
		if (any_ads && ( ! wide_display && ! is_piped ) ) {
			struct _adjust_widths_info wid_info = { 0, 0, 0, 0, NULL, wide_display, ixCol_Name, ixCol_Machine };
			if (width_of_fixed_cols > 0) {
				pm.adjust_formats(ppFetchColumnWidths, &wid_info);
				int remain = display_width - width_of_fixed_cols;
				if (ixCol_Name >= 0) remain -= wid_info.name_width;
				if (ixCol_Machine >= 0) remain -= wid_info.machine_width;
				if (remain < 0) {
					int crop = -remain;
					if (ixCol_Name >= 0) {
						if (ixCol_Machine >= 0) {
							int mwidth = MAX(7, wid_info.machine_width - (crop/2));
							crop -= (wid_info.machine_width - mwidth);
							wid_info.machine_width = mwidth;
						}
						wid_info.name_width = MAX(12, wid_info.name_width - crop);
					} else if (ixCol_Machine >= 0) {
						wid_info.machine_width = MAX(7, wid_info.machine_width - crop);
					}
					pm.adjust_formats(ppCropNameWidth, &wid_info);
				}
				//printf("\n_1234567890_2345678901_3456789012_4567890123_5678901234_6789012345_7890123456_8901234567_90123456789\n");
			}
		}
	}
	if ( ! no_headings) {
		//adList.Rewind();
		//ad = adList.Next();
		ClassAd * ad = NULL;
		// we pass an ad into ppDisplayHeadings so that it can adjust column widths.
		ppDisplayHeadings(stdout, ad, newline_after_headings);
	}

	return pps;
}

void PrettyPrinter::prettyPrintAd(ppOption pps, ClassAd *ad, int output_index, classad::References * includelist, bool fHashOrder)
{
	if ( ! ad) return;

	if (!wantOnlyTotals) {

		// as a special case to aid in some bug repro scenarios, honor the fHashOrder flag for
		// -long form classads even when there is an include list.
		if (pps == PP_LONG && fHashOrder && includelist && ! includelist->empty()) {
			classad::ClassAdUnParser unp;
			unp.SetOldClassAd( true, true );
			std::string line;
			for (classad::ClassAd::const_iterator itr = ad->begin(); itr != ad->end(); ++itr) {
				if (includelist->contains(itr->first)) {
					line = itr->first.c_str();
					line += " = ";
					unp.Unparse(line, itr->second);
					line += "\n";
					fputs(line.c_str(), stdout);
				}
			}
			if ( ! line.empty()) { fputs("\n", stdout); }
			return;
		}

		// get a sorted list of attributes to print for long form output or when there is an include list
		classad::References attrs;
		classad::References *proj = NULL;
		if (PP_IS_LONGish(pps) && ( ! fHashOrder || includelist)) {
			sGetAdAttrs(attrs, *ad, true, includelist);
			proj = &attrs;
		}

		switch (pps) {
		case PP_LONG:
			printVerbose(*ad, proj);
			break;
		case PP_XML:
			printXML (*ad, proj);
			break;
		case PP_JSON:
			printJSON (*ad, output_index == 0, proj);
			break;
		case PP_NEWCLASSAD:
			printNewClassad (*ad, output_index == 0, proj);
			break;

		case PP_SLOTS_COD:
			printCOD (ad);
			break;
		case PP_CUSTOM:
			printCustom (ad);
			break;
		default:
			pm.display(stdout, ad);
			break;
		}
	}
}

void PrettyPrinter::ppSetStartdOfflineCols()
{
		ppSetColumn( ATTR_NAME, -34, ! wide_display );
		// A custom printer for filtering out the ints would be handy.
		ppSetColumn( "OfflineUniverses", Lbl( "Offline Universes" ),
					 render_strings_from_list, -42, ! wide_display );

		// How should I print out the offline reasons and timestamps?

}

void PrettyPrinter::ppSetStartdAbsentCols()
{
		ppSetColumn(ATTR_NAME, -34, ! wide_display);
		ppSetColumn(ATTR_OPSYS, -10, true);
		ppSetColumn(ATTR_ARCH, -8, true);
		ppSetColumn(ATTR_LAST_HEARD_FROM, Lbl("Went Absent"), format_real_date, -11, true);
		ppSetColumn(ATTR_CLASSAD_LIFETIME, Lbl("Will Forget"), render_due_date, "%Y", -11, true);
}

const char * const startdCompact_PrintFormat = "SELECT\n"
	ATTR_MACHINE    " AS Machine      WIDTH AUTO\n"       // 0
	ATTR_OPSYS      " AS Platform     PRINTAS PLATFORM\n" // 1
	//	"split(CondorVersion)[1] AS Condor\n"
	ATTR_NUM_DYNAMIC_SLOTS " AS Slot PRINTF %4d OR ' '\n" // 2
	ATTR_SLOT_TYPE  " AS s NOPREFIX PRINTAS SLOT_COUNT_TAG\n"  // 3
	ATTR_TOTAL_SLOT_CPUS " AS Cpus  PRINTF %4d\n"              // 3
	"TotalSlotGpus        AS Gpus  PRINTF %4d\n"              // 4
	ATTR_TOTAL_SLOT_MEMORY "/1024.0 AS  ' TotalGb' PRINTF %8.2f\n" // 5
	ATTR_CPUS       " AS FreCpu PRINTF %6d\n"              // 6
	ATTR_MEMORY     "/1024.0 AS ' FreeGb ' PRINTF %8.2f\n"   // 7
	"TotalLoadAvg*1.0/TotalCpus AS CpuLoad PRINTF %7.2f\n" // 8
	ATTR_ACTIVITY   " AS ST WIDTH 2 PRINTAS ACTIVITY_CODE\n" // 9
	"RecentJobStarts/20.0 AS Jobs/Min PRINTF %8.2f OR _\n" // 10
	"Max(ChildMemory)/1024.0 AS MaxSlotGb   PRINTF %9.2f OR *\n" // 11
"WHERE " PMODE_STARTD_COMPACT_CONSTRAINT "\n"
"GROUP BY Machine\n"
"SUMMARY STANDARD\n";

int PrettyPrinter::ppSetStartdCompactCols (int /*width*/, int & mach_width, const char * & constr)
{
	const char * tag = "StartdCompact";
	const char * fmt = startdCompact_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

	startdCompact_ixCol_MachineSlot = 0;
	startdCompact_ixCol_Platform = 1;
	startdCompact_ixCol_Slots = 2;
	int delta=1; // set to 1 if SLOT_TYPE_COLUMN is shown
	startdCompact_ixCol_FreeCpus = 6+delta;
	startdCompact_ixCol_FreeMem = 7+delta; // double
	startdCompact_ixCol_ActCode = 9+delta;
	startdCompact_ixCol_JobStarts = 10+delta; // double
	startdCompact_ixCol_MaxSlotMem = 11+delta; // double

	mach_width = 12;
	return 12;
}

void PrettyPrinter::ppSetStartdNormalCols (int width)
{
	ivfield fit = WideInvalidField;
	int name_width = wide_display ? -34 : -18;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 61-width); fit = FitToName; }

	ppSetColumn(ATTR_NAME, name_width, ! wide_display, fit);

	if (javaMode) {
		ppSetColumn(ATTR_JAVA_VENDOR, -10, ! wide_display);
		ppSetColumn(ATTR_JAVA_VERSION, Lbl("Ver"), -6, ! wide_display);
	} else if (vmMode) {
		ppSetColumn(ATTR_VM_TYPE, Lbl("VmType"), -6, true);
		ppSetColumn(ATTR_VM_NETWORKING_TYPES, Lbl("Network"), -9, true);
	}else {
		ppSetColumn(ATTR_OPSYS, -10, true);
		ppSetColumn(ATTR_ARCH, -6, true);
	}
	ppSetColumn(ATTR_STATE,    -9, true);
	ppSetColumn(ATTR_ACTIVITY, -8, true);

	ppSetColumn(ATTR_CONDOR_LOAD_AVG, Lbl("LoadAv"), format_load_avg, NULL, 6, true);

	if (vmMode) {
		ppSetColumn(ATTR_VM_MEMORY, Lbl("VMMem"), "%4d", false);
	} else {
		ppSetColumn(ATTR_MEMORY, Lbl("Mem"), "%4d", false);
	}
	pm_head.emplace_back(wide_display ? "ActivityTime" : "  ActvtyTime");
	pm.registerFormat("%T", 12, FormatOptionAutoWidth | (wide_display ? 0 : FormatOptionNoPrefix) | AltDash,
		render_activity_time, ATTR_ENTERED_CURRENT_ACTIVITY /* "   [Unknown]"*/);
}

const char * const serverCompact_PrintFormat = "SELECT\n"
	ATTR_MACHINE    " AS Machine      WIDTH AUTO\n"       // 0
	ATTR_OPSYS      " AS Platform     PRINTAS PLATFORM\n" // 1
	//	"split(CondorVersion)[1] AS Condor\n"
	ATTR_NUM_DYNAMIC_SLOTS " AS Slot PRINTF %4d OR ' '\n" // 2
	ATTR_SLOT_TYPE  " AS s NOPREFIX PRINTAS SLOT_COUNT_TAG\n"  // 3
	ATTR_TOTAL_SLOT_CPUS " AS Cpus  PRINTF %4d\n"              // 4
	"TotalSlotGpus        AS Gpus  PRINTF %4d\n"              // 5
	ATTR_TOTAL_SLOT_MEMORY "/1024.0 AS  ' TotalGb' PRINTF %8.2f\n" // 6
	ATTR_MIPS       " AS ' Mips  ' PRINTF %7d OR -\n"     // 7
	ATTR_KFLOPS     " AS ' KFlops  ' PRINTF %9d OR -\n"     // 8
	"TotalLoadAvg*1.0/TotalCpus AS CpuLoad PRINTF %7.2f\n" // 9
	ATTR_ACTIVITY   " AS ST WIDTH 2 PRINTAS ACTIVITY_CODE\n" // 10
	"RecentJobStarts/20.0 AS Jobs/Min PRINTF %8.2f OR _\n" // 11
"WHERE " PMODE_SERVER_COMPACT_CONSTRAINT "\n"
"GROUP BY Machine\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetServerCols (int width, const char * & constr)
{
	if (compactMode) {
		const char * tag = "ServerCompact";
		const char * fmt = serverCompact_PrintFormat;
		if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
			fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
		}

		// adjust column offsets for these
		startdCompact_ixCol_MachineSlot = 0;
		startdCompact_ixCol_Platform = 1;
		startdCompact_ixCol_Slots = 2;
		startdCompact_ixCol_ActCode = 10;
		startdCompact_ixCol_JobStarts = 11; // double

		return;
	}
	int name_width = wide_display ? -34 : -13;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 65-width); }

	ppSetColumn(ATTR_NAME,    name_width, ! wide_display);
	ppSetColumn(ATTR_OPSYS,  -11, true);
	ppSetColumn(ATTR_ARCH,    -6, true);
	ppSetColumn(ATTR_CONDOR_LOAD_AVG, Lbl("LoadAv"), format_load_avg, NULL, 6, true);
	ppSetColumn(ATTR_MEMORY, "%8d",  true);
	ppSetColumn(ATTR_DISK, "%9d",  true);
	ppSetColumn(ATTR_MIPS, "%7d", true);
	ppSetColumn(ATTR_KFLOPS, "%9d", true);
}

//PRAGMA_REMIND("stateCompact_PrintFormat should show rolled up state and activity times")
const char * const stateCompact_PrintFormat = "SELECT\n"
	ATTR_MACHINE    " AS Machine      WIDTH AUTO\n"       // 0
	ATTR_OPSYS      " AS Platform     PRINTAS PLATFORM\n" // 1
	//	"split(CondorVersion)[1] AS Condor\n"
	ATTR_NUM_DYNAMIC_SLOTS " AS Slot PRINTF %4d OR ' '\n" // 2
	ATTR_SLOT_TYPE  " AS s NOPREFIX PRINTAS SLOT_COUNT_TAG\n"  // 3
	ATTR_TOTAL_SLOT_CPUS " AS Cpus  PRINTF %4d\n"              // 4
	"TotalSlotGpus        AS Gpus  PRINTF %4d\n"              // 5
	ATTR_TOTAL_SLOT_MEMORY "/1024.0 AS  ' TotalGb' PRINTF %8.2f\n" // 6
	"TotalLoadAvg*1.0/TotalCpus AS CpuLoad PRINTF %7.2f\n" // 7
	ATTR_ACTIVITY   " AS ST WIDTH 2 PRINTAS ACTIVITY_CODE\n" // 8
	"RecentJobStarts/20.0 AS Jobs/Min PRINTF %8.2f OR _\n" // 9
"WHERE " PMODE_STATE_COMPACT_CONSTRAINT "\n"
"GROUP BY Machine\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetStateCols (int width)
{
	if (compactMode) {
		const char * tag = "StateCompact";
		const char * fmt = stateCompact_PrintFormat;
		const char * constr = NULL;
		if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
			fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
		}

		// adjust column offsets for these
		startdCompact_ixCol_MachineSlot = 0;
		startdCompact_ixCol_Platform = 1;
		startdCompact_ixCol_Slots = 2;
		startdCompact_ixCol_ActCode = 8;
		startdCompact_ixCol_JobStarts = 9; // double

		return;
	}

	int timewid = (wide_display||(width>90)) ? 12 : 11;
	int name_width = wide_display ? -34 : -12;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, (34+3*timewid)-width); }

	ppSetColumn(ATTR_NAME, name_width, ! wide_display);
	ppSetColumn(ATTR_CPUS, Lbl("Cpu"), 3, true);
	ppSetColumn(ATTR_MEMORY, Lbl(" Mem"), 5, true);
	//ppSetColumn(ATTR_LOAD_AVG, Lbl("Load "), "%.3f", true);
	ppSetColumn(ATTR_CONDOR_LOAD_AVG, Lbl("LoadAv"), format_load_avg, NULL, 6, true);
	ppSetColumn(ATTR_KEYBOARD_IDLE, Lbl("  KbdIdle"), format_real_time, timewid, true);
	ppSetColumn(ATTR_STATE, -7,  true);
	ppSetColumn(ATTR_ENTERED_CURRENT_STATE, Lbl("  StateTime"), render_elapsed_time, "%T", timewid, true);
	ppSetColumn(ATTR_ACTIVITY, Lbl("Activ"), -5, true);
	ppSetColumn(ATTR_ENTERED_CURRENT_ACTIVITY, Lbl("  ActvtyTime"), render_elapsed_time, "%T", timewid, true);
}

const char * const claimedCompact_PrintFormat = "SELECT\n"
	ATTR_MACHINE    " AS Machine      WIDTH AUTO\n"       // 0
	ATTR_OPSYS      " AS Platform     PRINTAS PLATFORM\n" // 1
	//	"split(CondorVersion)[1] AS Condor\n"
	ATTR_NUM_DYNAMIC_SLOTS " AS Slot PRINTF %4d OR ' '\n" // 2
	ATTR_SLOT_TYPE  " AS s NOPREFIX PRINTAS SLOT_COUNT_TAG\n"  // 3
	"Child" ATTR_REMOTE_USER " AS Users PRINTAS UNIQUE\n" // 4
//	"Child" ATTR_CLIENT_MACHINE " AS Hosts PRINTAS UNIQUE\n" // 5
"WHERE " PMODE_CLAIMED_COMPACT_CONSTRAINT "\n"
"GROUP BY Machine\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetRunCols (int width)
{
	if (compactMode) {
		const char * tag = "ClaimedCompact";
		const char * fmt = claimedCompact_PrintFormat;
		const char * constr = NULL;
		if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
			fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
		}

		// adjust column offsets for these
		startdCompact_ixCol_MachineSlot = 0;
		startdCompact_ixCol_Platform = 1;
		startdCompact_ixCol_Slots = 2;

		return;
	}

	int name_width = wide_display ? -34 : -15;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 63-width); }

	ppSetColumn(ATTR_NAME,    name_width, ! wide_display);
	if (javaMode) {
		ppSetColumn(ATTR_JAVA_VENDOR,  -11, ! wide_display);
		ppSetColumn(ATTR_JAVA_VERSION, Lbl("Ver"),  -6, ! wide_display);
	} else if (vmMode) {
		ppSetColumn(ATTR_VM_TYPE, Lbl("VMType"), -6, ! wide_display);
		ppSetColumn(ATTR_VM_NETWORKING_TYPES, Lbl("Network"),  -11, ! wide_display);
	} else {
		ppSetColumn(ATTR_OPSYS,  -11, true);
		ppSetColumn(ATTR_ARCH,    -6, true);
	}
	ppSetColumn(ATTR_CONDOR_LOAD_AVG, Lbl("LoadAv"), format_load_avg, NULL, 6, true);
	ppSetColumn(ATTR_REMOTE_USER,    -20, ! wide_display);
	ppSetColumn(ATTR_CLIENT_MACHINE, -16, ! wide_display);
}

const char * const DaemonGPUs_PrintFormat = "SELECT\n"
ATTR_NAME    " AS Name      WIDTH AUTO\n"               // 0
"DetectedCPUs  AS Cores\n"  // 1
"DetectedMemory AS '  Memory' WIDTH 10 PRINTAS READABLE_MB\n" // 2
"DetectedGPUs  AS GPUs PRINTAS MEMBER_COUNT\n"                             // 3
"evalInEachContext(GlobalMemoryMB,DetectedGPUs) AS GPU-Memory PRINTAS UNIQUE OR ' '\n"
"evalInEachContext(DeviceName,DetectedGPUs) AS GPU-Name PRINTAS UNIQUE OR ' '\n"
"OfflineGPUs AS Offline-GPUs PRINTAS UNIQUE or ' '\n"
"WHERE TotalGPUs > 0\n"
"SUMMARY STANDARD\n";

const char * const DaemonGPUsCompact_PrintFormat = "SELECT\n"
ATTR_MACHINE    " AS Machine      WIDTH AUTO\n"         // 0
ATTR_OPSYS      " AS Platform     PRINTAS PLATFORM\n"   // 1 projects Arch, OpSysAndVer and OpSysShortName
ATTR_NUM_DYNAMIC_SLOTS " AS Slot PRINTF %4d OR ' '\n" // 2
ATTR_SLOT_TYPE  " AS s NOPREFIX PRINTAS SLOT_COUNT_TAG\n"  // 3
"DetectedGPUs AS GPUs  PRINTAS MEMBER_COUNT\n"          // 4
"WHERE TotalGPUs > 0\n"
"SUMMARY STANDARD\n";

const char * const SlotGPUs_PrintFormat = "SELECT\n"
ATTR_NAME    " AS Name      WIDTH AUTO\n"               // 0
ATTR_ACTIVITY " AS ST WIDTH 2 PRINTAS ACTIVITY_CODE\n"  // 1
ATTR_REMOTE_USER " AS User WIDTH AUTO PRINTF %s OR _\n" // 2
"GPUs AS GPUs PRINTF %4d\n"                             // 3
"GPUs_GlobalMemoryMB AS GPU-Memory PRINTAS READABLE_MB OR _\n"
"GPUs_DeviceName?:join(\",\",evalineachcontext(DeviceName,AvailableGPUs))?:CUDADeviceName  AS GPU-Name PRINTF %s OR *\n"
//"WHERE (PartitionableSlot && AssignedGPUs isnt undefined) ?: ((Gpus?:0) > 0)\n"
"WHERE " PMODE_GPUS_CONSTRAINT "\n"
"SUMMARY STANDARD\n";

const char * const SlotGPUsCompact_PrintFormat = "SELECT\n"
ATTR_MACHINE    " AS Machine      WIDTH AUTO\n"         // 0
ATTR_OPSYS      " AS Platform     PRINTAS PLATFORM\n"   // 1 projects Arch, OpSysAndVer and OpSysShortName
ATTR_NUM_DYNAMIC_SLOTS " AS Slot PRINTF %4d OR ' '\n" // 2
ATTR_SLOT_TYPE  " AS s NOPREFIX PRINTAS SLOT_COUNT_TAG\n"  // 3
//"Child" ATTR_REMOTE_USER " AS Users WIDTH 20 PRINTAS UNIQUE\n" //
"GPUs AS FreGPU  PRINTF %6d\n"                         // 4
"TotalSlotGPUs AS TotGPUs WIDTH 7 PRINTAS TOTAL_GPUS\n"        // 5  TOTAL_GPUS projects AssignedGPUs,OfflineGPUs
"AssignedGPUs AS Capability PRINTAS GPUS_CAPS\n"       // 6  
"AssignedGPUs AS GPUsMemory PRINTAS GPUS_MEM\n"        // 7  
"AssignedGPUs AS DeviceNames PRINTAS GPUS_NAMES\n"     // 8  
"WHERE " PMODE_GPUS_COMPACT_CONSTRAINT "\n"
"GROUP BY Machine\n"
"SUMMARY STANDARD\n";


void PrettyPrinter::ppSetGPUsCols (int /*width*/, bool daemon_ad, const char * & constr)
{
	if (compactMode) {
		const char * tag = "GPUsCompact";
		const char * fmt = daemon_ad ? DaemonGPUsCompact_PrintFormat : SlotGPUsCompact_PrintFormat;
		if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
			fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
		}

		// adjust column offsets for these
		startdCompact_ixCol_MachineSlot = 0;
		startdCompact_ixCol_Platform = 1;
		startdCompact_ixCol_Slots = 2;
		startdCompact_ixCol_FreeGPUs = 4;
		startdCompact_ixCol_TotGPUs = 5;
		startdCompact_ixCol_GPUsPropsCaps = 6;
		startdCompact_ixCol_GPUsPropsMem = 7;
		startdCompact_ixCol_GPUsPropsName = 8;

		return;
	}

	const char * tag = "GPUs";
	const char * fmt = daemon_ad ? DaemonGPUs_PrintFormat : SlotGPUs_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

}

void
printCODDetailLine( ClassAd* ad, const char* id )
{
	char* name = NULL;
	char* state = NULL;
	char* user = NULL;
	char* job_id = NULL;
	char* keyword = NULL;
	int entered_state = 0;
	int now = 0;

	ad->LookupString( ATTR_NAME, &name );
	if( ! name ) {
		name = strdup( "[???????????]" );
	}
	if( ! ad->LookupInteger(ATTR_LAST_HEARD_FROM, now) ) {
		if( ! stashed_now ) {
			stashed_now = time(nullptr);
		}
		now = stashed_now;
	}
	entered_state = getCODInt( ad, id, ATTR_ENTERED_CURRENT_STATE, 0 );
	int state_timer = now - entered_state;

	state = getCODStr( ad, id, ATTR_CLAIM_STATE, "[????????]" );
	user = getCODStr( ad, id, ATTR_REMOTE_USER, "[?????????]" );
	job_id = getCODStr( ad, id, ATTR_JOB_ID, " " );
	keyword = getCODStr( ad, id, ATTR_JOB_KEYWORD, " " );

	printf( "%-13.13s %-5.5s %-10.10s %-13.13s %-12.12s %-6.6s "
			"%-14.14s\n",  name, id, state, format_time(state_timer),
			user, job_id, keyword );

	free( name );
	free( state );
	free( user );
	free( job_id );
	free( keyword );
}

void
printCOD (ClassAd *ad)
{
	static bool first = true;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf( "\n%-13.13s %-5.5s %-10.10s %13.13s %-12.12s "
					"%-6.6s %-14.14s\n\n", ATTR_NAME, "ID",
					ATTR_CLAIM_STATE, "TimeInState ",
					ATTR_REMOTE_USER, ATTR_JOB_ID, "Keyword" );
			first = false;
		}
		StringList cod_claim_list;
		char* cod_claims = NULL;
		ad->LookupString( ATTR_COD_CLAIMS, &cod_claims );
		if( ! cod_claims ) {
			return;
		}
		cod_claim_list.initializeFromString( cod_claims );
		free( cod_claims );
		char* claim_id;
		cod_claim_list.rewind();
		while( (claim_id = cod_claim_list.next()) ) {
			printCODDetailLine( ad, claim_id );
		}
	}
}

const char * const scheddNormal_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
	"Machine        AS Machine      WIDTH AUTO\n"
	"TotalRunningJobs AS RunningJobs  PRINTF %11d\n"
	"TotalIdleJobs    AS '  IdleJobs' PRINTF %10d\n"
	"TotalHeldJobs    AS '  HeldJobs' PRINTF %10d\n"
"SUMMARY STANDARD\n";

int PrettyPrinter::ppSetScheddNormalCols (int /* width */, int & mach_width)
{
	const char * tag = "Schedd";
	const char * fmt = scheddNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

	mach_width = 12;
	int name_width = 15;
	return name_width;
}

const char * const scheddData_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
	"TransferQueueNumDownloading AS 'Download'  PRINTF %8d\n"
	"FileTransferDownloadBytesPerSecond_5m/1024.0 AS 'KB/sec(5m)' PRINTF %9.2f\n"
	"TransferQueueNumWaitingToDownload AS Waiting PRINTF %7d\n"
	"FileTransferMBWaitingToDownload AS WaitingMB PRINTF %9.2f\n"
	"TransferQueueNumUploading   AS '      Upload' PRINTF %12d\n"
	"FileTransferUploadBytesPerSecond_5m/1024.0 AS 'KB/sec(5m)' PRINTF %9.2f\n"
	"TransferQueueNumWaitingToUpload AS Waiting PRINTF %7d\n"
	"FileTransferMBWaitingToUpload AS WaitingMB PRINTF %9.2f\n"
"WHERE TransferQueueMaxUploading isnt undefined\n"
//"WHERE (TransferQueueNumDownloading+TransferQueueNumWaitingToDownload+TransferQueueNumUploading+TransferQueueNumWaitingToUpload) >= 0\n"
"SUMMARY NONE\n";

int PrettyPrinter::ppSetScheddDataCols (int /*width*/, const char * & constr)
{
	const char * tag = "ScheddData";
	const char * fmt = scheddData_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

	int name_width = 15;
	return name_width;
}

const char * const scheddRun_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
	"TotalJobAds    AS TotalJobs PRINTF %9d\n"
	"ShadowsRunning AS Shadows PRINTF %7d\n"
	"TotalSchedulerJobsRunning AS ActiveDAGs PRINTF %10d\n"
	"TotalSchedulerJobsIdle AS IdleDAGs PRINTF %8d\n"
	"RecentJobsCompleted AS RcntDone  PRINTF %8d\n"
	"RecentJobsStarted AS RcntStart PRINTF %9d\n"
"WHERE (ShadowsRunning+TotalSchedulerJobsRunning) > 0\n"
"SUMMARY NONE\n";

int PrettyPrinter::ppSetScheddRunCols (int, const char * & constr)
{
	const char * tag = "ScheddRun";
	const char * fmt = scheddRun_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

	int name_width = 15;
	return name_width;
}

const char * const submitterNormal_PrintFormat = "SELECT\n"
	"Name        AS Name         WIDTH AUTO\n"
	"Machine     AS Machine      WIDTH AUTO\n"
	"RunningJobs AS RunningJobs  PRINTF %11d\n"
	"IdleJobs    AS '  IdleJobs' PRINTF %10d\n"
	"HeldJobs    AS '  HeldJobs' PRINTF %10d\n"
"SUMMARY STANDARD\n";

int PrettyPrinter::ppSetSubmitterNormalCols (int, int & mach_width)
{
	const char * tag = "Submitter";
	const char * fmt = submitterNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

	mach_width = 12;
	int name_width = 15;
	return name_width;
}

void PrettyPrinter::ppSetCollectorNormalCols (int width)
{
	int name_width = wide_display ? -34 : -28;
	int mach_width = wide_display ? -34 : -18;
	if (width > 79 && ! wide_display) { 
		int wid = width - (28+18+11+8+10+4);
		wid = MIN(wid,50);
		int nw = MIN(20, wid*2/3);
		int nm = MIN(30, wid - nw);
		name_width -= nw;
		mach_width -= nm;
	}

	ppSetColumn(ATTR_NAME,    name_width, ! wide_display);
	ppSetColumn(ATTR_MACHINE, mach_width, ! wide_display);
	ppSetColumn(ATTR_RUNNING_JOBS, "%11d", true);
	ppSetColumn(ATTR_IDLE_JOBS,    "%8d", true);
	ppSetColumn(ATTR_NUM_HOSTS_TOTAL,    "%10d", true);
}

const char * const masterNormal_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
//	"CondorPlatform AS Platform     WIDTH -16 PRINTAS CONDOR_PLATFORM\n"
	"CondorVersion  AS Version      WIDTH -14 PRINTAS CONDOR_VERSION\n"
	"DetectedCpus   AS Cpus         WIDTH 4 PRINTF %4d OR ??\n"
	"DetectedMemory AS '  Memory'   WIDTH 10 PRINTAS READABLE_MB OR ??\n"
	"DaemonStartTime AS '   Uptime' WIDTH 13 %T PRINTAS ELAPSED_TIME\n"
"SUMMARY NONE\n";

int PrettyPrinter::ppSetMasterNormalCols(int)
{
	const char * tag = "Master";
	const char * fmt = masterNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return 12;
}

const char * const startDaemonNormal_PrintFormat = "SELECT\n"
"Name           AS Name         WIDTH AUTO\n"
"join(\"_\",ARCH,OPSYSANDVER) AS Platform  WIDTH AUTO\n"
"CondorVersion  AS Version      WIDTH AUTO PRINTAS CONDOR_VERSION\n"
"DetectedCpus   AS Cpus         WIDTH 4 PRINTF %4d OR ??\n"
"DetectedMemory AS '  Memory'   WIDTH 10 PRINTAS READABLE_MB OR ??\n"
"DetectedGPUs   AS GPUs         WIDTH 5 PRINTAS MEMBER_COUNT OR ' '\n"
"DaemonStartTime AS '   Uptime' WIDTH 13 %T PRINTAS ELAPSED_TIME\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetStartDaemonCols(int, const char * & constr )
{
	const char * tag = "StartD";
	const char * fmt = startDaemonNormal_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
}

void PrettyPrinter::ppSetCkptSrvrNormalCols (int width)
{
	int name_width = wide_display ? -34 : -28;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 50-width); }

	ppSetColumn(ATTR_NAME, name_width, ! wide_display);
	ppSetColumn(ATTR_DISK, Lbl("AvailDisk"), "%9d", true);
	ppSetColumn(ATTR_SUBNET, "%-11s", !wide_display);
}


void PrettyPrinter::ppSetStorageNormalCols (int width)
{
	int name_width = wide_display ? -34 : -30;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 48-width); }

	ppSetColumn(ATTR_NAME,  name_width, ! wide_display);
	ppSetColumn(ATTR_DISK, Lbl("AvailDisk"), "%9d", true);
	ppSetColumn(ATTR_SUBNET, "%-11s", !wide_display);
}

const char * const defragNormal_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
	"MachinesDraining AS Draining PRINTF %8d\n"
	"MachinesDrainingPeak AS '    Peak' PRINTF %8d\n"
	"DrainedMachines AS TotalDrained PRINTF %12d\n"
"SUMMARY NONE\n";

int PrettyPrinter::ppSetDefragNormalCols (int)
{
	const char * tag = "Defrag";
	const char * fmt = defragNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return 12;
}

const char * const accountingNormal_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
	"Priority       AS Priority     PRINTF %8.2f\n"
	"PriorityFactor AS PrioFactor   PRINTF %10.2f\n"
	"ResourcesUsed  AS ResInUse     PRINTF %8d\n"
	"WeightedAccumulatedUsage AS WeightedUsage WIDTH 13 PRINTF %13.2f\n"
	"LastUsageTime  AS '  LastUsage' WIDTH 12 PRINTAS ELAPSED_TIME\n"
"SUMMARY NONE\n";

int PrettyPrinter::ppSetAccountingNormalCols(int)
{
	const char * tag = "Accounting";
	const char * fmt = accountingNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return 12;
}

void PrettyPrinter::ppSetGridNormalCols (int width)
{
	int name_width = -34;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 41-width); }

	ppSetColumn(ATTR_NAME, name_width, ! wide_display);
	ppSetColumn("NumJobs", "%7d", true);
	ppSetColumn("SubmitsAllowed", Lbl("Allowed"), "%7d", true);
	ppSetColumn("SubmitsWanted", Lbl(" Wanted"), "%7d", true);
	ppSetColumn(ATTR_RUNNING_JOBS, "%11d", true);
	ppSetColumn(ATTR_IDLE_JOBS,    "%8d", true);
}

const char * const negotiatorNormal_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
	"LastNegotiationCycleEnd0  AS LastCycleEnd WIDTH 12 PRINTAS DATE\n"
	"LastNegotiationCycleDuration0 AS (Sec) PRINTF %5d\n"
	"LastNegotiationCycleCandidateSlots0 AS '  Slots' PRINTF %7d\n"
	"LastNegotiationCycleActiveSubmitterCount0 AS Submitrs PRINTF %8d\n"
	"LastNegotiationCycleNumSchedulers0 AS Schedds PRINTF %7d\n"
	"LastNegotiationCycleNumIdleJobs0 AS '   Jobs' PRINTF %7d\n"
	"LastNegotiationCycleMatches0 AS Matches PRINTF %7d\n"
	"LastNegotiationCycleRejections0 AS Rejections PRINTF %10d\n"
"SUMMARY NONE\n";

int PrettyPrinter::ppSetNegotiatorNormalCols (int)
{
	const char * tag = "Negotiator";
	const char * fmt = negotiatorNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return 12;
}

// Annex names are limited to 27 characters by the width of the client token
// that we embed them in.  With the new tag-on-creation, this may no longer
// be necessary, but it's convenient.
const char * const annexInstance_PrintFormat = "SELECT\n"
	// ATTR_ANNEX_NAME " AS 'Annex missing Instance' WIDTH -27\n"
	"EC2InstanceID AS 'Instance ID' WIDTH 19\n"
	ATTR_ANNEX_NAME " AS 'not in Annex'\n"
	ATTR_GRID_JOB_STATUS " AS 'Status'\n"
	ATTR_EC2_STATUS_REASON_CODE " AS 'Reason (if known)' PRINTF %s OR -\n"
	// There's not much reason to include the SSH key if we don't include
	// the address.  Unfortunately, Machine is much too large to fit.
	// ATTR_MACHINE " AS 'Machine' WIDTH 18\n"
	// ATTR_EC2_KEY_PAIR " AS 'SSH Key'\n"
	"SUMMARY NONE\n";

int PrettyPrinter::ppSetAnnexInstanceCols( int /* width */, const char * & constr ) {
	const char * tag = "AnnexInstance";
	const char * fmt = annexInstance_PrintFormat;
	if( set_status_print_mask_from_stream( fmt, false, & constr ) < 0 ) {
		fprintf( stderr, "Internal error: default %s print-format is invalid!\n", tag );
	}

	// FIXME: TJ?
	return 15;
}

/*
We can't use the AttrListPrintMask here, because the AttrList does not actually contain
the MyType and TargetType fields that we want to display. 
These are actually contained in the ClassAd.
*/

const char *
formatAdType (const char * type, Formatter &)
{
	static char temp[19];
	if ( ! type || ! type[0]) return "None";
	strncpy(temp, type, sizeof(temp));
	temp[sizeof(temp)-1] = 0;
	return temp;
}

void PrettyPrinter::ppSetAnyNormalCols()
{
	ppSetColumn(ATTR_MY_TYPE,     formatAdType, -18, true, CallInvalidField);
	ppSetColumn(ATTR_TARGET_TYPE, formatAdType, -18, true, CallInvalidField);
	ppSetColumn(ATTR_NAME, wide_display ? "%-41s" : "%-41.41s", ! wide_display, ShortInvalidField /*"[???]"*/);
}

void
printVerbose (ClassAd &ad, classad::References * attrs)
{
	std::string output;
	output.reserve(cchReserveForPrintingAds);
	if (attrs) {
		sPrintAdAttrs(output, ad, *attrs);
	} else {
		sPrintAd(output, ad);
	}

	output += "\n";
	fputs(output.c_str(), stdout);
}

void
printXML (ClassAd &ad, classad::References * attrs)
{
	classad::ClassAdXMLUnParser  unparser;
	std::string output;
	output.reserve(cchReserveForPrintingAds);

	unparser.SetCompactSpacing(false);
	if (attrs) {
		unparser.Unparse(output, &ad, *attrs);
	} else {
		unparser.Unparse(output, &ad);
	}

	output += "\n";
	fputs(output.c_str(), stdout);
	return;
}

void
printJSON (ClassAd &ad, bool first_ad, classad::References * attrs)
{
	classad::ClassAdJsonUnParser  unparser;
	std::string output;
	output.reserve(cchReserveForPrintingAds);

	if ( first_ad ) {
		output += "[\n";
	} else {
		output += ",\n";
	}
	if (attrs) {
		unparser.Unparse(output, &ad, *attrs);
	} else {
		unparser.Unparse(output, &ad);
	}

	output += "\n";
	fputs(output.c_str(), stdout);
	return;
}

void
printNewClassad (ClassAd &ad, bool first_ad, classad::References * attrs)
{
	classad::ClassAdUnParser  unparser;
	std::string output;
	output.reserve(cchReserveForPrintingAds);

	if ( first_ad ) {
		output += "{\n";
	} else {
		output += ",\n";
	}
	if (attrs) {
		unparser.Unparse(output, &ad, *attrs);
	} else {
		unparser.Unparse(output, &ad);
	}

	output += "\n";
	fputs(output.c_str(), stdout);
	return;
}

void PrettyPrinter::printCustom( ClassAd * ad )
{
	(void) pm.display (stdout, ad, targetAd);
}

int ppAdjustProjection(void*pv, int index, Formatter * fmt, const char * attr)
{
	struct _adjust_widths_info * pi = (struct _adjust_widths_info *)pv;
	classad::References * proj = pi->proj;
	if (attr) {
		proj->insert(attr);
		if (MATCH == strcasecmp(attr, ATTR_NAME)) {
			pi->ixCol_Name = index;
			if (fmt->options & FormatOptionSpecial001) {
				// special001 already set means FitToName
				fmt->width = 12;  // HACK! to test autowidth code for startd normal
				fmt->options |= FormatOptionAutoWidth;
			} else {
				fmt->options |= FormatOptionSpecial001;
				if (pi->name_width) {
					fmt->width = pi->name_width;
					if ( ! pi->wide_display) fmt->options &= ~FormatOptionAutoWidth;
					fmt->options |= (pi->name_flags & FormatOptionAutoWidth);
				}
			}
		}
		else if (MATCH == strcasecmp(attr, ATTR_MACHINE)) {
			pi->ixCol_Machine = index;
			fmt->options |= FormatOptionSpecial002;
			if (pi->machine_width) {
				fmt->width = pi->machine_width;
				fmt->options &= ~FormatOptionAutoWidth;
				fmt->options |= (pi->machine_flags & FormatOptionAutoWidth);
			}
		}
	}
	if (fmt->sf) {
		const CustomFormatFnTable GlobalFnTable = getGlobalPrintFormatTable();
		const CustomFormatFnTable * pFnTable = &GlobalFnTable;
		if (pFnTable) {
			const CustomFormatFnTableItem * ptable = pFnTable->pTable;
			for (int ii = 0; ii < (int)pFnTable->cItems; ++ii) {
				if ((StringCustomFormat)ptable[ii].cust == fmt->sf) {
					const char * pszz = ptable[ii].extra_attribs;
					if (pszz) {
						size_t cch = strlen(pszz);
						while (cch > 0) {
							proj->insert(pszz);
							pszz += cch+1; cch = strlen(pszz);
						}
					}
					break;
				}
			}
		}
	}
	return 0;
}

void PrettyPrinter::ppInitPrintMask(ppOption pps, classad::References & proj, const char * & constr, bool no_pr_files)
{
	if (using_print_format) {
		return;
	}

	pm.SetAutoSep(NULL, " ", NULL, "\n");
	//pm.SetAutoSep(NULL, " (", ")", "\n"); // for debugging, delimit the field data explicitly
	bool is_piped = false;
	int display_width = getDisplayWidth(&is_piped)-1;
	if (forced_display_width || ( ! wide_display && ! is_piped) ) {
		pm.SetOverallWidth(display_width);
	}

	// If setting a 'normal' output, check to see if there is a user-defined normal output
	if ( ! no_pr_files) {
		std::string param_name;
		auto_free_ptr pf_file(param(paramNameFromPPMode(param_name)));
		if (pf_file) {
			struct stat stat_buff;
			if (0 != stat(pf_file.ptr(), &stat_buff)) {
				// do nothing, this is not an error.
			} else if (set_status_print_mask_from_stream(pf_file, true, &constr) < 0) {
				fprintf(stderr, "Warning: default %s select file '%s' is invalid\n", param_name.c_str(), pf_file.ptr());
			} else {
				// we got the format already, just return.
				return;
			}
		}
	}

	int name_width = 0;
	int machine_width = 0;
	int name_flags =  wide_display ? FormatOptionAutoWidth : 0;
	int machine_flags = name_flags;

	switch (pps) {
		case PP_SLOTS_NORMAL:
		if (absentMode) {
			ppSetStartdAbsentCols();
		} else if(offlineMode) {
			ppSetStartdOfflineCols();
		} else if (compactMode && ! (vmMode || javaMode)) {
			ppSetStartdCompactCols(display_width, machine_width, constr);
			machine_flags = FormatOptionAutoWidth;
		} else {
			ppSetStartdNormalCols(display_width);
		}
		break;

		case PP_SLOTS_SERVER:
		ppSetServerCols(display_width, constr);
		break;

		case PP_SLOTS_RUN:
		ppSetRunCols(display_width);
		break;

		case PP_STARTD_GPUS:
		case PP_SLOTS_GPUS:
		ppSetGPUsCols(display_width, (pps==PP_STARTD_GPUS), constr);
		break;

		case PP_STARTDAEMON:
		ppSetStartDaemonCols(display_width, constr);
		break;

		case PP_SLOTS_COD:
			//PRAGMA_REMIND("COD format needs conversion to ppSetCodCols")
		//ppSetCODNormalCols(display_width);
		break;

		case PP_SLOTS_STATE:
		ppSetStateCols(display_width);
		break;

		case PP_SCHEDD_NORMAL:
		name_width = ppSetScheddNormalCols(display_width, machine_width);
		machine_flags = name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 35;
		break;

		case PP_SCHEDD_DATA:
		name_width = ppSetScheddDataCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 8+9+7+12+8+9+5;
		break;

		case PP_SCHEDD_RUN:
		name_width = ppSetScheddRunCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 35;
		break;

		case PP_NEGOTIATOR_NORMAL:
		name_width = ppSetNegotiatorNormalCols(display_width);
		name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 12+4+8+7+8+7;
		break;

		case PP_SUBMITTER_NORMAL:
		name_width = ppSetSubmitterNormalCols(display_width, machine_width);
		machine_flags = name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 35;
		break;

		case PP_MASTER_NORMAL:
		name_width = ppSetMasterNormalCols(display_width);
		name_flags = FormatOptionAutoWidth;
		break;

		case PP_COLLECTOR_NORMAL:
		ppSetCollectorNormalCols(display_width);
		break;

		case PP_CKPT_SRVR_NORMAL:
		ppSetCkptSrvrNormalCols(display_width);
		break;

		case PP_STORAGE_NORMAL:
		ppSetStorageNormalCols(display_width);
		break;

		case PP_DEFRAG_NORMAL:
		name_width = ppSetDefragNormalCols(display_width);
		name_flags = FormatOptionAutoWidth;
		break;

		case PP_ACCOUNTING_NORMAL:
		name_width = ppSetAccountingNormalCols(display_width);
		break;

		case PP_GRID_NORMAL:
		ppSetGridNormalCols(display_width);
		break;

		case PP_GENERIC_NORMAL:
		case PP_GENERIC:
		case PP_ANY_NORMAL:
		ppSetAnyNormalCols();
		break;

		default: // some cases have nothing to setup, this is needed to prevent gcc to bitching...
		break;
	}

	struct _adjust_widths_info wid_info = { name_width, name_flags, machine_width, machine_flags, &proj, wide_display, ixCol_Name, ixCol_Machine };
	pm.adjust_formats(ppAdjustProjection, &wid_info);
}


