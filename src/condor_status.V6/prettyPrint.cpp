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
#include "condor_attributes.h"
#include "ad_printmask.h"
#include "condor_adtypes.h"
#include "condor_config.h"
#include "condor_state.h"
#include "status_types.h"
#include "totals.h"
#include "format_time.h"
#include "console-utils.h"
#include "prettyPrint.h"
#include "setflags.h"
#include <vector>

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

class VectorInputStream : public SimpleInputStream {
	std::vector<const char*>::const_iterator cur{};
	std::vector<const char*>::const_iterator endp{};
	int lines_read{0};
public:
	VectorInputStream(const std::vector<const char*> & vec) : cur(vec.begin()), endp(vec.end()) {}
	VectorInputStream(std::vector<const char*>::const_iterator _begin, std::vector<const char*>::const_iterator _end)
		: cur(_begin), endp(_end) {}
	virtual ~VectorInputStream() { }
	virtual int count_of_lines_read() { return lines_read; } // count of lines returned
	virtual const char * nextline() { // scan for and return next non-empty line
		const char * str = nullptr;
		while (cur != endp) {
			str = *cur++;
			if ( ! str || ! str[0]) continue;
			++lines_read;
			break;
		}
		return str;
	}
};

void PrettyPrinter::setPPwidth () {
	bool is_piped = false;
	int display_width = getDisplayWidth(&is_piped)-1;
	if (forced_display_width || ( ! wide_display && ! is_piped) ) {
		pm.SetOverallWidth(display_width);
	}
}

constexpr const int	FormatOptionSpecial001 = FormatOptionSpecialBase;
constexpr const int	FormatOptionSpecial002 = FormatOptionSpecialBase<<1;
//constexpr const int	FormatOptionSpecial004 = FormatOptionSpecialBase<<2;
//constexpr const int	FormatOptionSpecial008 = FormatOptionSpecialBase<<3;


void PrettyPrinter::ppDisplayHeadings(FILE* file, ClassAd *ad, const char * pszExtra)
{
	if (ad) {
		// render the first ad to a string so the column widths update
		std::string tmp;
		pm.display(tmp, ad, NULL);
	}
	if (pm.has_headings()) {
		pm.display_Headings(file);
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
			if (pm.has_headings()) {
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
			for (const auto& [attr_name, inlineExpr] : *ad) {
				if (includelist->contains(attr_name)) {
					line = attr_name;
					line += " = ";
					unp.Unparse(line, inlineExpr.materialize());
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

// TODO: How to print out the offline reasons and timestamps?
const char * const startdOffline_PrintFormat = "SELECT\n"
	ATTR_NAME                       " WIDTH -34 FIT PRINTF %s OR ??\n"
	"OfflineUniverses AS 'Offline Universes' WIDTH -42 PRINTAS STRINGS_FROM_LIST OR '  '\n"
"WHERE size(OfflineUniverses) != 0\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetStartdOfflineCols(int /*width*/, const char * & constr)
{
	const char * tag = "StartdOffline";
	const char * fmt = startdOffline_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
}

const char * const startdAbsent_PrintFormat = "SELECT\n"
	ATTR_NAME                       " WIDTH -34 FIT PRINTF %s OR ??\n"
	ATTR_OPSYS                      " WIDTH -10 OR ??\n"
	ATTR_ARCH                       " WIDTH  -8 OR ??\n"
	ATTR_LAST_HEARD_FROM  " AS 'Went Absent' WIDTH -11 PRINTAS DATE OR ??\n"
	ATTR_CLASSAD_LIFETIME " AS 'Will Forget' WIDTH -11 PRINTAS DUE_DATE ??\n"
"WHERE Absent\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetStartdAbsentCols(int /*width*/, const char * & constr)
{
	const char * tag = "StartdAbsent";
	const char * fmt = startdAbsent_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
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

const char * const startdPFV_Normal[] = { "SELECT",
	ATTR_NAME                       " WIDTH -34 FIT PRINTF %s OR ??",
	ATTR_OPSYS                      " WIDTH -10 OR ??",
	ATTR_ARCH                       " WIDTH  -8 OR ??",
	ATTR_STATE                      " WIDTH AUTO OR ??",
	ATTR_ACTIVITY                   " WIDTH  -8 PRINTAS ACTIVITY_OR_OCU OR ??",
	ATTR_CONDOR_LOAD_AVG " AS LoadAv  WIDTH   6 TRUNCATE PRINTAS LOAD_AVG OR ??",
	ATTR_MEMORY          " AS Mem     WIDTH   4 PRINTF %4d OR ??",
	ATTR_ENTERED_CURRENT_ACTIVITY " AS ActivityTime WIDTH AUTO TRUNCATE PRINTAS ACTIVITY_TIME OR -",
	"SUMMARY STANDARD"
};

const char * startdPF_ATTR_JAVA_VENDOR  = ATTR_JAVA_VENDOR  " WIDTH -10 OR ??";
const char * startdPF_ATTR_JAVA_VERSION = ATTR_JAVA_VERSION " WIDTH  -6 OR ??";
const char * startdPF_ATTR_VM_TYPE    = ATTR_VM_TYPE " AS VmType  WIDTH  -6 OR ??";
const char * startdPF_ATTR_VM_NETWORK = ATTR_VM_NETWORKING_TYPES " AS Network WIDTH  -9 OR ??";
const char * startdPF_ATTR_VM_MEMORY  = ATTR_VM_MEMORY " AS VMMem   WIDTH   5 PRINTF %4d OR ??";
const char * startdPF_ActvtyTime  = ATTR_ENTERED_CURRENT_ACTIVITY " AS '  ActvtyTime' WIDTH 12 NOPREFIX TRUNCATE PRINTAS ACTIVITY_TIME OR -";
const char * startdPF_ActivityTime = ATTR_ENTERED_CURRENT_ACTIVITY " AS ActivityTime WIDTH 12 TRUNCATE PRINTAS ACTIVITY_TIME OR -";

int ppLastColNoPrefix(void* pv, int index, Formatter * fmt, const char * /*attr*/)
{
	int * pcol_count = (int*)pv;
	if (index == *pcol_count -1) {
		fmt->options |= FormatOptionNoPrefix;
	}
	return 0;
}
int ppSetFirstColWidth(void* pv, int index, Formatter * fmt, const char * /*attr*/)
{
	if (index == 0) { fmt->width = *(int*)pv; }
	return -1;
}

void PrettyPrinter::initStartdNormalPFV(int width)
{
	pfvec.assign(std::begin(startdPFV_Normal), std::end(startdPFV_Normal));
	ASSERT(*pfvec[2] == *ATTR_OPSYS);
	ASSERT(*pfvec[3] == *ATTR_ARCH);
	ASSERT(*pfvec[7] == *ATTR_MEMORY);
	ASSERT(*pfvec[8] == *ATTR_ENTERED_CURRENT_ACTIVITY);
	if (javaMode) {
		pfvec[2] = startdPF_ATTR_JAVA_VENDOR;
		pfvec[3] = startdPF_ATTR_JAVA_VERSION;
	} else if (vmMode) {
		pfvec[2] = startdPF_ATTR_VM_TYPE;
		pfvec[3] = startdPF_ATTR_VM_NETWORK;
		pfvec[7] = startdPF_ATTR_VM_MEMORY;
	}
	if (width < 100 && ! wide_display) { pfvec[8] = startdPF_ActvtyTime; }
}

void PrettyPrinter::ppSetStartdNormalCols (int width, const char * & constr)
{
	const char * tag = javaMode ? "java" : (vmMode ? "VM" : "");
	initStartdNormalPFV(width);

	VectorInputStream stm(pfvec);
	if (set_print_mask_from_stream(stm, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
}

// adjust format mask to make the last column multi-line
int ppLastColMultiline(void* pv, int index, Formatter * fmt, const char * /*attr*/)
{
	int * pcol_count = (int*)pv;
	if (index == *pcol_count -1) {
		fmt->options |= FormatOptionMultiLine;
	}
	return 0;
}

const char * const StartdBroken_PrintFormat = "SELECT\n"
ATTR_NAME    " AS Machine      WIDTH AUTO\n"               // 0
"BrokenReasons?:BrokenSlots  AS  'Resource     Reason'     PRINTAS BROKEN_REASONS_VECTOR\n"      // 1
"WHERE size(BrokenReasons?:BrokenSlots) > 0\n"
"SUMMARY STANDARD\n";

const char * const SlotsBroken_PrintFormat = "SELECT\n"
ATTR_NAME    " AS Name      WIDTH AUTO\n"               // 0
"SlotBrokenCode AS  Code    PRINTF %4d OR ' '\n"      // 1
"SlotBrokenReason AS Reason\n"      // 2
"WHERE size(SlotBrokenReason) > 0\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetBrokenCols (int /*width*/, bool daemon_ad, const char * & constr)
{
	const char * tag = "Broken";
	const char * fmt = daemon_ad ? StartdBroken_PrintFormat : SlotsBroken_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	} else {
		int col_count = pm.ColCount();
		pm.adjust_formats(ppLastColMultiline, &col_count);
	}
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

const char * const server_PrintFormat = "SELECT\n"
	ATTR_NAME                       " WIDTH -34 FIT PRINTF %s OR ??\n"
	ATTR_OPSYS                      " WIDTH -10 OR ??\n"
	ATTR_ARCH                       " WIDTH  -8 OR ??\n"
	ATTR_CONDOR_LOAD_AVG " AS LoadAv  WIDTH   6 TRUNCATE PRINTAS LOAD_AVG OR ??\n"
	ATTR_MEMORY                     " WIDTH   8 PRINTF %8d OR ??\n"
	ATTR_DISK                       " WIDTH   9 PRINTF %9d OR ??\n"
	ATTR_MIPS                       " WIDTH   7 PRINTF %7d OR ??\n"
	ATTR_KFLOPS                     " WIDTH   9 PRINTF %9d OR ??\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetServerCols (int /*width*/, const char * & constr)
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

	const char * tag = "Server";
	const char * fmt = server_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
}

// TODO: stateCompact_PrintFormat should show rolled up state and activity times
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

const char * const startdState_PrintFormat = "SELECT\n"
	ATTR_NAME                         " WIDTH -34 FIT PRINTF %s OR ??\n"
	ATTR_CPUS                " AS Cpu           OR ??\n"
	ATTR_MEMORY           " AS ' Mem'   WIDTH   5 OR ??\n"
	ATTR_CONDOR_LOAD_AVG " AS LoadAv    WIDTH   6 TRUNCATE PRINTAS LOAD_AVG OR ??\n"
	ATTR_KEYBOARD_IDLE " AS '  KbdIdle' WIDTH  12 TRUNCATE PRINTAS TIME OR ??\n"
	ATTR_STATE                        " WIDTH  -7 OR ??\n"
	ATTR_ENTERED_CURRENT_STATE    " AS '  StateTime' WIDTH  12 TRUNCATE PRINTAS ELAPSED_TIME OR ??\n"
	ATTR_ACTIVITY           " AS Activ          OR ??\n"
	ATTR_ENTERED_CURRENT_ACTIVITY " AS '  ActvtyTime' WIDTH 12 TRUNCATE PRINTAS ELAPSED_TIME OR -\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetStateCols (int /*width*/, const char * & constr)
{
	if (compactMode) {
		const char * tag = "StateCompact";
		const char * fmt = stateCompact_PrintFormat;
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

	const char * tag = "State";
	const char * fmt = startdState_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
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

void PrettyPrinter::ppSetRunCols (int width, const char * & constr)
{
	if (compactMode) {
		const char * tag = "ClaimedCompact";
		const char * fmt = claimedCompact_PrintFormat;
		if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
			fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
		}

		// adjust column offsets for these
		startdCompact_ixCol_MachineSlot = 0;
		startdCompact_ixCol_Platform = 1;
		startdCompact_ixCol_Slots = 2;

		return;
	}

	initStartdNormalPFV(width);
	pfvec[4] = nullptr; // skip ATTR_STATE
	pfvec[5] = nullptr; // skip ATTR_ACTIVITY
	pfvec[7] = ATTR_REMOTE_USER     " WIDTH  -20 OR ??";
	pfvec[8] = ATTR_CLIENT_MACHINE  " WIDTH  -16 OR ??";

	const char * tag = "Claimed";
	VectorInputStream stm(pfvec);
	if (set_print_mask_from_stream(stm, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
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
	time_t now = 0;

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
	time_t state_timer = now - entered_state;

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
		std::string cod_claims;
		ad->LookupString( ATTR_COD_CLAIMS, cod_claims );
		if( cod_claims.empty()) {
			return;
		}
		for (const auto &claim_id : StringTokenIterator(cod_claims)) {
			printCODDetailLine( ad, claim_id.c_str());
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

int PrettyPrinter::ppSetScheddNormalCols (int /* width */, int & mach_width, const char * & constr)
{
	const char * tag = "Schedd";
	const char * fmt = scheddNormal_PrintFormat;
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
	ATTR_NAME                  " WIDTH AUTO\n"
	ATTR_MACHINE               " WIDTH AUTO\n"
	"RunningJobs AS RunningJobs  PRINTF %11d\n"
	"IdleJobs    AS '  IdleJobs' PRINTF %10d\n"
	"HeldJobs    AS '  HeldJobs' PRINTF %10d\n"
"SUMMARY STANDARD\n";

int PrettyPrinter::ppSetSubmitterNormalCols (int, int & mach_width, const char * & constr)
{
	const char * tag = "Submitter";
	const char * fmt = submitterNormal_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

	mach_width = 12;
	int name_width = 15;
	return name_width;
}

const char * const collectorNormal_PrintFormat = "SELECT FROM Collector\n"
	ATTR_NAME     " WIDTH AUTO\n"
	ATTR_MACHINE  " WIDTH AUTO\n"
	ATTR_RUNNING_JOBS                    " PRINTF %11d\n"
	ATTR_IDLE_JOBS       " AS '  IdleJobs' PRINTF %10d\n"
	ATTR_NUM_HOSTS_TOTAL                 " PRINTF %10d\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetCollectorNormalCols (int width, int & mach_width, const char * & constr)
{
	const char * tag = "Collector";
	const char * fmt = collectorNormal_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	} else {
		//int name_width = wide_display ? -34 : -28;
		mach_width = wide_display ? -34 : -18;
		if (width > 79 && ! wide_display) { 
			int wid = width - (28+18+11+8+10+4);
			wid = MIN(wid,50);
			int nw = MIN(20, wid*2/3);
			int nm = MIN(30, wid - nw);
			//name_width -= nw;
			mach_width -= nm;
		}
	}
}

const char * const masterNormal_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
//	"CondorPlatform AS Platform     WIDTH -16 PRINTAS CONDOR_PLATFORM\n"
	"CondorVersion  AS Version      WIDTH -14 PRINTAS CONDOR_VERSION\n"
	"DetectedCpus   AS Cpus         WIDTH 4 PRINTF %4d OR ??\n"
	"DetectedMemory AS '  Memory'   WIDTH 10 PRINTAS READABLE_MB OR ??\n"
	"DaemonStartTime AS '   Uptime' WIDTH 13 %T PRINTAS ELAPSED_TIME\n"
"SUMMARY NONE\n";

int PrettyPrinter::ppSetMasterNormalCols(int, const char * & constr)
{
	const char * tag = "Master";
	const char * fmt = masterNormal_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return 12;
}

const char * const startDaemonNormal_PrintFormat = "SELECT\n"
"Name           AS Name         WIDTH AUTO\n"
"OpSys          AS Platform     WIDTH AUTO PRINTAS PLATFORM\n"
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

const char * const startdUsingLVM_PrintFormat = "SELECT\n"
ATTR_NAME "                                                   AS HOST       WIDTH AUTO\n"
ATTR_LVM_BACKING_STORE "                                      AS DEVICE     WIDTH AUTO OR ??\n"
ATTR_LVM_USE_THIN_PROVISION "=?=true ? \"thin \" : \"thick\"  AS PROVISION  WIDTH    9 PRINTF %-9s\n"
"max({(LvmDetectedDisk?:0 - LvmNonCondorDiskUsage?:0), 0})    AS '    DISK' WIDTH    9 PRINTAS READABLE_BYTES\n"
ATTR_LVM_USE_LOOPBACK "=?=true ? \" true\" : \"false\"        AS LOOPBACK   WIDTH    8 PRINTF %8s\n"
"WHERE " PMODE_STARTD_USING_LVM_CONSTRAINT "\n"
"SUMMARY NONE\n";

void PrettyPrinter::ppSetStartdLvmCols( int /*width*/, const char * & constr )
{
	const char * tag = "StartdLvm";
	const char * fmt = startdUsingLVM_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
}

const char * const slotLvUsage_PrintFormat = "SELECT\n"
ATTR_NAME "                                                   AS NAME       WIDTH AUTO\n"
ATTR_LVM_USE_THIN_PROVISION "=?=true ? \"thin \" : \"thick\"  AS PROVISION  WIDTH    9 PRINTF %-9s\n"
ATTR_DISK "                                                   AS ALLOCATED  WIDTH    9 PRINTAS READABLE_KB\n"
"(DiskUsage?:0 / real(Disk)) * 100                            AS '  USAGE'  WIDTH AUTO PRINTF '%3.2f%%'\n"
"WHERE " PMODE_SLOT_LV_USAGE_CONSTRAINT "\n"
"SUMMARY NONE\n";

void PrettyPrinter::ppSetSlotLvUsageCols( int /*width*/, const char * & constr )
{
	const char * tag = "SlotLvUsage";
	const char * fmt = slotLvUsage_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
}

const char * const ckptServer_PrintFormat = "SELECT FROM CkptServer\n"
	ATTR_NAME        "        WIDTH -34 FIT OR ??\n"
	ATTR_DISK " AS AvailDisk  WIDTH    9 PRINTF %-9s OR ??\n"
	ATTR_SUBNET             " WIDTH -11 PRINTF -11s OR ??\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetCkptSrvrNormalCols (int /*width*/, const char * & constr)
{
	const char * tag = "CkptServer";
	const char * fmt = ckptServer_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
}

const char * const storage_PrintFormat = "SELECT FROM Storage\n"
	ATTR_NAME        "        WIDTH -34 FIT OR ??\n"
	ATTR_DISK " AS AvailDisk  WIDTH    9 PRINTF %-9s OR ??\n"
	ATTR_SUBNET             " WIDTH -11 PRINTF -11s OR ??\n"
"SUMMARY STANDARD\n";


void PrettyPrinter::ppSetStorageNormalCols (int /*width*/, const char * & constr)
{
	const char * tag = "CkptServer";
	const char * fmt = storage_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
}

const char * const defragNormal_PrintFormat = "SELECT FROM Defrag\n"
	"Name           AS Name         WIDTH AUTO\n"
	"MachinesDraining AS Draining PRINTF %8d\n"
	"MachinesDrainingPeak AS '    Peak' PRINTF %8d\n"
	"DrainedMachines AS TotalDrained PRINTF %12d\n"
"SUMMARY NONE\n";

int PrettyPrinter::ppSetDefragNormalCols (int, const char * & constr)
{
	const char * tag = "Defrag";
	const char * fmt = defragNormal_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return 12;
}

const char * const accountingNormal_PrintFormat = "SELECT FROM Accounting\n"
	"Name           AS Name         WIDTH AUTO\n"
	"Priority       AS Priority     PRINTF %8.2f\n"
	"PriorityFactor AS PrioFactor   PRINTF %10.2f\n"
	"ResourcesUsed  AS ResInUse     PRINTF %8d\n"
	"WeightedAccumulatedUsage AS WeightedUsage WIDTH 13 PRINTF %13.2f\n"
	"LastUsageTime  AS '  LastUsage' WIDTH 12 PRINTAS ELAPSED_TIME\n"
"SUMMARY NONE\n";

int PrettyPrinter::ppSetAccountingNormalCols(int, const char * & constr)
{
	const char * tag = "Accounting";
	const char * fmt = accountingNormal_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return 12;
}

const char * const gridNormal_PrintFormat = "SELECT FROM Grid\n"
	ATTR_NAME                 " WIDTH -34 FIT OR ??\n"
	"NumJobs                    WIDTH   7 PRINTF %7d OR ??\n"
	"SubmitsAllowed AS Allowed  WIDTH   7 PRINTF %7d OR ??\n"
	"SubmitsWanted AS ' Wanted' WIDTH   7 PRINTF %7d OR ??\n"
	"RunningJobs                WIDTH  11 PRINTF %11d OR ??\n"
	"IdleJobs                   WIDTH   8 PRINTF %8d OR ??\n"
"SUMMARY STANDARD\n";

void PrettyPrinter::ppSetGridNormalCols (int /*width*/, const char * & constr)
{
	const char * tag = "Grid";
	const char * fmt = gridNormal_PrintFormat;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
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

int PrettyPrinter::ppSetNegotiatorNormalCols (int, const char * & constr)
{
	const char * tag = "Negotiator";
	const char * fmt = negotiatorNormal_PrintFormat;
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

const char * const adtype_PrintFormat = "SELECT FROM Any\n"
	ATTR_MY_TYPE     "?:\"None\" AS MyType     WIDTH -18\n"
	ATTR_TARGET_TYPE "?:\"None\" AS TargetType WIDTH -18\n"
	ATTR_NAME                                " WIDTH -41 FIT OR ?\n"
"SUMMARY NONE\n";

void PrettyPrinter::ppSetAnyNormalCols(int,  const char * & constr)
{
	const char * tag = "Any";
	const char * fmt = adtype_PrintFormat;
	if( set_status_print_mask_from_stream( fmt, false, & constr ) < 0 ) {
		fprintf( stderr, "Internal error: default %s print-format is invalid!\n", tag );
	}
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
			ppSetStartdAbsentCols(display_width, constr);
		} else if(offlineMode) {
			ppSetStartdOfflineCols(display_width, constr);
		} else if (compactMode && ! (vmMode || javaMode)) {
			ppSetStartdCompactCols(display_width, machine_width, constr);
			machine_flags = FormatOptionAutoWidth;
		} else {
			ppSetStartdNormalCols(display_width, constr);
			name_flags = FormatOptionAutoWidth;
			width_of_fixed_cols = 61;
		}
		break;

		case PP_SLOTS_SERVER:
		ppSetServerCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 65;
		break;

		case PP_SLOTS_RUN:
		ppSetRunCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 63;
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
		ppSetStateCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 34 + 3*12;
		break;

		case PP_STARTD_BROKEN:
		case PP_SLOTS_BROKEN:
		ppSetBrokenCols(display_width, (pps==PP_STARTD_BROKEN), constr);
		break;

		case PP_SCHEDD_NORMAL:
		name_width = ppSetScheddNormalCols(display_width, machine_width, constr);
		machine_flags = name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 35;
		break;

		case PP_SCHEDD_DATA:
		name_width = ppSetScheddDataCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 8+9+7+9+12+9+7+9;
		break;

		case PP_SCHEDD_RUN:
		name_width = ppSetScheddRunCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 35;
		break;

		case PP_NEGOTIATOR_NORMAL:
		name_width = ppSetNegotiatorNormalCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 12+5+7+8+7+7+7+10;
		break;

		case PP_SUBMITTER_NORMAL:
		name_width = ppSetSubmitterNormalCols(display_width, machine_width, constr);
		machine_flags = name_flags = FormatOptionAutoWidth;
		width_of_fixed_cols = 35;
		break;

		case PP_MASTER_NORMAL:
		name_width = ppSetMasterNormalCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		break;

		case PP_COLLECTOR_NORMAL:
		ppSetCollectorNormalCols(display_width, machine_width, constr);
		break;

		case PP_CKPT_SRVR_NORMAL:
		ppSetCkptSrvrNormalCols(display_width, constr);
		break;

		case PP_STORAGE_NORMAL:
		ppSetStorageNormalCols(display_width, constr);
		break;

		case PP_DEFRAG_NORMAL:
		name_width = ppSetDefragNormalCols(display_width, constr);
		name_flags = FormatOptionAutoWidth;
		break;

		case PP_ACCOUNTING_NORMAL:
		name_width = ppSetAccountingNormalCols(display_width, constr);
		break;

		case PP_GRID_NORMAL:
		ppSetGridNormalCols(display_width, constr);
		break;

		case PP_SLOTS_LV_USAGE:
		ppSetSlotLvUsageCols(display_width, constr);
		break;

		case PP_STARTD_LVM:
		ppSetStartdLvmCols(display_width, constr);
		break;

		case PP_GENERIC_NORMAL:
		case PP_GENERIC:
		case PP_ANY_NORMAL:
		ppSetAnyNormalCols(display_width, constr);
		break;

		default: // some cases have nothing to setup, this is needed to prevent gcc to bitching...
		break;
	}

	struct _adjust_widths_info wid_info = { name_width, name_flags, machine_width, machine_flags, &proj, wide_display, ixCol_Name, ixCol_Machine };
	pm.adjust_formats(ppAdjustProjection, &wid_info);
}


