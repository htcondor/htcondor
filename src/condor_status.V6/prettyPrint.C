/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_api.h"
//#include "condor_query.h"	// NAC
//#include "ad_printmask.h"	// NAC
//#include "classadList.h"	// NAC
#include "status_types.h"
#include "totals.h"
#include "format_time.h"
#include "condor_xml_classads.h"

extern ppOption				ppStyle;
extern ClassAdPrintMask 	pm;
extern int					wantOnlyTotals;
extern bool javaMode;

extern char *format_time( int );

void printStartdNormal 	(ClassAd *);
void printScheddNormal 	(ClassAd *);
void printScheddSubmittors(ClassAd *);
void printMasterNormal 	(ClassAd *);
void printCollectorNormal (ClassAd *);
void printCkptSrvrNormal(ClassAd *);
void printStorageNormal(ClassAd *);
void printAnyNormal(ClassAd *);
void printServer 		(ClassAd *);
void printRun    		(ClassAd *);
void printState			(ClassAd *);
void printVerbose   	(ClassAd *);
void printXML       	(ClassAd *, bool first_ad, bool last_ad);
void printCustom    	(ClassAd *);

char *formatElapsedTime( int , ClassAd* );
char *formatRealTime( int , ClassAd * );

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

			  case PP_STARTD_STATE:
				printState( ad );
				break;

			  case PP_SCHEDD_NORMAL:
				printScheddNormal (ad);
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

	printf ("\n");

	// if totals are required, display totals
	if (adList.MyLength() > 0 && totals) totals->displayTotals(stdout, 20);
}


void
printStartdNormal (ClassAd *ad)
{
	static bool first = true;
	static ClassAdPrintMask pm; 
	int    now;
	int	   actvty;

	char *opsys_attr, *arch_attr;
	char *opsys_name, *arch_name;

	if(javaMode) {
		opsys_name = opsys_attr = (char*) ATTR_JAVA_VENDOR;
		arch_name = "Ver";
		arch_attr = (char*) ATTR_JAVA_VERSION;	
	} else {
		opsys_name = opsys_attr = (char*) ATTR_OPSYS;
		arch_name = arch_attr = (char*) ATTR_ARCH;
	}

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-13.13s %-11.11s %-6.6s %-10.10s %-10.10s %-6.6s "
						"%-4.4s  %s\n\n", 
					ATTR_NAME, opsys_name, arch_name, ATTR_STATE, ATTR_ACTIVITY, 
					ATTR_LOAD_AVG, "Mem", "ActvtyTime");
		
			pm.registerFormat("%-13.13s ", ATTR_NAME, "[???????????] ");
			pm.registerFormat("%-11.11s " , opsys_attr, "[?????????] ");
			pm.registerFormat("%-6.6s " , arch_attr, "[????] ");
			pm.registerFormat("%-10.10s ",  ATTR_STATE), "[????????] ";
			pm.registerFormat("%-10.10s ",  ATTR_ACTIVITY, "[????????] ");
			pm.registerFormat("%.3f  ",  ATTR_LOAD_AVG, "[???]  ");
			pm.registerFormat("%4d",  ATTR_MEMORY, "[??]  ");

			first = false;
		}

		pm.display (stdout, ad);
//		if( ad->LookupInteger( ATTR_ENTERED_CURRENT_ACTIVITY , actvty ) &&
//			ad->LookupInteger( ATTR_LAST_HEARD_FROM , now ) )
		if( ad->EvaluateAttrInt( ATTR_ENTERED_CURRENT_ACTIVITY, actvty ) &&
			ad->EvaluateAttrInt( ATTR_LAST_HEARD_FROM, now ) )
		{
			actvty = now - actvty;
			printf( "%s\n", format_time( actvty ) );
		}
		else
		{
			printf(" [Unknown]\n");
		}
	}
}


void
printServer (ClassAd *ad)
{
	static bool first = true;
	static ClassAdPrintMask pm; 

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-13.13s %-11.11s %-6.6s %-6.6s %-6.6s  %-7.7s "
					"%-10.10s %-10.10s\n\n", 
				ATTR_NAME, ATTR_OPSYS, ATTR_ARCH, ATTR_LOAD_AVG, ATTR_MEMORY, 
				ATTR_DISK, ATTR_MIPS, ATTR_KFLOPS);
		
			pm.registerFormat("%-13.13s ", ATTR_NAME, "[???????????] ");
			pm.registerFormat("%-11.11s " , ATTR_OPSYS, "[?????????] ");
			pm.registerFormat("%-6.6s " , ATTR_ARCH, "[????] ");
			pm.registerFormat("%.3f  ",  ATTR_LOAD_AVG, "[???]  ");
			pm.registerFormat("%6d  ",  ATTR_MEMORY, "[????]  ");
			pm.registerFormat("%7d ",  ATTR_DISK, "[?????]");
			pm.registerFormat("%10d ", ATTR_MIPS, "[????????] ");
			pm.registerFormat("%10d\n", ATTR_KFLOPS, "[????????]\n");

			first = false;
		}

		pm.display (stdout, ad);
	}
}

void
printState (ClassAd *ad)
{
	static bool first = true;
	static ClassAdPrintMask pm; 

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
		
			pm.registerFormat("%-10.10s ", ATTR_NAME, "[????????] ");
			pm.registerFormat("%3d " , ATTR_CPUS, "[??] ");
			pm.registerFormat("%5d " , ATTR_MEMORY, "[???] ");
			pm.registerFormat("%.3f ", ATTR_LOAD_AVG, "[???]  ");
			pm.registerFormat( (IntCustomFmt) format_time,
									ATTR_KEYBOARD_IDLE,
									"[??????????] ");
			pm.registerFormat(" %-7.7s ",  ATTR_STATE, "[????????]  ");
			pm.registerFormat( (IntCustomFmt) formatElapsedTime, 
									ATTR_ENTERED_CURRENT_STATE,
									"[??????????] ");
			pm.registerFormat(" %-4.4s ",  ATTR_ACTIVITY, "[????????]  ");
			pm.registerFormat( (IntCustomFmt) formatElapsedTime, 
									ATTR_ENTERED_CURRENT_ACTIVITY,
									"[??????????] ");
			pm.registerFormat("\n", "*bogus*", "\n");  // force newline

			first = false;
		}

		pm.display (stdout, ad);
	}
}

void
printRun (ClassAd *ad)
{
	static bool first = true;
	static ClassAdPrintMask pm; 

	char *opsys_attr, *arch_attr;
	char *opsys_name, *arch_name;

	if(javaMode) {
		opsys_name = opsys_attr = (char*) ATTR_JAVA_VENDOR;
		arch_name = "Ver";
		arch_attr = (char*) ATTR_JAVA_VERSION;	
	} else {
		opsys_name = opsys_attr = (char*) ATTR_OPSYS;
		arch_name = arch_attr = (char*) ATTR_ARCH;
	}

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-13.13s %-11.11s %-6.6s %-6.6s %-20.20s %-15.15s\n\n",
				ATTR_NAME, opsys_name, arch_name, ATTR_LOAD_AVG, 
				ATTR_REMOTE_USER, ATTR_CLIENT_MACHINE);
		
			pm.registerFormat("%-13.13s ", ATTR_NAME, "[???????????] ");
			pm.registerFormat("%-11.11s " , opsys_attr, "[?????????] ");
			pm.registerFormat("%-6.6s " , arch_attr, "[????] ");
			pm.registerFormat("%-.3f  ",  ATTR_LOAD_AVG, "[???]  ");
			pm.registerFormat("%-20.20s ", ATTR_REMOTE_USER, 
													"[??????????????????] ");
			pm.registerFormat("%-15.15s\n", ATTR_CLIENT_MACHINE, 
													"[?????????????]\n");

			first = false;
		}

		pm.display (stdout, ad);
	}
}


void
printScheddNormal (ClassAd *ad)
{
	static bool first = true;
	static ClassAdPrintMask pm; 

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-10.10s %-16.16s %-13.13s %-14.14s\n\n",
				ATTR_NAME, ATTR_MACHINE, ATTR_TOTAL_RUNNING_JOBS, 
				ATTR_TOTAL_IDLE_JOBS, ATTR_TOTAL_HELD_JOBS);
		
			pm.registerFormat("%-20.20s ", ATTR_NAME, 
													"[??????????????????] ");
			pm.registerFormat("%-10.10s ", ATTR_MACHINE, 
													"[????????] ");
			pm.registerFormat("%16d ",ATTR_TOTAL_RUNNING_JOBS,
													"[??????????????] ");
			pm.registerFormat("%13d ",ATTR_TOTAL_IDLE_JOBS, 
													"[???????????] ");
			pm.registerFormat("%14d\n",ATTR_TOTAL_HELD_JOBS,"[????????????]\n");

			first = false;
		}

		pm.display (stdout, ad);
	}
}


void
printScheddSubmittors (ClassAd *ad)
{
	static bool first = true;
	static ClassAdPrintMask pm; 

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-10.10s  %8.8s %-8.8s %-8.8s\n\n",
				ATTR_NAME, ATTR_MACHINE, "Running", ATTR_IDLE_JOBS, 
				ATTR_HELD_JOBS);
		
			pm.registerFormat("%-20.20s ", ATTR_NAME, 
													"[??????????????????] ");
			pm.registerFormat("%-10.10s  ", ATTR_MACHINE, 
													"[????????]  ");
			pm.registerFormat("%8d ", ATTR_RUNNING_JOBS, "[??????] ");
			pm.registerFormat("%8d ", ATTR_IDLE_JOBS, "[??????] ");
			pm.registerFormat("%8d\n", ATTR_HELD_JOBS, "[???????]\n");

			first = false;
		}

		pm.display (stdout, ad);
	}
}

void
printCollectorNormal(ClassAd *ad)
{
	static bool first = true;
	static ClassAdPrintMask pm; 

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-20.20s  %-8.8s %-8.8s  %s\n\n",
				ATTR_NAME, ATTR_MACHINE, "Running", ATTR_IDLE_JOBS, 
				ATTR_NUM_HOSTS_TOTAL);
		
			pm.registerFormat("%-20.20s ", ATTR_NAME, 
													"[??????????????????] ");
			pm.registerFormat("%-20.20s ", ATTR_MACHINE, 
													"[??????????????????] ");
			pm.registerFormat("%8d ", ATTR_RUNNING_JOBS, " [?????] ");
			pm.registerFormat("%8d  ", ATTR_IDLE_JOBS, " [?????]  ");
			pm.registerFormat("%8d\n", ATTR_NUM_HOSTS_TOTAL, "[?????]\n");

			first = false;
		}

		pm.display (stdout, ad);
	}
}

void
printMasterNormal(ClassAd *ad)
{
	static bool first = true;
	static ClassAdPrintMask pm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			pm.registerFormat("%s\n",ATTR_NAME,"[??????????????????]");
			first = false;
		}
	}

	pm.display (stdout, ad);
}


void
printCkptSrvrNormal(ClassAd *ad)
{
	static bool first = true;
	static ClassAdPrintMask pm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-20.20s %-9.9s %-11.11s\n\n", ATTR_NAME,
					"AvailDisk", ATTR_SUBNET);
			pm.registerFormat("%-20.20s ", ATTR_NAME, "[??????????????????]");
			pm.registerFormat("%9d ", ATTR_DISK, "[?????]");
			pm.registerFormat("%-11s\n", ATTR_SUBNET, "[?????]\n");
			first = false;
		}
	}

	pm.display (stdout, ad);
}


void
printStorageNormal(ClassAd *ad)
{
	static bool first = true;
	static AttrListPrintMask pm;

	if (ad)
	{
		// print header if necessary
		if (first)
		{
			printf ("\n%-30.30s %-9.9s %-11.11s\n\n", ATTR_NAME,
					"AvailDisk", ATTR_SUBNET);
			pm.registerFormat("%-30.30s ", ATTR_NAME, "[??????????????????]");
			pm.registerFormat("%9d ", ATTR_DISK, "[?????]");
			pm.registerFormat("%-11s\n", ATTR_SUBNET, "[?????]\n");
			first = false;
		}
	}

	pm.display (stdout, ad);
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
	char name[ATTRLIST_MAX_EXPRESSION];
	const char *my_type, *target_type;

	if (ad)
	{
		if (first)
		{		
			printf ("\n%-20.20s %-20.20s %-30.30s\n\n", ATTR_MY_TYPE, ATTR_TARGET_TYPE, ATTR_NAME );
			first = false;
		}
		if(!ad->LookupString(ATTR_NAME,name)) {
			strcpy(name,"[???]");
		}

		my_type = ad->GetMyTypeName();
		if(!my_type[0]) my_type = "None";

		target_type = ad->GetTargetTypeName();
		if(!target_type[0]) target_type = "None";

		printf("%-20.20s %-20.20s %-30.30s\n",my_type,target_type,name);
	}
	pm.display (stdout, ad);
}


void
printVerbose (ClassAd *ad)
{
//	ad->fPrint (stdout);
	string 		buffer;		// NAC
	PrettyPrint pp;			// NAC

	pp.Unparse( buffer, ad );	// NAC
	cout << buffer << endl;			// NAC

//	fputc ('\n', stdout);	
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
	unparser.Unparse(ad, xml);

	if (last_ad) {
		unparser.AddXMLFileFooter(xml);
	}

	printf("%s\n", xml.Value());
	return;
}

void
printCustom (ClassAd *ad)
{
	(void) pm.display (stdout, ad);
}


char *
formatElapsedTime( int t , ClassAd *ad )
{
	int	now;

//	ad->LookupInteger( ATTR_LAST_HEARD_FROM , now );	
	ad->EvaluateAttrInt( ATTR_LAST_HEARD_FROM , now );	// NAC
	t = now - t;
	return format_time( t );
}

char *
formatRealTime( int t , ClassAd * )
{
	return format_time( t );
}



