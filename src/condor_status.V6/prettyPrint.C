#include "condor_api.h"
#include "status_types.h"
#include "totals.h"

extern ppOption				ppStyle;
extern AttrListPrintMask 	pm;
extern int					wantTotals;
extern Totals				total;

void printNormal 	(ClassAd *);
void printServer 	(ClassAd *);
void printAvail  	(ClassAd *);
void printRun    	(ClassAd *);
void printSubmittors(ClassAd *);
void printVerbose   (ClassAd *);
void printCustom    (ClassAd *);

void
prettyPrint (ClassAdList &adList)
{
	ClassAd	*ad;

	adList.Open();
	while (ad = adList.Next()) {
		switch (ppStyle) {
    	  case PP_NORMAL:
			printNormal (ad);
			break;

    	  case PP_SERVER:
			printServer (ad);
			break;

    	  case PP_AVAIL:
			printAvail (ad);
			break;

    	  case PP_RUN:
			printRun (ad);
			break;

    	  case PP_SUBMITTORS:
			printSubmittors (ad);
			break;

    	  case PP_VERBOSE:
			printVerbose (ad);
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
	adList.Close();

	// if totals are required, call the required funtion
	switch (ppStyle) {
   	  case PP_NORMAL:
		printNormal (NULL);
		break;

   	  case PP_SERVER:
		printServer (NULL);
		break;

   	  case PP_AVAIL:
		printAvail (NULL);
		break;

   	  case PP_RUN:
		printRun (NULL);
		break;

   	  case PP_SUBMITTORS:
		printSubmittors (NULL);
		break;
	}
}


void
printNormal (ClassAd *ad)
{
}


void
printServer (ClassAd *ad)
{
}


void
printAvail (ClassAd *ad)
{
}


void
printRun (ClassAd *ad)
{
}


void
printSubmittors (ClassAd *ad)
{
}


void
printVerbose (ClassAd *ad)
{
	ad->fPrint (stdout);
	fputc ('\n', stdout);	
}


void
printCustom (ClassAd *ad)
{
	(void) pm.display (stdout, ad);
}
