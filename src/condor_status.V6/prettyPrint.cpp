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
#include "status_types.h"
#include "totals.h"
#include "format_time.h"
#include "condor_xml_classads.h"

extern ppOption				ppStyle;
extern AttrListPrintMask 	pm;
extern int					wantOnlyTotals;
extern bool javaMode;
extern bool vmMode;
extern ClassAd *targetAd;

extern char *format_time( int );

static int stashed_now = 0;

void printStartdNormal 	(ClassAd *);
void printScheddNormal 	(ClassAd *);

#ifdef WANT_QUILL
void printQuillNormal 	(ClassAd *);
#endif /* WANT_QUILL */

void printScheddSubmittors(ClassAd *);
void printMasterNormal 	(ClassAd *);
void printCollectorNormal (ClassAd *);
void printCkptSrvrNormal(ClassAd *);
void printStorageNormal (ClassAd *);
void printNegotiatorNormal (ClassAd *);
void printGridNormal 	(ClassAd *);
void printAnyNormal 	(ClassAd *);
void printServer 		(ClassAd *);
void printRun    		(ClassAd *);
void printCOD    		(ClassAd *);
void printState			(ClassAd *);
void printVerbose   	(ClassAd *);
void printXML       	(ClassAd *, bool first_ad, bool last_ad);
void printCustom    	(ClassAd *);

char *formatElapsedTime( int , AttrList* );
char *formatRealTime( int , AttrList * );

void
prettyPrint (ClassAdList &adList, TrackTotals *totals)
{
	ClassAd	*ad;
	int     classad_index;
	int     last_classad_index;

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
			if ( !strcmp( ad->GetMyTypeName(), SCHEDD_ADTYPE ) &&
				 !ad->Lookup( ATTR_NUM_USERS ) ) {
				ad->SetMyTypeName( SUBMITTER_ADTYPE );
			}
			switch (ppStyle) {
			  case PP_STARTD_NORMAL:
				printStartdNormal (ad);
				break;

			  case PP_STARTD_SERVER:
				printServer (ad);
				break;

			  case PP_STARTD_RUN:
				printRun (ad);
				break;

			  case PP_STARTD_COD:
				printCOD (ad);
				break;

			  case PP_STARTD_STATE:
				printState( ad );
				break;

#ifdef WANT_QUILL
			  case PP_QUILL_NORMAL:
				printQuillNormal (ad);
				break;
#endif /* WANT_QUILL */

			  case PP_SCHEDD_NORMAL:
				printScheddNormal (ad);
				break;

			  case PP_NEGOTIATOR_NORMAL:
				printNegotiatorNormal (ad);
				break;


			  case PP_SCHEDD_SUBMITTORS:
				printScheddSubmittors (ad);
				break;

			  case PP_VERBOSE:
				printVerbose (ad);
				break;

			  case PP_XML:
				printXML (ad, (classad_index == 0),
					(classad_index == last_classad_index));
				break;

			  case PP_MASTER_NORMAL:
				printMasterNormal(ad);
				break;

			  case PP_COLLECTOR_NORMAL:
				printCollectorNormal(ad);
				break;

			  case PP_CKPT_SRVR_NORMAL:
				printCkptSrvrNormal(ad);
				break;

			  case PP_STORAGE_NORMAL:
				printStorageNormal(ad);
				break;

			  case PP_GRID_NORMAL:
				printGridNormal(ad);
				break;

			  case PP_GENERIC_NORMAL:
			  case PP_GENERIC:
			  case PP_ANY_NORMAL:
				printAnyNormal(ad);
				break;

			  case PP_CUSTOM:
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
printStartdNormal (ClassAd *ad)
{
	static bool first = true;
	static AttrListPrintMask alpm;
	int    now;
	int	   actvty;

	const char *opsys_attr, *arch_attr, *mem_attr;
	const char *opsys_name, *arch_name, *mem_name;
 
	mem_name = "Mem";
	mem_attr = ATTR_MEMORY;

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
}


void
printServer (ClassAd *ad)
{
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
}

void
printState (ClassAd *ad)
{
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
}

void
printRun (ClassAd *ad)
{
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

#ifdef WANT_QUILL
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
#endif /* WANT_QUILL */

void
printScheddNormal (ClassAd *ad)
{
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
}


void
printScheddSubmittors (ClassAd *ad)
{
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
}

void
printCollectorNormal(ClassAd *ad)
{
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
}

void
printMasterNormal(ClassAd *ad)
{
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
}


void
printCkptSrvrNormal(ClassAd *ad)
{
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
}


void
printStorageNormal(ClassAd *ad)
{
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
}

void
printGridNormal(ClassAd *ad)
{
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
}

void
printNegotiatorNormal(ClassAd *ad)
{
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
}

/*
We can't use the AttrListPrintMask here, because the AttrList does not actually contain
the MyType and TargetType fields that we want to display. 
These are actually contained in the ClassAd.
*/

void
printAnyNormal(ClassAd *ad)
{
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
	ClassAdXMLUnparser  unparser;
	MyString            xml;

	if (first_ad) {
		unparser.AddXMLFileHeader(xml);
	}

	unparser.SetUseCompactSpacing(false);
	if ( NULL != ad ) {
		unparser.Unparse(ad, xml);
	}

	if (last_ad) {
		unparser.AddXMLFileFooter(xml);
	}

	printf("%s\n", xml.Value());
	return;
}

void
printCustom (ClassAd *ad)
{
	(void) pm.display (stdout, ad, targetAd);
}

char *
formatElapsedTime( int t , AttrList *al )
{
	int	now;

	al->LookupInteger( ATTR_LAST_HEARD_FROM , now );	
	t = now - t;
	return format_time( t );
}

char *
formatRealTime( int t , AttrList * )
{
	return format_time( t );
}
