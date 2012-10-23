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
//static const char *formatFloat (float, AttrList *, Formatter &);
static const char *formatLoadAvg (float, AttrList *, Formatter &);

static void ppInit()
{
	pm.SetAutoSep(NULL, " ", NULL, "\n");
	//pm.SetAutoSep(NULL, " (", ")", "\n");
}

const char *StdInvalidField = "[?]"; // fill field with ??? surrounded by [], if 

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

static const char * ppMakeInvalidField(int width, const char * alt, char * buf, int buf_size)
{
	if (alt == StdInvalidField) {
		if (invalid_fields_empty) 
			return "";
		// make a field-width string of "[????]" for the alt string.
		return ppMakeFieldofQuestions(width, buf, buf_size);
	} else if ( ! alt) {
		return "";
	}
	return alt;
}

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


static void ppSetColumn(const char * label, const char * attr, IntCustomFmt fmt, int width, bool truncate = true, const char * alt = NULL)
{
	pm_head.Append(label ? label : attr);

	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;

	if (((fmt == formatElapsedTime) || (fmt == formatRealTime)) && (width == 11)) {
		opts |= FormatOptionNoPrefix;
		width = 12;
	}

	char altq[22];
	alt = ppMakeInvalidField(width, alt, altq, sizeof(altq));

	pm.registerFormat(NULL, width, opts, fmt, attr, alt);
}

static void ppSetColumn(const char * label, const char * attr, StringCustomFmt fmt, int width, bool truncate = true, const char * alt = NULL)
{
	pm_head.Append(label ? label : attr);

	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;

	char altq[22];
	alt = ppMakeInvalidField(width, alt, altq, sizeof(altq));

	pm.registerFormat(NULL, width, opts, fmt, attr, alt);
}

static void ppSetColumn(const char * label, const char * attr, FloatCustomFmt fmt, const char * print, int width, bool truncate = true, const char * alt = NULL)
{
	pm_head.Append(label ? label : attr);

	int opts = FormatOptionAutoWidth;
	if (0 == width) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;
	else if ( ! truncate) opts |= FormatOptionAutoWidth | FormatOptionNoTruncate;

	char altq[22];
	alt = ppMakeInvalidField(width, alt, altq, sizeof(altq));

	pm.registerFormat(print, width, opts, fmt, attr, alt);
}

static void ppDisplayHeadings(FILE* file, ClassAd *ad, const char * pszExtra)
{
	if (ad) {
		// render the first ad to a string so the column widths update
		char * tmp = pm.display(ad, NULL);
		delete [] tmp;
	}
	pm.display_Headings(file, pm_head);
	if (pszExtra)
		printf(pszExtra);
}

void
prettyPrint (ClassAdList &adList, TrackTotals *totals)
{
	ClassAd	*ad;
	int     classad_index;
	int     last_classad_index;
	bool    fPrintHeadings = pm_head.Length() > 0;

	classad_index = 0;
	last_classad_index = adList.Length() - 1;
	adList.Open();
	while ((ad = adList.Next())) {
		if (!wantOnlyTotals) {
				// CRUFT: Before 7.3.2, submitter ads had a MyType of
				//   "Scheduler". The only way to tell the difference
				//   was that submitter ads didn't have ATTR_NUM_USERS.
				//   Coerce MyStype to "Submitter" for ads coming from
				//   these older schedds. This used to be done inside
				//   the ClassAd library.
				//   Before 7.7.3, submitter ads for parallel universe
				//   jobs had a MyType of "Scheduler".
			if ( !strcmp( ad->GetMyTypeName(), SCHEDD_ADTYPE ) &&
				 !ad->LookupExpr( ATTR_NUM_USERS ) ) {
				ad->SetMyTypeName( SUBMITTER_ADTYPE );
			}
			switch (ppStyle) {
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
					pm.display_Headings(stdout, pm_head);
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
	if ( PP_XML == ppStyle && 0 == classad_index ) {
		printXML (NULL, true, true);
	}

	// if totals are required, display totals
	if (adList.MyLength() > 0 && totals) totals->displayTotals(stdout, 20);
}


void
printStartdAbsent (ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME, -34, ! wide_display, StdInvalidField);
		ppSetColumn(0,ATTR_OPSYS, -10, true, StdInvalidField);
		ppSetColumn(0,ATTR_ARCH, -10, true, StdInvalidField);
		ppSetColumn("Went Absent", ATTR_LAST_HEARD_FROM, formatRealDate, -10, true);
		ppSetColumn("Will Forget", ATTR_CLASSAD_LIFETIME, formatDueDate, -10, true);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;
	int    now;
	int	   actvty;

	if( ad ) {
		if( first ) {
			printf( "\n%-34.34s %-10.10s %-10.10s %-11.11s %-11.11s\n\n",
				ATTR_NAME, ATTR_OPSYS, ATTR_ARCH,
				"Went Absent", "Will Forget" );

			alpm.registerFormat( "%-34.34s ", ATTR_NAME, "[???] " );
			alpm.registerFormat( "%-10.10s " , ATTR_OPSYS, "[???] " );
			alpm.registerFormat( "%-10.10s ", ATTR_ARCH, "[???] " );

			first = false;
			}

		alpm.display( stdout, ad );

		if( ad->LookupInteger( ATTR_LAST_HEARD_FROM, now ) ) {
			printf( "%-11.11s", format_date( now ) );

			if( ad->LookupInteger( ATTR_CLASSAD_LIFETIME, actvty ) ) {
				printf( " %-11.11s", format_date( now + actvty ) );
				}
			}

		printf( "\n" );
		}
	}
#endif
	return;
}

void
printStartdNormal (ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0, ATTR_NAME, wide_display ? -34 : -18, ! wide_display, StdInvalidField);

		if (javaMode) {
			ppSetColumn(0, ATTR_JAVA_VENDOR, -10, ! wide_display, StdInvalidField);
			ppSetColumn("Ver", ATTR_JAVA_VERSION, -6, ! wide_display, StdInvalidField);
		} else if (vmMode) {
			ppSetColumn("VmType", ATTR_VM_TYPE, -6, true, StdInvalidField);
			ppSetColumn("Network", ATTR_VM_NETWORKING_TYPES, -9, true, StdInvalidField);
		}else {
			ppSetColumn(0, ATTR_OPSYS, -10, true, StdInvalidField);
			ppSetColumn(0, ATTR_ARCH, -6, true, StdInvalidField);
		}
		ppSetColumn(0, ATTR_STATE,    -9, true, StdInvalidField);
		ppSetColumn(0, ATTR_ACTIVITY, -8, true, StdInvalidField);

		//ppSetColumn(0, ATTR_LOAD_AVG, "%.3f ", false, invalid_fields_empty ? "" : "[???] ");
		//pm_head.Append(ATTR_LOAD_AVG);
		//pm_head.Append(wide_display ? ATTR_LOAD_AVG : "LoadAv");
		//pm.registerFormat("%.3f ", wide_display ? 7 : 6, FormatOptionAutoWidth, ATTR_LOAD_AVG, invalid_fields_empty ? "" : "[???] ");
		ppSetColumn("LoadAv",ATTR_LOAD_AVG, formatLoadAvg, NULL, 6, true, StdInvalidField);

		if (vmMode) {
			ppSetColumn("VMMem", ATTR_VM_MEMORY, "%4d", false, StdInvalidField);
		} else {
			ppSetColumn("Mem", ATTR_MEMORY, "%4d", false, StdInvalidField);
		}
		pm_head.Append(wide_display ? "ActivityTime" : "  ActvtyTime");
		pm.registerFormat(NULL, 12, FormatOptionAutoWidth | (wide_display ? 0 : FormatOptionNoPrefix), formatActivityTime, ATTR_ENTERED_CURRENT_ACTIVITY, "   [Unknown]");

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;
	int    now;
	int	   actvty;

	const char *opsys_attr, *arch_attr, *mem_attr;
	const char *opsys_name, *arch_name, *mem_name;
 
	mem_name = "Mem";
	mem_attr = ATTR_MEMORY;

    /* 
     * The absent mode would share no more than four lines of code with
     * the other two modes, so just handle it separately.
     */
    if( absentMode ) {
        if( ad ) {
            if( first ) {
                printf( "\n%-34.34s %-10.10s %-10.10s %-11.11s %-11.11s\n\n",
                        ATTR_NAME, ATTR_OPSYS, ATTR_ARCH,
                        "Went Absent", "Will Forget" );

                alpm.registerFormat( "%-34.34s ", ATTR_NAME, "[???] " );
                alpm.registerFormat( "%-10.10s " , ATTR_OPSYS, "[???] " );
                alpm.registerFormat( "%-10.10s ", ATTR_ARCH, "[???] " );

                first = false;
            }

            alpm.display( stdout, ad );

            if( ad->LookupInteger( ATTR_LAST_HEARD_FROM, now ) ) {
                printf( "%-11.11s", format_date( now ) );

                if( ad->LookupInteger( ATTR_CLASSAD_LIFETIME, actvty ) ) {
                    printf( " %-11.11s", format_date( now + actvty ) );
                }
            }

            printf( "\n" );
        }

        return;
    }

	if(javaMode) {
		opsys_name = opsys_attr = ATTR_JAVA_VENDOR;
		arch_name = "Ver";
		arch_attr = ATTR_JAVA_VERSION;	
	} else if(vmMode) {
		// For vm universe, we will print ATTR_VM_MEMORY
		// instead of ATTR_MEMORY
		opsys_name = "VMType";
		opsys_attr = ATTR_VM_TYPE;
		arch_name = "Network";
		arch_attr = ATTR_VM_NETWORKING_TYPES;
		mem_name = "VMMem";
		mem_attr = ATTR_VM_MEMORY;
	} else {
		opsys_name = opsys_attr = ATTR_OPSYS;
		arch_name = arch_attr = ATTR_ARCH;
	}

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			if( vmMode ) {
				printf ("\n%-18.18s %-6.6s %-10.10s %-9.9s %-8.8s %-6.6s "
						"%-4.4s %-12.12s  %s\n\n",
						ATTR_NAME, opsys_name, arch_name, ATTR_STATE, ATTR_ACTIVITY,
						ATTR_LOAD_AVG, mem_name, "ActvtyTime", "VMNetworking");
			}else {
				printf ("\n%-18.18s %-10.10s %-6.6s %-9.9s %-8.8s %-6.6s "
						"%-4.4s  %s\n\n",
						ATTR_NAME, opsys_name, arch_name, ATTR_STATE, ATTR_ACTIVITY,
						ATTR_LOAD_AVG, mem_name, "ActvtyTime");
			}
		
			alpm.registerFormat("%-18.18s ", ATTR_NAME, "[????????????????] ");

			if( vmMode ) {
				alpm.registerFormat("%-6.6s " , opsys_attr, "[????] ");
				alpm.registerFormat("%-10.10s " , arch_attr, "[????????] ");
			}else {
				alpm.registerFormat("%-10.10s " , opsys_attr, "[????????] ");
				alpm.registerFormat("%-6.6s " , arch_attr, "[????] ");
			}

			alpm.registerFormat("%-9.9s ",  ATTR_STATE, "[???????] ");
			alpm.registerFormat("%-8.8s ",  ATTR_ACTIVITY, "[??????] ");
			alpm.registerFormat("%.3f  ",  ATTR_LOAD_AVG, "[???]  ");
			alpm.registerFormat("%4d",  mem_attr, "[??]");

			first = false;
		}

		alpm.display(stdout, ad);
		if (ad->LookupInteger(ATTR_ENTERED_CURRENT_ACTIVITY, actvty)
			&& (ad->LookupInteger(ATTR_MY_CURRENT_TIME, now) ||
				ad->LookupInteger(ATTR_LAST_HEARD_FROM, now))) {
			actvty = now - actvty;
			printf("%s", format_time(actvty));
		} else {
			printf("   [Unknown]");
		}

		printf("\n");
	}
#endif
}


void
printServer (ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME,    wide_display ? -34 : -13, ! wide_display, StdInvalidField);
		ppSetColumn(0,ATTR_OPSYS,  -11, true, StdInvalidField);
		ppSetColumn(0,ATTR_ARCH,    -6, true, StdInvalidField);
		ppSetColumn("LoadAv",ATTR_LOAD_AVG, formatLoadAvg, NULL, 6, true, StdInvalidField);
		ppSetColumn(0,ATTR_MEMORY, "%8d",  true, StdInvalidField);
		ppSetColumn(0,ATTR_DISK, "%9d",  true, StdInvalidField);
		ppSetColumn(0,ATTR_MIPS, "%7d", true, StdInvalidField);
		ppSetColumn(0,ATTR_KFLOPS, "%9d", true, StdInvalidField);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-13.13s %-11.11s %-6.6s %-6.6s %-6.6s  %-7.7s "
					"%-10.10s %-10.10s\n\n",
				ATTR_NAME, ATTR_OPSYS, ATTR_ARCH, ATTR_LOAD_AVG, ATTR_MEMORY,
				ATTR_DISK, ATTR_MIPS, ATTR_KFLOPS);
		
			alpm.registerFormat("%-13.13s ", ATTR_NAME, "[???????????] ");
			alpm.registerFormat("%-11.11s " , ATTR_OPSYS, "[?????????] ");
			alpm.registerFormat("%-6.6s " , ATTR_ARCH, "[????] ");
			alpm.registerFormat("%.3f  ",  ATTR_LOAD_AVG, "[???]  ");
			alpm.registerFormat("%6d  ",  ATTR_MEMORY, "[????]  ");
			alpm.registerFormat("%7d ",  ATTR_DISK, "[?????] ");
			alpm.registerFormat("%10d ", ATTR_MIPS, "[????????] ");
			alpm.registerFormat("%10d\n", ATTR_KFLOPS, "[????????]\n");

			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
}

void
printState (ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		int timewid = wide_display ? 12 : 11;
		ppSetColumn(0,ATTR_NAME,    wide_display ? -34 : -12, ! wide_display, StdInvalidField);
		ppSetColumn("Cpu",ATTR_CPUS,  3, true, StdInvalidField);
		ppSetColumn(" Mem",ATTR_MEMORY, 5, true, StdInvalidField);
		//ppSetColumn("Load ",ATTR_LOAD_AVG, "%.3f", true, StdInvalidField);
		ppSetColumn("LoadAv",ATTR_LOAD_AVG, formatLoadAvg, NULL, 6, true, StdInvalidField);
		ppSetColumn("  KbdIdle",ATTR_KEYBOARD_IDLE, formatRealTime, timewid, true, StdInvalidField);
		ppSetColumn(0,ATTR_STATE, -7,  true, StdInvalidField);
		ppSetColumn("  StateTime",ATTR_ENTERED_CURRENT_STATE, formatElapsedTime, timewid, true, StdInvalidField);
		ppSetColumn("Activ",ATTR_ACTIVITY, -5, true, StdInvalidField);
		ppSetColumn("  ActvtyTime",ATTR_ENTERED_CURRENT_ACTIVITY, formatElapsedTime, timewid, true, StdInvalidField);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-10.10s %-3.3s %5.5s %-6.6s  "
					"%-10.10s %-7.7s   %-11.11s"
					"%-4.4s %-12.12s\n\n",
				ATTR_NAME, ATTR_CPUS, ATTR_MEMORY, "LoadAv",
				"KbdIdle", ATTR_STATE, "StateTime",
				ATTR_ACTIVITY, "ActvtyTime");
		
			alpm.registerFormat("%-10.10s ", ATTR_NAME, "[????????] ");
			alpm.registerFormat("%3d " , ATTR_CPUS, "[?] ");
			alpm.registerFormat("%5d " , ATTR_MEMORY, "[???] ");
			alpm.registerFormat("%.3f ", ATTR_LOAD_AVG, "[???] ");
			alpm.registerFormat( (IntCustomFmt) format_time,
									ATTR_KEYBOARD_IDLE,
									"[??????????]");
			alpm.registerFormat(" %-7.7s ",  ATTR_STATE, " [?????] ");
			alpm.registerFormat( (IntCustomFmt) formatElapsedTime,
									ATTR_ENTERED_CURRENT_STATE,
									"[??????????]");
			alpm.registerFormat(" %-4.4s ",  ATTR_ACTIVITY, " [??] ");
			alpm.registerFormat( (IntCustomFmt) formatElapsedTime,
									ATTR_ENTERED_CURRENT_ACTIVITY,
									"[??????????]");
			alpm.registerFormat("\n", "*bogus*", "\n");  // force newline

			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
}

void
printRun (ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME,    wide_display ? -34 : -15, ! wide_display, StdInvalidField);
		if (javaMode) {
			ppSetColumn(0,ATTR_JAVA_VENDOR,  -11, ! wide_display, StdInvalidField);
			ppSetColumn("Ver",ATTR_JAVA_VERSION,  -6, ! wide_display, StdInvalidField);
		} else if (vmMode) {
			ppSetColumn("VMType",ATTR_VM_TYPE,  -6, ! wide_display, StdInvalidField);
			ppSetColumn("Network",ATTR_VM_NETWORKING_TYPES,  -11, ! wide_display, StdInvalidField);
		} else {
			ppSetColumn(0,ATTR_OPSYS,  -11, true, StdInvalidField);
			ppSetColumn(0,ATTR_ARCH,    -6, true, StdInvalidField);
		}
		ppSetColumn("LoadAv",ATTR_LOAD_AVG, formatLoadAvg, NULL, 6, true, StdInvalidField);
		ppSetColumn(0,ATTR_REMOTE_USER,    -20, ! wide_display, StdInvalidField);
		ppSetColumn(0,ATTR_CLIENT_MACHINE, -16, ! wide_display, StdInvalidField);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	const char *opsys_attr, *arch_attr;
	const char *opsys_name, *arch_name;

	if(javaMode) {
		opsys_name = opsys_attr = ATTR_JAVA_VENDOR;
		arch_name = "Ver";
		arch_attr = ATTR_JAVA_VERSION;	
	} else if(vmMode) {
		opsys_name = "VMType";
		opsys_attr = ATTR_VM_TYPE;
		arch_name = "Network";
		arch_attr = ATTR_VM_NETWORKING_TYPES;
	} else {
		opsys_name = opsys_attr = ATTR_OPSYS;
		arch_name = arch_attr = ATTR_ARCH;
	}

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			if(vmMode) {
				printf ("\n%-13.13s %-6.6s %-11.11s %-6.6s %-20.20s %-15.15s\n\n",
						ATTR_NAME, opsys_name, arch_name, ATTR_LOAD_AVG,
						ATTR_REMOTE_USER, ATTR_CLIENT_MACHINE);
			}else {
				printf ("\n%-13.13s %-11.11s %-6.6s %-6.6s %-20.20s %-15.15s\n\n",
						ATTR_NAME, opsys_name, arch_name, ATTR_LOAD_AVG,
						ATTR_REMOTE_USER, ATTR_CLIENT_MACHINE);
			}
		
			alpm.registerFormat("%-13.13s ", ATTR_NAME, "[???????????] ");


			if(vmMode) {
				alpm.registerFormat("%-6.6s " , opsys_attr, "[????] ");
				alpm.registerFormat("%-11.11s " , arch_attr, "[?????????] ");
			}else {
				alpm.registerFormat("%-11.11s " , opsys_attr, "[?????????] ");
				alpm.registerFormat("%-6.6s " , arch_attr, "[????] ");
			}

			alpm.registerFormat("%-.3f  ",  ATTR_LOAD_AVG, "[???]  ");
			alpm.registerFormat("%-20.20s ", ATTR_REMOTE_USER,
													"[??????????????????] ");
			alpm.registerFormat("%-15.15s\n", ATTR_CLIENT_MACHINE,
													"[?????????????]\n");

			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
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
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME,    wide_display ? -34 : -20, ! wide_display, StdInvalidField);
		ppSetColumn(0,ATTR_MACHINE, wide_display ? -34 : -10, ! wide_display, StdInvalidField);
		ppSetColumn(0,ATTR_TOTAL_RUNNING_JOBS, "%16d", true, StdInvalidField);
		ppSetColumn(0,ATTR_TOTAL_IDLE_JOBS,    "%13d", true, StdInvalidField);
		ppSetColumn(0,ATTR_TOTAL_HELD_JOBS,    "%14d", true, StdInvalidField);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-10.10s %-16.16s %-13.13s %-14.14s\n\n",
				ATTR_NAME, ATTR_MACHINE, ATTR_TOTAL_RUNNING_JOBS,
				ATTR_TOTAL_IDLE_JOBS, ATTR_TOTAL_HELD_JOBS);
		
			alpm.registerFormat("%-20.20s ", ATTR_NAME,
													"[??????????????????] ");
			alpm.registerFormat("%-10.10s ", ATTR_MACHINE,
													"[????????] ");
			alpm.registerFormat("%16d ",ATTR_TOTAL_RUNNING_JOBS,
													"[??????????????] ");
			alpm.registerFormat("%13d ",ATTR_TOTAL_IDLE_JOBS,
													"[???????????] ");
			alpm.registerFormat("%14d\n",ATTR_TOTAL_HELD_JOBS,"[????????????]\n");

			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
}


void
printScheddSubmittors (ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME,    wide_display ? -34 : -28, ! wide_display, StdInvalidField);
		ppSetColumn(0,ATTR_MACHINE, wide_display ? -34 : -18, ! wide_display, StdInvalidField);
		ppSetColumn(0,ATTR_RUNNING_JOBS, "%11d", true, StdInvalidField);
		ppSetColumn(0,ATTR_IDLE_JOBS,    "%8d", true, StdInvalidField);
		ppSetColumn(0,ATTR_HELD_JOBS,    "%8d", true, StdInvalidField);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-10.10s  %8.8s %-8.8s %-8.8s\n\n",
				ATTR_NAME, ATTR_MACHINE, "Running", ATTR_IDLE_JOBS,
				ATTR_HELD_JOBS);
		
			alpm.registerFormat("%-20.20s ", ATTR_NAME,
													"[??????????????????] ");
			alpm.registerFormat("%-10.10s  ", ATTR_MACHINE,
													"[????????]  ");
			alpm.registerFormat("%8d ", ATTR_RUNNING_JOBS, "[??????] ");
			alpm.registerFormat("%8d ", ATTR_IDLE_JOBS, "[??????] ");
			alpm.registerFormat("%8d\n", ATTR_HELD_JOBS, "[???????]\n");

			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
}

void
printCollectorNormal(ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME,    wide_display ? -34 : -28, ! wide_display, StdInvalidField);
		ppSetColumn(0,ATTR_MACHINE, wide_display ? -34 : -18, ! wide_display, StdInvalidField);
		ppSetColumn(0,ATTR_RUNNING_JOBS, "%11d", true, StdInvalidField);
		ppSetColumn(0,ATTR_IDLE_JOBS,    "%8d", true, StdInvalidField);
		ppSetColumn(0,ATTR_NUM_HOSTS_TOTAL,    "%10d", true, StdInvalidField);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-20.20s  %-8.8s %-8.8s  %s\n\n",
				ATTR_NAME, ATTR_MACHINE, "Running", ATTR_IDLE_JOBS,
				ATTR_NUM_HOSTS_TOTAL);
		
			alpm.registerFormat("%-20.20s ", ATTR_NAME,
													"[??????????????????] ");
			alpm.registerFormat("%-20.20s ", ATTR_MACHINE,
													"[??????????????????] ");
			alpm.registerFormat("%8d ", ATTR_RUNNING_JOBS, " [?????] ");
			alpm.registerFormat("%8d  ", ATTR_IDLE_JOBS, " [?????]  ");
			alpm.registerFormat("%8d\n", ATTR_NUM_HOSTS_TOTAL, "[?????]\n");

			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
}

void
printMasterNormal(ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME, -20, false, StdInvalidField);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			alpm.registerFormat("%s\n",ATTR_NAME,"[??????????????????]");
			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
}


void
printCkptSrvrNormal(ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME,    wide_display ? -34 : -28, ! wide_display, StdInvalidField);
		ppSetColumn("AvailDisk",ATTR_DISK, "%9d", true, StdInvalidField);
		ppSetColumn(0,ATTR_SUBNET, "%-11s", !wide_display, "[?????]");

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-9.9s %-11.11s\n\n", ATTR_NAME,
					"AvailDisk", ATTR_SUBNET);
			alpm.registerFormat("%-20.20s ", ATTR_NAME, "[??????????????????] ");
			alpm.registerFormat("%9d ", ATTR_DISK, "[???????] ");
			alpm.registerFormat("%-11s\n", ATTR_SUBNET, "[?????]\n");
			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
}


void
printStorageNormal(ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME,    wide_display ? -34 : -30, ! wide_display, StdInvalidField);
		ppSetColumn("AvailDisk",ATTR_DISK, "%9d", true, StdInvalidField);
		ppSetColumn(0,ATTR_SUBNET, "%-11s", !wide_display, "[?????]");

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-30.30s %-9.9s %-11.11s\n\n", ATTR_NAME,
					"AvailDisk", ATTR_SUBNET);
			alpm.registerFormat("%-30.30s ", ATTR_NAME, "[????????????????????????????] ");
			alpm.registerFormat("%9d ", ATTR_DISK, "[???????] ");
			alpm.registerFormat("%-11s\n", ATTR_SUBNET, "[?????]\n");
			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
}

void
printGridNormal(ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME, -34, ! wide_display, StdInvalidField);
		ppSetColumn(0,"NumJobs", "%7d", true, StdInvalidField);
		ppSetColumn("Allowed","SubmitsAllowed", "%7d", true, StdInvalidField);
		ppSetColumn(" Wanted","SubmitsWanted", "%7d", true, StdInvalidField);
		ppSetColumn(0,ATTR_RUNNING_JOBS, "%11d", true, StdInvalidField);
		ppSetColumn(0,ATTR_IDLE_JOBS,    "%8d", true, StdInvalidField);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
    static bool first = true;
	static AttrListPrintMask alpm;

    if (ad)
    {
        // print header if necessary
        if (first)
        {
            printf ("\n%-35.35s %-7.7s %-7.7s %-7.7s %-7.7s %-7.7s\n\n",
				ATTR_NAME, "NumJobs", "Allowed", "Wanted", "Running", "Idle" );
			
			alpm.registerFormat("%-35.35s ", ATTR_NAME, 
				"[?????????????????????????????????] " );
			alpm.registerFormat ( "%-7d ", "NumJobs",
				"[?????] " );
			alpm.registerFormat ( "%-7d ", "SubmitsAllowed",
				"[?????] " );
			alpm.registerFormat ( "%-7d ", "SubmitsWanted",
				"[?????] " );
			alpm.registerFormat ( "%-7d ", ATTR_RUNNING_JOBS,
				"[?????] " );
			alpm.registerFormat ( "%-7d\n", ATTR_IDLE_JOBS,
				"[?????]\n" );

            first = false;
        }
        
        alpm.display (stdout, ad);
	}
#endif
}

void
printNegotiatorNormal(ClassAd *ad, bool first)
{
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_NAME, wide_display ? -32 : -20, ! wide_display,  StdInvalidField);
		ppSetColumn(0,ATTR_MACHINE, wide_display ? -32 : -20, ! wide_display,  StdInvalidField);

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	static AttrListPrintMask alpm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-20.20s\n\n",
				ATTR_NAME, ATTR_MACHINE);
		
			alpm.registerFormat("%-20.20s ", ATTR_NAME,
													"[??????????????????] ");
			alpm.registerFormat("%-20.20s\n", ATTR_MACHINE,
													"[??????????????????] ");

			first = false;
		}

		alpm.display (stdout, ad);
	}
#endif
}

/*
We can't use the AttrListPrintMask here, because the AttrList does not actually contain
the MyType and TargetType fields that we want to display. 
These are actually contained in the ClassAd.
*/

const char *
formatAdType ( char * type, AttrList *, Formatter &)
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
#if 1
	if (first) {
		ppInit();
		ppSetColumn(0,ATTR_MY_TYPE,     formatAdType, -18, true,  "None");
		ppSetColumn(0,ATTR_TARGET_TYPE, formatAdType, -18, true,  "None");
		ppSetColumn(0,ATTR_NAME, wide_display ? "%-41s" : "%-41.41s", ! wide_display, "[???]");

		ppDisplayHeadings(stdout, ad, "\n");
	}
	if (ad)
		pm.display (stdout, ad);
#else
	static bool first = true;
	char *name;
	const char *my_type, *target_type;

	if (ad)
	{
		if (first)
		{		
			printf ("\n%-20.20s %-20.20s %-30.30s\n\n", ATTR_MY_TYPE, ATTR_TARGET_TYPE, ATTR_NAME );
			first = false;
		}
		name = 0;
		if(!ad->LookupString(ATTR_NAME,&name)) {
			name = (char *) malloc(strlen("[???]") + 1);
			ASSERT( name != NULL );
			strcpy(name,"[???]");
		}

		my_type = ad->GetMyTypeName();
		if(!my_type[0]) my_type = "None";

		target_type = ad->GetTargetTypeName();
		if(!target_type[0]) target_type = "None";

		printf("%-20.20s %-20.20s %-30.30s\n",my_type,target_type,name);
		free(name);

		pm.display (stdout, ad);
	}
#endif
}


void
printVerbose (ClassAd *ad)
{
	ad->fPrint (stdout);
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
formatLoadAvg (float fl, AttrList *, Formatter &)
{
	static char buf[60];
	sprintf(buf, "%.3f", fl);
	return buf;
}

#if 0 // not currently used
static const char *
formatFloat (float fl, AttrList *, Formatter & fmt)
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
