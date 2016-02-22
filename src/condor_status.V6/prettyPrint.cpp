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
#include "format_time.h"
#include "string_list.h"
#include "metric_units.h"

#define USE_LATE_PROJECTION 1

extern ppOption				ppStyle;
extern AttrListPrintMask 	pm;
extern printmask_headerfooter_t pmHeadFoot;
extern bool using_print_format; // hack for now so we can get standard totals when using -print-format
extern List<const char>     pm_head; // The list of headings for the mask entries
extern int					wantOnlyTotals;
extern bool wide_display; // when true, don't truncate field data
extern bool invalid_fields_empty; // when true, print "" for invalid data instead of "[?]"

extern bool javaMode;
extern bool vmMode;
extern bool absentMode;
extern bool offlineMode;
extern ClassAd *targetAd;

extern char *format_time( int );
extern int set_status_print_mask_from_stream (const char * streamid, bool is_filename, const char ** pconstraint);

static int stashed_now = 0;

void ppInitPrintMask(ppOption pps, classad::References & proj);
const CustomFormatFnTable * getCondorStatusPrintFormats();

#ifdef HAVE_EXT_POSTGRESQL
void printQuillNormal 	(ClassAd *);
#endif /* HAVE_EXT_POSTGRESQL */

void printCOD    		(ClassAd *);
void printVerbose   	(ClassAd *);
void printXML       	(ClassAd *, bool first_ad, bool last_ad);
void printCustom    	(ClassAd *);

static bool renderActivityTime(long long & atime, AttrList* , Formatter &);
static bool renderDueDate(long long & atime, AttrList* , Formatter &);
static bool renderElapsedTime(long long & etime, AttrList* , Formatter &);
static bool renderVersion(std::string & str, AttrList*, Formatter & fmt);
static bool renderPlatform(std::string & str, AttrList*, Formatter & fmt);
static const char* formatVersion(const char * condorver, Formatter &);
static const char *formatRealTime( long long , Formatter &);
static const char *formatRealDate( long long , Formatter &);
//static const char *formatFloat (double, AttrList *, Formatter &);
static const char *formatLoadAvg (double, Formatter &);
static const char *formatStringsFromList( const classad::Value &, Formatter & );

static const char *
format_readable_mb(const classad::Value &val, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi * 1024.0 * 1024.0;
	} else if (val.IsRealValue(kb)) {
		kb *= 1024.0 * 1024.0;
	} else {
		return "        ";
	}
	return metric_units(kb);
}

static const char *
format_readable_kb(const classad::Value &val, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi*1024.0;
	} else if (val.IsRealValue(kb)) {
		kb *= 1024.0;
	} else {
		return "        ";
	}
	return metric_units(kb);
}

#ifdef WIN32
int getConsoleWindowSize(int * pHeight = NULL) {
	CONSOLE_SCREEN_BUFFER_INFO ws;
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ws)) {
		if (pHeight)
			*pHeight = (int)(ws.srWindow.Bottom - ws.srWindow.Top)+1;
		return (int)ws.dwSize.X;
	}
	return 80;
}
#else
#include <sys/ioctl.h> 
int getConsoleWindowSize(int * pHeight = NULL) {
    struct winsize ws; 
	if (0 == ioctl(0, TIOCGWINSZ, &ws)) {
		//printf ("lines %d\n", ws.ws_row); 
		//printf ("columns %d\n", ws.ws_col); 
		if (pHeight)
			*pHeight = (int)ws.ws_row;
		return (int) ws.ws_col;
	}
	return 80;
}
#endif

int  forced_display_width = 0;
int getDisplayWidth() {
	if (forced_display_width <= 0) {
		int width = getConsoleWindowSize();
		if (width <= 0)
			return wide_display ? 1024 : 80;
		return width;
	}
	return forced_display_width;
}

void setPPwidth () { 
	if (wide_display || forced_display_width) {
		pm.SetOverallWidth(getDisplayWidth()-1);
	}
}

enum ivfield {
	BlankInvalidField = 0,
	WideInvalidField = 1,
	ShortInvalidField = 2,
	CallInvalidField = 3,   // call the formatter function to generate invalid fields.  only valid when there is a format function.
	FitToName = 4,
};
class Lbl {
public:
	explicit Lbl(const char * ll) : m_lbl(ll) {};
	operator const char*() const { return m_lbl; }
private:
	const char * m_lbl;
};

static int ppWidthOpts(int width, int truncate)
{
	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	return opts;
}

static int ppAltOpts(ivfield alt_in)
{
	ivfield alt = (ivfield)(alt_in & 3);
	int opts = 0;
	if (alt == WideInvalidField) {
		if (!invalid_fields_empty) opts |= AltQuestion | AltWide;
	} else if (alt == ShortInvalidField) {
		if (!invalid_fields_empty) opts |= AltQuestion;
	} else if (alt == CallInvalidField) {
		opts |= FormatOptionAlwaysCall;
	} else if (alt != BlankInvalidField) {
		//opts |= AltFixMe;
	}
	if (alt_in & FitToName) { opts |= FormatOptionSpecial001; }
	return opts;
}

static void ppSetColumnFormat(const char * print, int width, bool truncate, ivfield alt, const char * attr)
{
	int opts = ppWidthOpts(width, truncate) | ppAltOpts(alt);
	pm.registerFormat(print, width, opts, attr);
}

static void ppSetColumnFormat(const CustomFormatFn & fmt, const char * print, int width, bool truncate, ivfield alt, const char * attr)
{
	int opts = ppWidthOpts(width, truncate) | ppAltOpts(alt);
	if (width == 11 && fmt.IsNumber() && (fmt.Is(renderElapsedTime) || fmt.Is(formatRealTime))) {
		opts |= FormatOptionNoPrefix;
		width = 12;
	}
	pm.registerFormat(print, width, opts, fmt, attr);
}

// ------ helpers
//
static void ppSetColumn(const char * attr, const Lbl & label, int width, bool truncate, ivfield alt = WideInvalidField)
{
	pm_head.Append(label);
	ppSetColumnFormat("%v", width, truncate, alt, attr);
}
static void ppSetColumn(const char * attr, int width, bool truncate, ivfield alt = WideInvalidField)
{
	pm_head.Append(attr);
	ppSetColumnFormat("%v", width, truncate, alt, attr);
}


static void ppSetColumn(const char * attr, const Lbl & label, const char * print, bool truncate, ivfield alt = WideInvalidField)
{
	pm_head.Append(label);
	ppSetColumnFormat(print, 0, truncate, alt, attr);
}
static void ppSetColumn(const char * attr, const char * print, bool truncate, ivfield alt = WideInvalidField)
{
	pm_head.Append(attr);
	ppSetColumnFormat(print, 0, truncate, alt, attr);
}


static void ppSetColumn(const char * attr, const Lbl & label, const CustomFormatFn & fmt, int width, bool truncate, ivfield alt = WideInvalidField)
{
	pm_head.Append(label);
	ppSetColumnFormat(fmt, NULL, width, truncate, alt, attr);
}
static void ppSetColumn(const char * attr, const CustomFormatFn & fmt, int width, bool truncate, ivfield alt = WideInvalidField)
{
	pm_head.Append(attr);
	ppSetColumnFormat(fmt, NULL, width, truncate, alt, attr);
}

static void ppSetColumn(const char * attr, const Lbl & label, const CustomFormatFn & fmt, const char * print, int width, bool truncate = true, ivfield alt = WideInvalidField)
{
	pm_head.Append(label);
	ppSetColumnFormat(fmt, print, width, truncate, alt, attr);
}
/* not currently used
static void ppSetColumn(const char * attr, const CustomFormatFn & fmt, const char * print, int width, bool truncate = true, ivfield alt = WideInvalidField)
{
	pm_head.Append(attr);
	ppSetColumnFormat(fmt, print, width, truncate, alt, attr);
}
*/


static void ppDisplayHeadings(FILE* file, ClassAd *ad, const char * pszExtra)
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

void prettyPrintInitMask(classad::References & proj)
{
	//bool old_headings = (ppStyle == PP_STARTD_COD) || (ppStyle == PP_QUILL_NORMAL);
	bool long_form = (ppStyle == PP_VERBOSE) || (ppStyle == PP_XML);
	bool custom = (ppStyle == PP_CUSTOM);
	if ( ! using_print_format && ! wantOnlyTotals && ! custom && ! long_form) {
		ppInitPrintMask(ppStyle, proj);
	}
}

// this struct used pass args to the ppAdjustProjection or ppAdjustNameWidth callback
struct _adjust_widths_info {
	int name_width;
	int machine_width;
	classad::References * proj;
};

int ppAdjustNameWidth(void*pv, int /*index*/, Formatter * fmt, const char * /*attr*/)
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
		}
	}
	return 0;
}

void
prettyPrint (ClassAdList &adList, TrackTotals *totals)
{
	ClassAd	*ad;
	int     classad_index = 0;
	int     num_ads = adList.Length();
	int     last_classad_index = num_ads - 1;
	int     totals_key_width = wide_display ? -1 : 20;

	ppOption pps = ppStyle;
	bool no_headings = wantOnlyTotals || (num_ads <= 0);
	bool old_headings = (pps == PP_STARTD_COD) || (pps == PP_QUILL_NORMAL);
	bool long_form = (pps == PP_VERBOSE) || (pps == PP_XML);
	const char * newline_after_headings = "\n";
	if ((pps == PP_CUSTOM) || using_print_format) {
		pps = PP_CUSTOM;
		no_headings = true;
		newline_after_headings = NULL;
		if ( ! wantOnlyTotals) {
			bool fHasHeadings = pm.has_headings() || (pm_head.Length() > 0);
			if (fHasHeadings) {
				no_headings = (pmHeadFoot & HF_NOHEADER);
			}
		}
	} else if (old_headings || long_form) {
		no_headings = true;
	} else {
		// before we print headings, adjust the width of the name column
		if (num_ads > 0) {
			struct _adjust_widths_info wid_info = { 0, 0, NULL };
			adList.Rewind();
			ad = adList.Next();
			if (ad) {
				std::string name;
				do {
					if (ad->LookupString(ATTR_NAME, name)) {
						int width = name.length();
						if (width > wid_info.name_width) wid_info.name_width = width;
					}
					if (ad->LookupString(ATTR_MACHINE, name)) {
						int width = name.length();
						if (width > wid_info.machine_width) wid_info.machine_width = width;
					}
				} while ((ad = adList.Next()));
				if (wid_info.name_width && wid_info.name_width < 16) wid_info.name_width = 16;
				if (wid_info.machine_width && wid_info.machine_width < 16) wid_info.machine_width = 10;
			}
			if ( ! wide_display && (wid_info.name_width > totals_key_width)) {
				if (pps != PP_SCHEDD_NORMAL) { totals_key_width = wid_info.name_width; }
			}
			pm.adjust_formats(ppAdjustNameWidth, &wid_info);
		}
	}
	if ( ! no_headings) {
		adList.Rewind();
		ad = adList.Next();
		// we pass an ad into ppDisplayHeadings so that it can adjust column widths.
		ppDisplayHeadings(stdout, ad, newline_after_headings);
	}

	adList.Open();
	while ((ad = adList.Next())) {
		if (!wantOnlyTotals) {
			switch (pps) {
			case PP_VERBOSE:
				printVerbose(ad);
				break;
			case PP_STARTD_COD:
				printCOD (ad);
				break;
			case PP_CUSTOM:
				printCustom (ad);
				break;
			case PP_XML:
				printXML (ad, (classad_index == 0), (classad_index == last_classad_index));
				break;
		#ifdef HAVE_EXT_POSTGRESQL
			case PP_QUILL_NORMAL:
				printQuillNormal (ad);
				break;
		#endif /* HAVE_EXT_POSTGRESQL */
			default:
				pm.display(stdout, ad);
				break;
			}
		}
		classad_index++;
		totals->update(ad);
	}
	adList.Close();

	// if there are no ads to print, but the user wanted XML output,
	// then print out the XML header and footer, so that naive XML
	// parsers won't get confused.
	if ( PP_XML == pps && 0 == classad_index ) {
		printXML (NULL, true, true);
	}

	// if totals are required, display totals
	if (classad_index > 0 && totals && totals->haveTotals()) {
		fprintf(stdout, "\n");
		totals->displayTotals(stdout, totals_key_width);
	}
}



// The strdup() make leak memory, but IsListValue() may as well?
const char *
formatStringsFromList( const classad::Value & value, Formatter & ) {
	const classad::ExprList * list = NULL;
	if( ! value.IsListValue( list ) ) {
		return "[Attribute not a list.]";
	}

	std::string prettyList;
	classad::ExprList::const_iterator i = list->begin();
	for( ; i != list->end(); ++i ) {
		classad::Value value;
		if( ! (*i)->Evaluate( value ) ) { continue; }

		std::string universeName;
		if( value.IsStringValue( universeName ) ) {
			prettyList += universeName + ", ";
		}
	}
	if( prettyList.length() > 0 ) {
		prettyList.erase( prettyList.length() - 2 );
	}

	return strdup( prettyList.c_str() );
}

void ppSetStartdOfflineCols (int /*width*/)
{
		ppSetColumn( ATTR_NAME, -34, ! wide_display );
		// A custom printer for filtering out the ints would be handy.
		ppSetColumn( "OfflineUniverses", Lbl( "Offline Universes" ),
					 formatStringsFromList, -42, ! wide_display );

		// How should I print out the offline reasons and timestamps?

}

void ppSetStartdAbsentCols (int /*width*/)
{
		ppSetColumn(ATTR_NAME, -34, ! wide_display);
		ppSetColumn(ATTR_OPSYS, -10, true);
		ppSetColumn(ATTR_ARCH, -8, true);
		ppSetColumn(ATTR_LAST_HEARD_FROM, Lbl("Went Absent"), formatRealDate, -11, true);
		ppSetColumn(ATTR_CLASSAD_LIFETIME, Lbl("Will Forget"), renderDueDate, "%Y", -11, true);
}

void ppSetStartdNormalCols (int width)
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

	//ppSetColumn(0, ATTR_LOAD_AVG, "%.3f ", false, invalid_fields_empty ? "" : "[???] ");
	//pm_head.Append(ATTR_LOAD_AVG);
	//pm_head.Append(wide_display ? ATTR_LOAD_AVG : "LoadAv");
	//pm.registerFormat("%.3f ", wide_display ? 7 : 6, FormatOptionAutoWidth, ATTR_LOAD_AVG, invalid_fields_empty ? "" : "[???] ");
	ppSetColumn(ATTR_LOAD_AVG, Lbl("LoadAv"), formatLoadAvg, NULL, 6, true);

	if (vmMode) {
		ppSetColumn(ATTR_VM_MEMORY, Lbl("VMMem"), "%4d", false);
	} else {
		ppSetColumn(ATTR_MEMORY, Lbl("Mem"), "%4d", false);
	}
	pm_head.Append(wide_display ? "ActivityTime" : "  ActvtyTime");
	pm.registerFormat("%T", 12, FormatOptionAutoWidth | (wide_display ? 0 : FormatOptionNoPrefix) | AltDash,
		renderActivityTime, ATTR_ENTERED_CURRENT_ACTIVITY /* "   [Unknown]"*/);
}


void ppSetServerCols (int width)
{
	int name_width = wide_display ? -34 : -13;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 65-width); }

	ppSetColumn(ATTR_NAME,    name_width, ! wide_display);
	ppSetColumn(ATTR_OPSYS,  -11, true);
	ppSetColumn(ATTR_ARCH,    -6, true);
	ppSetColumn(ATTR_LOAD_AVG, Lbl("LoadAv"), formatLoadAvg, NULL, 6, true);
	ppSetColumn(ATTR_MEMORY, "%8d",  true);
	ppSetColumn(ATTR_DISK, "%9d",  true);
	ppSetColumn(ATTR_MIPS, "%7d", true);
	ppSetColumn(ATTR_KFLOPS, "%9d", true);
}


void ppSetStateCols (int width)
{
	int timewid = (wide_display||(width>90)) ? 12 : 11;
	int name_width = wide_display ? -34 : -12;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, (34+3*timewid)-width); }

	ppSetColumn(ATTR_NAME, name_width, ! wide_display);
	ppSetColumn(ATTR_CPUS, Lbl("Cpu"), 3, true);
	ppSetColumn(ATTR_MEMORY, Lbl(" Mem"), 5, true);
	//ppSetColumn(ATTR_LOAD_AVG, Lbl("Load "), "%.3f", true);
	ppSetColumn(ATTR_LOAD_AVG, Lbl("LoadAv"), formatLoadAvg, NULL, 6, true);
	ppSetColumn(ATTR_KEYBOARD_IDLE, Lbl("  KbdIdle"), formatRealTime, timewid, true);
	ppSetColumn(ATTR_STATE, -7,  true);
	ppSetColumn(ATTR_ENTERED_CURRENT_STATE, Lbl("  StateTime"), renderElapsedTime, "%T", timewid, true);
	ppSetColumn(ATTR_ACTIVITY, Lbl("Activ"), -5, true);
	ppSetColumn(ATTR_ENTERED_CURRENT_ACTIVITY, Lbl("  ActvtyTime"), renderElapsedTime, "%T", timewid, true);
}


void ppSetRunCols (int width)
{
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
	ppSetColumn(ATTR_LOAD_AVG, Lbl("LoadAv"), formatLoadAvg, NULL, 6, true);
	ppSetColumn(ATTR_REMOTE_USER,    -20, ! wide_display);
	ppSetColumn(ATTR_CLIENT_MACHINE, -16, ! wide_display);
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
			stashed_now = (int)time(NULL);
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

#ifdef HAVE_EXT_POSTGRESQL
void
printQuillNormal (ClassAd *ad) {
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-10.10s %-16.16s %-18.18s\n\n",
				ATTR_NAME, ATTR_MACHINE, ATTR_QUILL_SQL_TOTAL,
				ATTR_QUILL_SQL_LAST_BATCH);
		
			alpm.registerFormat("%-20.20s ", ATTR_NAME,
													"[??????????????????] ");
			alpm.registerFormat("%-10.10s ", ATTR_MACHINE,
													"[????????] ");
			alpm.registerFormat("%16d ",ATTR_QUILL_SQL_TOTAL,
													"[??????????????] ");
			alpm.registerFormat("%18d\n",ATTR_QUILL_SQL_LAST_BATCH,
													"[???????????]\n");
			first = false;
		}

		alpm.display (stdout, ad);
	}
}
#endif /* HAVE_EXT_POSTGRESQL */

const char * const scheddNormal_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
	"Machine        AS Machine      WIDTH AUTO\n"
	"TotalRunningJobs AS RunningJobs  PRINTF %11d\n"
	"TotalIdleJobs    AS '  IdleJobs' PRINTF %10d\n"
	"TotalHeldJobs    AS '  HeldJobs' PRINTF %10d\n"
"SUMMARY NONE\n";

int ppSetScheddNormalCols (int width, int & mach_width)
{
#if 1
	const char * tag = "Schedd";
	const char * fmt = scheddNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

	int name_width = wide_display ? 36 : 28;
	mach_width = wide_display ? 32 : 15;
	if (width > 79 && ! wide_display) { 
		int wid = width - (20+10+11+10+10+4);
		wid = MIN(wid,50);
		int nw = MIN(20, wid*2/3);
		int nm = MIN(30, wid - nw);
		name_width += nw;
		mach_width += nm;
	}
	return name_width;
#else
	int name_width = wide_display ? -34 : -20;
	int mach_width = wide_display ? -34 : -10;
	if (width > 79 && ! wide_display) { 
		int wid = width - (20+10+16+13+14+4);
		wid = MIN(wid,50);
		int nw = MIN(20, wid*2/3);
		int nm = MIN(30, wid - nw);
		name_width -= nw;
		mach_width -= nm;
	}

	ppSetColumn(ATTR_NAME,    name_width, ! wide_display);
	ppSetColumn(ATTR_MACHINE, mach_width, ! wide_display);
	ppSetColumn(ATTR_TOTAL_RUNNING_JOBS, "%16d", true);
	ppSetColumn(ATTR_TOTAL_IDLE_JOBS,    "%13d", true);
	ppSetColumn(ATTR_TOTAL_HELD_JOBS,    "%14d", true);
#endif
}

const char * const submitterNormal_PrintFormat = "SELECT\n"
	"Name        AS Name         WIDTH AUTO\n"
	"Machine     AS Machine      WIDTH AUTO\n"
	"RunningJobs AS RunningJobs  PRINTF %11d\n"
	"IdleJobs    AS '  IdleJobs' PRINTF %10d\n"
	"HeldJobs    AS '  HeldJobs' PRINTF %10d\n"
"SUMMARY NONE\n";

int ppSetSubmitterNormalCols (int width, int & mach_width)
{
#if 1
	const char * tag = "Submitter";
	const char * fmt = submitterNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

	int name_width = wide_display ? 34 : 28;
	mach_width = wide_display ? 34 : 15;
	if (width > 79 && ! wide_display) { 
		int wid = width - (20+10+11+10+10+4);
		wid = MIN(wid,50);
		int nw = MIN(20, wid*2/3);
		int nm = MIN(30, wid - nw);
		name_width += nw;
		mach_width += nm;
	}
	return name_width;
#else
	int name_width = wide_display ? -34 : -28;
	int mach_width = wide_display ? -34 : -18;
	if (width > 79 && ! wide_display) { 
		int wid = width - (28+18+11+8+8+4);
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
	ppSetColumn(ATTR_HELD_JOBS,    "%8d", true);
#endif
}


void ppSetCollectorNormalCols (int width)
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

int ppSetMasterNormalCols(int width)
{
#if 1
	const char * tag = "Master";
	const char * fmt = masterNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return width - (1+14 + 1+4 + 1+10 + 1+13 +1);
#else
	int name_width = wide_display ? -34 : -28;
	ppSetColumn(ATTR_NAME, name_width, ! wide_display);

	//pm_head.Append("Version");
	//ppSetColumnFormat(formatVersion, NULL, 14, true, BlankInvalidField, "CondorVersion");
	ppSetColumn("CondorVersion", Lbl("Version"), renderVersion, NULL, -14, true);

	ppSetColumn("DetectedCpus", Lbl("Cpus"), "%4d", false);
	ppSetColumn("DetectedMemory", Lbl("Memory"), format_readable_mb, NULL, 12, false);

	ppSetColumn(ATTR_DAEMON_START_TIME, Lbl("   Uptime"), renderElapsedTime, "%T", 13, true);
#endif
}


void ppSetCkptSrvrNormalCols (int width)
{
	int name_width = wide_display ? -34 : -28;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 50-width); }

	ppSetColumn(ATTR_NAME, name_width, ! wide_display);
	ppSetColumn(ATTR_DISK, Lbl("AvailDisk"), "%9d", true);
	ppSetColumn(ATTR_SUBNET, "%-11s", !wide_display);
}


void ppSetStorageNormalCols (int width)
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

int ppSetDefragNormalCols (int width)
{
#if 1
	const char * tag = "Defrag";
	const char * fmt = defragNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return width - (1+8 + 1+8 + 1+12 +1);
#else
	int name_width = wide_display ? -34 : -30;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 48-width); }

	ppSetColumn(ATTR_NAME,  name_width, ! wide_display);
	ppSetColumn("MachinesDraining",     Lbl("Draining"), "%8d", true);
	ppSetColumn("MachinesDrainingPeak", Lbl("    Peak"), "%8d", true);
	ppSetColumn("DrainedMachines",      Lbl("TotalDrained"), "%12d", true);
#endif
}

const char * const accountingNormal_PrintFormat = "SELECT\n"
	"Name           AS Name         WIDTH AUTO\n"
	"Priority       AS Priority     PRINTF %8.2f\n"
	"PriorityFactor AS PrioFactor   PRINTF %10.2f\n"
	"ResourcesUsed  AS ResInUse     PRINTF %8d\n"
	"WeightedAccumulatedUsage AS WeightedUsage WIDTH 13 PRINTF %13.2f\n"
	"LastUsageTime  AS '  LastUsage' WIDTH 12 PRINTAS ELAPSED_TIME\n"
"SUMMARY NONE\n";

int ppSetAccountingNormalCols (int width)
{
#if 1
	const char * tag = "Accounting";
	const char * fmt = accountingNormal_PrintFormat;
	const char * constr = NULL;
	if (set_status_print_mask_from_stream(fmt, false, &constr) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	return width - (1+8 + 1+10 + 1+8 + 1+13 + 1+12 +1);
#else
	int name_width = wide_display ? -34 : -20;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 63-width); }

	ppSetColumn(ATTR_NAME,  name_width, ! wide_display);
	ppSetColumn("Priority",           Lbl("Priority"), "%8.2f", true);
	ppSetColumn("PriorityFactor",     Lbl("PrioFactor"), "%10.2f", true);
	ppSetColumn("ResourcesUsed",      Lbl("ResInUse"), "%8d", true);
	//ppSetColumn("AccumulatedUsage",
	ppSetColumn("WeightedAccumulatedUsage", Lbl("WeightedUsage"), "%13.2f", true);
	ppSetColumn("LastUsageTime", Lbl("  LastUsage"), renderElapsedTime, "%T", 12, true);
#endif
}

void ppSetGridNormalCols (int width)
{
	int name_width = wide_display ? -34 : -34;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, 41-width); }

	ppSetColumn(ATTR_NAME, name_width, ! wide_display);
	ppSetColumn("NumJobs", "%7d", true);
	ppSetColumn("SubmitsAllowed", Lbl("Allowed"), "%7d", true);
	ppSetColumn("SubmitsWanted", Lbl(" Wanted"), "%7d", true);
	ppSetColumn(ATTR_RUNNING_JOBS, "%11d", true);
	ppSetColumn(ATTR_IDLE_JOBS,    "%8d", true);
}


void ppSetNegotiatorNormalCols (int width)
{
	int name_width = wide_display ? -32 : -20;
	if (width > 79 && ! wide_display) { name_width = MAX(-40, width/3); }

	ppSetColumn(ATTR_NAME, name_width, ! wide_display);
	ppSetColumn(ATTR_MACHINE, name_width, ! wide_display);
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

void ppSetAnyNormalCols (int /*width*/)
{
	ppSetColumn(ATTR_MY_TYPE,     formatAdType, -18, true, CallInvalidField);
	ppSetColumn(ATTR_TARGET_TYPE, formatAdType, -18, true, CallInvalidField);
	ppSetColumn(ATTR_NAME, wide_display ? "%-41s" : "%-41.41s", ! wide_display, ShortInvalidField /*"[???]"*/);
}


void
printVerbose (ClassAd *ad)
{
	fPrintAd (stdout, *ad);
	fputc ('\n', stdout);	
}

void
printXML (ClassAd *ad, bool first_ad, bool last_ad)
{
	classad::ClassAdXMLUnParser  unparser;
	std::string            xml;

	if (first_ad) {
		AddClassAdXMLFileHeader(xml);
	}

	unparser.SetCompactSpacing(false);
	if ( NULL != ad ) {
		unparser.Unparse(xml, ad);
	}

	if (last_ad) {
		AddClassAdXMLFileFooter(xml);
	}

	printf("%s\n", xml.c_str());
	return;
}

void
printCustom (ClassAd *ad)
{
	(void) pm.display (stdout, ad, targetAd);
}


int ppAdjustProjection(void*pv, int /*index*/, Formatter * fmt, const char * attr)
{
	struct _adjust_widths_info * pi = (struct _adjust_widths_info *)pv;
	classad::References * proj = pi->proj;
	if (attr) {
		proj->insert(attr);
		if (MATCH == strcasecmp(attr, ATTR_NAME)) {
			fmt->options |= FormatOptionSpecial001;
			if (pi->name_width) {
				fmt->width = pi->name_width;
				if ( ! wide_display) fmt->options &= ~FormatOptionAutoWidth;
			}
		}
		else if (MATCH == strcasecmp(attr, ATTR_MACHINE)) {
			fmt->options |= FormatOptionSpecial002;
			if (pi->machine_width) {
				fmt->width = pi->machine_width;
				if ( ! wide_display) fmt->options &= ~FormatOptionAutoWidth;
			}
		}
	}
	if (fmt->sf) {
		const CustomFormatFnTable * pFnTable = getCondorStatusPrintFormats();
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

void ppInitPrintMask(ppOption pps, classad::References & proj)
{
	if (using_print_format) {
		return;
	}

	pm.SetAutoSep(NULL, " ", NULL, "\n");
	//pm.SetAutoSep(NULL, " (", ")", "\n"); // for debugging, delimit the field data explicitly
	int display_width = getDisplayWidth()-1;
	if (wide_display || forced_display_width) {
		pm.SetOverallWidth(display_width);
	}

	int name_width = 0;
	int machine_width = 0;

	switch (pps) {
		case PP_STARTD_NORMAL:
		if (absentMode) {
			ppSetStartdAbsentCols(display_width);
		} else if(offlineMode) {
			ppSetStartdOfflineCols(display_width);
		} else {
			ppSetStartdNormalCols(display_width);
		}
		break;

		case PP_STARTD_SERVER:
		ppSetServerCols(display_width);
		break;

		case PP_STARTD_RUN:
		ppSetRunCols(display_width);
		break;

		case PP_STARTD_COD:
			PRAGMA_REMIND("COD format needs conversion to ppSetCodCols")
		//ppSetCODNormalCols(display_width);
		break;

		case PP_STARTD_STATE:
		ppSetStateCols(display_width);
		break;

		case PP_QUILL_NORMAL:
		#ifdef HAVE_EXT_POSTGRESQL
			PRAGMA_REMIND("QUILL format needs conversion to ppSetQuillNormalCols")
		ppSetQuillNormalCols(display_width);
		#endif /* HAVE_EXT_POSTGRESQL */
		break;

		case PP_SCHEDD_NORMAL:
		name_width = ppSetScheddNormalCols(display_width, machine_width);
		break;

		case PP_NEGOTIATOR_NORMAL:
		ppSetNegotiatorNormalCols(display_width);
		break;

		case PP_SUBMITTER_NORMAL:
		name_width = ppSetSubmitterNormalCols(display_width, machine_width);
		break;

		case PP_MASTER_NORMAL:
		name_width = ppSetMasterNormalCols(display_width);
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
		ppSetAnyNormalCols(display_width);
		break;

		default: // some cases have nothing to setup, this is needed to prevent gcc to bitching...
		break;
	}

	struct _adjust_widths_info wid_info = { name_width, machine_width, &proj };
	pm.adjust_formats(ppAdjustProjection, &wid_info);
}


static const char *
formatLoadAvg (double fl, Formatter &)
{
	static char buf[60];
	sprintf(buf, "%.3f", fl);
	return buf;
}

#if 0 // not currently used
static const char *
formatFloat (double fl, AttrList *, Formatter & fmt)
{
	static char buf[60];
	sprintf(buf, fmt.printfFmt, fl);
	return buf;
}
#endif

static bool
renderActivityTime (long long & atime, AttrList *al, Formatter &)
{
	long long now = 0;
	if (al->LookupInteger(ATTR_MY_CURRENT_TIME, now)
		|| al->LookupInteger(ATTR_LAST_HEARD_FROM, now)) {
		atime = now - atime; // format_time
		return true; 
	}
	return false; // print "   [Unknown]"
}

static bool
renderDueDate (long long & dt, AttrList *al, Formatter &)
{
	long long now;
	if (al->LookupInteger(ATTR_LAST_HEARD_FROM , now)) {
		dt = now + dt; // format_date
		return true;
	}
	return false;
}

#if 0 // not currently used
static const char *
formatElapsedDate (int dt, AttrList *al, Formatter &)
{
	int now;
	if (al->LookupInteger(ATTR_LAST_HEARD_FROM , now)) {
		return format_date(now - dt);
	}
	return "";
}
#endif

static bool
renderElapsedTime (long long & tm, AttrList *al , Formatter &)
{
	long long now;
	if (al->LookupInteger(ATTR_LAST_HEARD_FROM, now)) {
		tm = now - tm; // format_time
		return true;
	}
	return false;
}

static const char *
formatRealDate (long long dt, Formatter &)
{
	return format_date(dt);
}

static const char *
formatRealTime(long long t, Formatter &)
{
	return format_time( t );
}

// extract version and build id from $CondorVersion string
static const char *
formatVersion(const char * condorver, Formatter & fmt)
{
	bool no_build_id = !(fmt.options & FormatOptionAutoWidth) && (fmt.width > -10 && fmt.width < 10);
	static char ret[9+12+2];
	char * r = ret;
	char * rend = ret+sizeof(ret)-2;
	const char * p = condorver;
	while (*p && *p != ' ') ++p;  // skip $CondorVersion:
	while (*p == ' ') ++p;
	while (*p && *p != ' ') {
		if (r < rend) *r++ = *p++; // copy X.Y.Z version
		else p++;
	}
	while (*p == ' ') ++p;
	while (*p && *p != ' ') ++p; // skip Feb
	while (*p == ' ') ++p;
	while (*p && *p != ' ') ++p; // skip 12
	while (*p == ' ') ++p;
	while (*p && *p != ' ') ++p; // skip 2016
	while (*p == ' ') ++p;
	// *p now points to a BuildId:, or "PRE-RELEASE"
	if (*p == 'B') {
		while (*p && *p != ' ') ++p; // skip BuildId:
		while (*p == ' ') ++p;
	}
	if (*p != '$' && ! no_build_id) {
		*r++ = '.';
		while (*p && *p != '-' && *p != ' ') {
			if (r < rend) *r++ = *p++; // copy PRE or NNNNN buildid
			else p++;
		}
	}
	*r = 0;
	return ret;
}

static bool renderVersion(std::string & str, AttrList*, Formatter & fmt)
{
	if ( ! str.empty()) {
		str = formatVersion(str.c_str(), fmt);
		return true;
	}
	return false;
}

static bool renderPlatform(std::string & str, AttrList*, Formatter & /*fmt*/)
{
	if ( ! str.empty()) {
		size_t ix = str.find_first_of(' ');
		ix = str.find_first_not_of(' ', ix);
		size_t ixe = str.find_first_of(" .$", ix);
		str = str.substr(ix, ixe - ix);
		if (str[0] == 'X') str[0] = 'x';
		ix = str.find_first_of('-');
		while (ix != std::string::npos) {
			str[ix] = '_';
			ix = str.find_first_of('-');
		}
		ix = str.find("WINDOWS_");
		if (ix != std::string::npos) { str.erase(ix+7, std::string::npos); }
		return true;
	}
	return false;
}

/*
# default condor_status output
SELECT
   Name       AS Name     WIDTH -18 TRUNCATE
   OpSys      AS OpSys    WIDTH -10
   Arch       AS Arch     WIDTH -6
   State      AS State    WIDTH -9
   Activity   AS Activity WIDTH -8
   LoadAvg    AS LoadAv             PRINTAS LOAD_AVG
   Memory     AS Mem                PRINTF "%4d"
   EnteredCurrentActivity AS "  ActvtyTime" NOPREFIX PRINTAS ACTIVITY_TIME
SUMMARY STANDARD
*/

// !!! ENTRIES IN THIS TABLE MUST BE SORTED BY THE FIRST FIELD !!
static const CustomFormatFnTableItem LocalPrintFormats[] = {
	{ "ACTIVITY_TIME", ATTR_ENTERED_CURRENT_ACTIVITY, "%T", renderActivityTime, ATTR_LAST_HEARD_FROM "\0" ATTR_MY_CURRENT_TIME "\0"  },
	{ "CONDOR_PLATFORM", "CondorPlatform", 0, renderPlatform, ATTR_ARCH "\0" ATTR_OPSYS "\0" },
	{ "CONDOR_VERSION", "CondorVersion", 0, renderVersion, NULL },
	{ "DATE",         NULL, 0, formatRealDate, NULL },
	{ "DUE_DATE",     ATTR_CLASSAD_LIFETIME, "%Y", renderDueDate, ATTR_LAST_HEARD_FROM "\0" },
	{ "ELAPSED_TIME", ATTR_LAST_HEARD_FROM, "%T", renderElapsedTime, ATTR_LAST_HEARD_FROM "\0" },
	{ "LOAD_AVG",     ATTR_LOAD_AVG, 0, formatLoadAvg, NULL },
	{ "READABLE_KB",  ATTR_DISK, 0, format_readable_kb, NULL },
	{ "READABLE_MB",  ATTR_MEMORY, 0, format_readable_mb, NULL },
	{ "STRINGS_FROM_LIST", NULL, 0, formatStringsFromList, NULL },
	{ "TIME",         ATTR_KEYBOARD_IDLE, 0, formatRealTime, NULL },
};
static const CustomFormatFnTable LocalPrintFormatsTable = SORTED_TOKENER_TABLE(LocalPrintFormats);
const CustomFormatFnTable * getCondorStatusPrintFormats() { return &LocalPrintFormatsTable; }

