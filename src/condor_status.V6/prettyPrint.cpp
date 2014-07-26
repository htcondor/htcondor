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
extern ClassAd *targetAd;

extern char *format_time( int );

static int stashed_now = 0;

void printStartdNormal 	(ClassAd *, bool first);
void printStartdAbsent 	(ClassAd *, bool first);
void printScheddNormal 	(ClassAd *, bool first);

#ifdef HAVE_EXT_POSTGRESQL
void printQuillNormal 	(ClassAd *);
#endif /* HAVE_EXT_POSTGRESQL */

void printScheddSubmittors(ClassAd *, bool first);
void printMasterNormal 	(ClassAd *, bool first);
void printCollectorNormal (ClassAd *, bool first);
void printCkptSrvrNormal(ClassAd *, bool first);
void printStorageNormal (ClassAd *, bool first);
void printNegotiatorNormal (ClassAd *, bool first);
void printGridNormal 	(ClassAd *, bool first);
void printAnyNormal 	(ClassAd *, bool first);
void printServer 		(ClassAd *, bool first);
void printRun    		(ClassAd *, bool first);
void printCOD    		(ClassAd *);
void printState			(ClassAd *, bool first);
void printVerbose   	(ClassAd *);
void printXML       	(ClassAd *, bool first_ad, bool last_ad);
void printCustom    	(ClassAd *);

static const char *formatActivityTime( int , AttrList* , Formatter &);
static const char *formatDueDate( int , AttrList* , Formatter &);
//static const char *formatElapsedDate( int , AttrList* , Formatter &);
static const char *formatElapsedTime( int , AttrList* , Formatter &);
static const char *formatRealTime( int , AttrList * , Formatter &);
static const char *formatRealDate( int , AttrList * , Formatter &);
//static const char *formatFloat (double, AttrList *, Formatter &);
static const char *formatLoadAvg (double, AttrList *, Formatter &);

static void ppInit()
{
	pm.SetAutoSep(NULL, " ", NULL, "\n");
	//pm.SetAutoSep(NULL, " (", ")", "\n"); // for debugging, delimit the field data explicitly
}

#ifdef AD_PRINTMASK_V2
enum ivfield {
	BlankInvalidField = 0,
	WideInvalidField = 1,
	ShortInvalidField = 2,
	CallInvalidField = 3,   // call the formatter function to generate invalid fields.  only valid when there is a format function.
};
class Lbl {
public:
	explicit Lbl(const char * ll) : m_lbl(ll) {};
	operator const char*() const { return m_lbl; }
private:
	const char * m_lbl;
};
#else
const char *StdInvalidField = "[?]"; // fill field with ??? surrounded by [], if 
#endif

#ifdef AD_PRINTMASK_V2
#else
// construct a string if the form "[????]" that is the given width
// into the supplied buffer, output not to exceed max_buf, and
// if the buffer or width is < 3, then return "?" instead of "[?]"
static const char * ppMakeFieldofQuestions(int width, char * buf, int buf_size)
{
	if (buf_size < 2)
		return "";

	int cq = width;
	if (cq < 0) 
		cq = 0-width;

	cq = MIN(buf_size-1, cq);

	if (cq < 3) {
		strcpy(buf, "?");
	} else {
		buf[cq] = 0;
		buf[--cq] = ']';
		while (--cq) buf[cq] = '?';
		buf[0] = '[';
	}
	return buf;
}

static const char *MinInvalidField = "[?]"; // fill field with ??? surrounded by [], if 

static const char * ppMakeInvalidField(int width, const char * alt, char * buf, int buf_size)
{
	if (alt == MinInvalidField) {
		if (invalid_fields_empty) 
			return "";
		// make a field-width string of "[????]" for the alt string.
		return ppMakeFieldofQuestions(width, buf, buf_size);
	} else if ( ! alt) {
		return "";
	}
	return alt;
}
#endif

#ifdef AD_PRINTMASK_V2

static int ppWidthOpts(int width, int truncate)
{
	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	return opts;
}

static int ppAltOpts(ivfield alt)
{
	int opts = 0;
	if (alt == WideInvalidField) {
		if (!invalid_fields_empty) opts |= AltQuestion | AltWide;
	} else if (alt == ShortInvalidField) {
		if (!invalid_fields_empty) opts |= AltQuestion;
	} else if (alt == CallInvalidField) {
		opts |= FormatOptionAlwaysCall;
	} else if (alt != BlankInvalidField) {
		opts |= AltFixMe;
	}
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
	if (width == 11 && fmt.IsNumber() && (fmt.Is(formatElapsedTime) || fmt.Is(formatRealTime))) {
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
#else

static void ppSetColumn(const char * label, const char * attr, int width, bool truncate, const char * alt)
{
	pm_head.Append(label ? label : attr);

	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;

	char altq[22];
	alt = ppMakeInvalidField(width, alt, altq, sizeof(altq));

	pm.registerFormat("%v", width, opts, attr, alt);
}


static void ppSetColumn(const char * label, const char * attr, const char * fmt, bool truncate = true, const char * alt = NULL)
{
	pm_head.Append(label ? label : attr);

	int width = 0;
	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;

	char altq[22];
	alt = ppMakeInvalidField(width, alt, altq, sizeof(altq));

	pm.registerFormat(fmt, width, opts, attr, alt);
}

static void ppSetColumn(const char * label, const char * attr, const CustomFormatFn & fmt, int width, bool truncate = true, const char * alt = NULL)
{
	pm_head.Append(label ? label : attr);

	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;

	if (width == 11 && fmt.IsNumber() && (fmt.Is(formatElapsedTime) || fmt.Is(formatRealTime))) {
		opts |= FormatOptionNoPrefix;
		width = 12;
	}

	char altq[22];
	alt = ppMakeInvalidField(width, alt, altq, sizeof(altq));

	pm.registerFormat(NULL, width, opts, fmt, attr, alt);
}

static void ppSetColumn(const char * label, const char * attr, const CustomFormatFn & fmt, const char * print, int width, bool truncate = true, const char * alt = NULL)
{
	pm_head.Append(label ? label : attr);

	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;

	char altq[22];
	alt = ppMakeInvalidField(width, alt, altq, sizeof(altq));

	pm.registerFormat(print, width, opts, fmt, attr, alt);
}

#endif


static void ppDisplayHeadings(FILE* file, ClassAd *ad, const char * pszExtra)
{
	if (ad) {
		// render the first ad to a string so the column widths update
		char * tmp = pm.display(ad, NULL);
		delete [] tmp;
	}
	if (pm.has_headings()) {
		pm.display_Headings(file);
	} else {
		pm.display_Headings(file, pm_head);
	}
	if (pszExtra)
		printf("%s", pszExtra);
}

void
prettyPrint (ClassAdList &adList, TrackTotals *totals)
{
	ppOption pps = using_print_format ? PP_CUSTOM : ppStyle;
	ClassAd	*ad;
	int     classad_index;
	int     last_classad_index;
	bool    fPrintHeadings = pm.has_headings() || (pm_head.Length() > 0);

	classad_index = 0;
	last_classad_index = adList.Length() - 1;
	adList.Open();
	while ((ad = adList.Next())) {
		if (!wantOnlyTotals) {
			switch (pps) {
			  case PP_STARTD_NORMAL:
				if (absentMode) {
					printStartdAbsent (ad, (classad_index == 0));
				} else {
					printStartdNormal (ad, (classad_index == 0));
				}
				break;

			  case PP_STARTD_SERVER:
				printServer (ad, (classad_index == 0));
				break;

			  case PP_STARTD_RUN:
				printRun (ad, (classad_index == 0));
				break;

			  case PP_STARTD_COD:
				printCOD (ad);
				break;

			  case PP_STARTD_STATE:
				printState(ad, (classad_index == 0));
				break;

#ifdef HAVE_EXT_POSTGRESQL
			  case PP_QUILL_NORMAL:
				printQuillNormal (ad);
				break;
#endif /* HAVE_EXT_POSTGRESQL */

			  case PP_SCHEDD_NORMAL:
				printScheddNormal (ad, (classad_index == 0));
				break;

			  case PP_NEGOTIATOR_NORMAL:
				printNegotiatorNormal (ad, (classad_index == 0));
				break;


			  case PP_SCHEDD_SUBMITTORS:
				printScheddSubmittors (ad, (classad_index == 0));
				break;

			  case PP_VERBOSE:
				printVerbose (ad);
				break;

			  case PP_XML:
				printXML (ad, (classad_index == 0),
					(classad_index == last_classad_index));
				break;

			  case PP_MASTER_NORMAL:
				printMasterNormal(ad, (classad_index == 0));
				break;

			  case PP_COLLECTOR_NORMAL:
				printCollectorNormal(ad, (classad_index == 0));
				break;

			  case PP_CKPT_SRVR_NORMAL:
				printCkptSrvrNormal(ad, (classad_index == 0));
				break;

			  case PP_STORAGE_NORMAL:
				printStorageNormal(ad, (classad_index == 0));
				break;

			  case PP_GRID_NORMAL:
				printGridNormal(ad, (classad_index == 0));
				break;

			  case PP_GENERIC_NORMAL:
			  case PP_GENERIC:
			  case PP_ANY_NORMAL:
				printAnyNormal(ad, (classad_index == 0));
				break;

			  case PP_CUSTOM:
				  // hack: print a single item to a string, then discard the string
				  // this makes sure that the headings line up correctly over the first
				  // line of data.
				if (fPrintHeadings) {
					char * tmp = pm.display(ad, targetAd);
					delete [] tmp;
					if (pm.has_headings()) {
						if ( ! (pmHeadFoot & HF_NOHEADER))
							pm.display_Headings(stdout);
					} else {
						pm.display_Headings(stdout, pm_head);
					}
					fPrintHeadings = false;
				}
				printCustom (ad);
				break;

			  case PP_NOTSET:
				fprintf (stderr, "Error:  pretty printing set to PP_NOTSET.\n");
				exit (1);

			  default:
				fprintf (stderr, "Error:  Unknown pretty print option.\n");
				exit (1);			
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
	if (adList.MyLength() > 0 && totals) totals->displayTotals(stdout, 20);
}


void
printStartdAbsent (ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME, -34, ! wide_display);
		ppSetColumn(ATTR_OPSYS, -10, true);
		ppSetColumn(ATTR_ARCH, -10, true);
		ppSetColumn(ATTR_LAST_HEARD_FROM, Lbl("Went Absent"), formatRealDate, -10, true);
		ppSetColumn(ATTR_CLASSAD_LIFETIME, Lbl("Will Forget"), formatDueDate, -10, true);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
	return;
}

void
printStartdNormal (ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME, wide_display ? -34 : -18, ! wide_display);

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
		pm.registerFormat(NULL, 12, FormatOptionAutoWidth | (wide_display ? 0 : FormatOptionNoPrefix) | AltFixMe,
			formatActivityTime, ATTR_ENTERED_CURRENT_ACTIVITY /* "   [Unknown]"*/);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}


void
printServer (ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME,    wide_display ? -34 : -13, ! wide_display);
		ppSetColumn(ATTR_OPSYS,  -11, true);
		ppSetColumn(ATTR_ARCH,    -6, true);
		ppSetColumn(ATTR_LOAD_AVG, Lbl("LoadAv"), formatLoadAvg, NULL, 6, true);
		ppSetColumn(ATTR_MEMORY, "%8d",  true);
		ppSetColumn(ATTR_DISK, "%9d",  true);
		ppSetColumn(ATTR_MIPS, "%7d", true);
		ppSetColumn(ATTR_KFLOPS, "%9d", true);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}

void
printState (ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		int timewid = wide_display ? 12 : 11;
		ppSetColumn(ATTR_NAME,    wide_display ? -34 : -12, ! wide_display);
		ppSetColumn(ATTR_CPUS, Lbl("Cpu"), 3, true);
		ppSetColumn(ATTR_MEMORY, Lbl(" Mem"), 5, true);
		//ppSetColumn(ATTR_LOAD_AVG, Lbl("Load "), "%.3f", true);
		ppSetColumn(ATTR_LOAD_AVG, Lbl("LoadAv"), formatLoadAvg, NULL, 6, true);
		ppSetColumn(ATTR_KEYBOARD_IDLE, Lbl("  KbdIdle"), formatRealTime, timewid, true);
		ppSetColumn(ATTR_STATE, -7,  true);
		ppSetColumn(ATTR_ENTERED_CURRENT_STATE, Lbl("  StateTime"), formatElapsedTime, timewid, true);
		ppSetColumn(ATTR_ACTIVITY, Lbl("Activ"), -5, true);
		ppSetColumn(ATTR_ENTERED_CURRENT_ACTIVITY, Lbl("  ActvtyTime"), formatElapsedTime, timewid, true);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}

void
printRun (ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME,    wide_display ? -34 : -15, ! wide_display);
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

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
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

void
printScheddNormal (ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME,    wide_display ? -34 : -20, ! wide_display);
		ppSetColumn(ATTR_MACHINE, wide_display ? -34 : -10, ! wide_display);
		ppSetColumn(ATTR_TOTAL_RUNNING_JOBS, "%16d", true);
		ppSetColumn(ATTR_TOTAL_IDLE_JOBS,    "%13d", true);
		ppSetColumn(ATTR_TOTAL_HELD_JOBS,    "%14d", true);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}


void
printScheddSubmittors (ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME,    wide_display ? -34 : -28, ! wide_display);
		ppSetColumn(ATTR_MACHINE, wide_display ? -34 : -18, ! wide_display);
		ppSetColumn(ATTR_RUNNING_JOBS, "%11d", true);
		ppSetColumn(ATTR_IDLE_JOBS,    "%8d", true);
		ppSetColumn(ATTR_HELD_JOBS,    "%8d", true);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}

void
printCollectorNormal(ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME,    wide_display ? -34 : -28, ! wide_display);
		ppSetColumn(ATTR_MACHINE, wide_display ? -34 : -18, ! wide_display);
		ppSetColumn(ATTR_RUNNING_JOBS, "%11d", true);
		ppSetColumn(ATTR_IDLE_JOBS,    "%8d", true);
		ppSetColumn(ATTR_NUM_HOSTS_TOTAL,    "%10d", true);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}

void
printMasterNormal(ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME, -20, false);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}


void
printCkptSrvrNormal(ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME,    wide_display ? -34 : -28, ! wide_display);
		ppSetColumn(ATTR_DISK, Lbl("AvailDisk"), "%9d", true);
		ppSetColumn(ATTR_SUBNET, "%-11s", !wide_display);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}


void
printStorageNormal(ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME,    wide_display ? -34 : -30, ! wide_display);
		ppSetColumn(ATTR_DISK, Lbl("AvailDisk"), "%9d", true);
		ppSetColumn(ATTR_SUBNET, "%-11s", !wide_display);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}

void
printGridNormal(ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME, -34, ! wide_display);
		ppSetColumn("NumJobs", "%7d", true);
		ppSetColumn("SubmitsAllowed", Lbl("Allowed"), "%7d", true);
		ppSetColumn("SubmitsWanted", Lbl(" Wanted"), "%7d", true);
		ppSetColumn(ATTR_RUNNING_JOBS, "%11d", true);
		ppSetColumn(ATTR_IDLE_JOBS,    "%8d", true);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}

void
printNegotiatorNormal(ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_NAME, wide_display ? -32 : -20, ! wide_display);
		ppSetColumn(ATTR_MACHINE, wide_display ? -32 : -20, ! wide_display);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
}

/*
We can't use the AttrListPrintMask here, because the AttrList does not actually contain
the MyType and TargetType fields that we want to display. 
These are actually contained in the ClassAd.
*/

const char *
formatAdType (const char * type, AttrList *, Formatter &)
{
	static char temp[19];
	if ( ! type || ! type[0]) return "None";
	strncpy(temp, type, sizeof(temp));
	temp[sizeof(temp)-1] = 0;
	return temp;
}

void
printAnyNormal(ClassAd *ad, bool first)
{
	if (first) {
		ppInit();
		ppSetColumn(ATTR_MY_TYPE,     formatAdType, -18, true, CallInvalidField);
		ppSetColumn(ATTR_TARGET_TYPE, formatAdType, -18, true, CallInvalidField);
		ppSetColumn(ATTR_NAME, wide_display ? "%-41s" : "%-41.41s", ! wide_display, ShortInvalidField /*"[???]"*/);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
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

static const char *
formatLoadAvg (double fl, AttrList *, Formatter &)
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

static const char *
formatActivityTime ( int actvty, AttrList *al, Formatter &)
{
	int now = 0;
	if (al->LookupInteger(ATTR_MY_CURRENT_TIME, now)
		|| al->LookupInteger(ATTR_LAST_HEARD_FROM, now)) {
		actvty = now - actvty;
		return format_time(actvty);
	}
	return "   [Unknown]";
}

static const char *
formatDueDate (int dt, AttrList *al, Formatter &)
{
	int now;
	if (al->LookupInteger(ATTR_LAST_HEARD_FROM , now)) {
		return format_date(now + dt);
	}
	return "";
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

static const char *
formatElapsedTime (int tm, AttrList *al , Formatter &)
{
	int now;
	if (al->LookupInteger(ATTR_LAST_HEARD_FROM , now)) {
		return format_time(now - tm);
	}
	return "";
}

static const char *
formatRealDate (int dt, AttrList * , Formatter &)
{
	return format_date(dt);
}

static const char *
formatRealTime( int t , AttrList * , Formatter &)
{
	return format_time( t );
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
	{ "ACTIVITY_TIME", NULL, formatActivityTime, ATTR_LAST_HEARD_FROM "\0" ATTR_MY_CURRENT_TIME "\0"  },
	{ "DATE",         NULL, formatRealDate, NULL },
	{ "ELAPSED_TIME", ATTR_LAST_HEARD_FROM, formatElapsedTime, ATTR_LAST_HEARD_FROM "\0" },
	{ "LOAD_AVG",     ATTR_LOAD_AVG, formatLoadAvg, NULL },
	{ "TIME",         ATTR_KEYBOARD_IDLE, formatRealTime, NULL },
};
static const CustomFormatFnTable LocalPrintFormatsTable = SORTED_TOKENER_TABLE(LocalPrintFormats);
const CustomFormatFnTable * getCondorStatusPrintFormats() { return &LocalPrintFormatsTable; }
