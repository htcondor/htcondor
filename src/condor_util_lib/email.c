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
#include "config.h"
#include "debug.h"
#include "condor_uid.h"
#include "condor_email.h"

#define EMAIL_SUBJECT_PROLOG "[Condor] "

#ifdef WIN32
static char * EMAIL_FINAL_COMMAND = NULL;
#endif

FILE *
email_open( const char *email_addr, const char *subject )
{
	char *FinalAddr;
	char *Mailer;
	char *temp;
	FILE *mailerstream;
	priv_state priv;
	char RelayHost[150];
	char *FinalSubject = "\0";
	char final_command[2000];

	RelayHost[0] = '\0';

	if ( (Mailer = param("MAIL")) == NULL ) {
		dprintf(D_FULLDEBUG,
			"Trying to email, but MAIL not specified in config file\n");
		return NULL;
	}

#ifdef WIN32
	/* On WinNT, we need to be given an SMTP server, and we must pass
	 * this servername to the Mailer with a -relay option.  So, if there
	 * is not alrady a -relay option specified, add one.
	 */
	if ( strstr(Mailer,"-relay") == NULL ) {
		if ( (temp=param("SMTP_SERVER")) == NULL ) {
			dprintf(D_FULLDEBUG,
				"Trying to email, but SMTP_SERVER not specified in config file\n");
			free(Mailer);
			return NULL;
		}
		sprintf(RelayHost,"-relay %s",temp);
		free(temp);
	}
#	define EMAIL_POPEN_FLAGS "wt"
#else
#	define EMAIL_POPEN_FLAGS "w"
#endif
	
	/* Take care of the subject. */
	if ( subject ) {
		FinalSubject = subject;
	}

	/* Take care of destination email address.  If it is NULL, grab 
	 * the email of the Condor admin from the config file.
	 * We strdup this since we modify it (we replace any commas with spaces
	 * so user can give a list of addresses and we can pass it to the Mailer).
	 */
	if ( email_addr ) {
		FinalAddr = strdup(email_addr);
	} else {
		if ( (temp = param("CONDOR_ADMIN")) == NULL ) {
			dprintf(D_FULLDEBUG,
				"Trying to email, but CONDOR_ADMIN not specified in config file\n");
			free(Mailer);
			return NULL;
		}
		FinalAddr = strdup(temp);
		free(temp);
	}
	/* now replace commas with spaces */
	while ( (temp=strchr(FinalAddr,',')) ) {
		*temp = ' ';
	}

	/* create the final command to pass to popen */
	sprintf(final_command,"%s -s \"%s%s\" %s %s",
		Mailer,EMAIL_SUBJECT_PROLOG,FinalSubject,RelayHost,FinalAddr);
	dprintf(D_FULLDEBUG,"Sending email using command %s\n",final_command);

	/* Want the letter to come from "condor" if possible */
	priv = set_condor_priv();

	/* finally, open up pipe to the mailer */
	mailerstream = popen(final_command,EMAIL_POPEN_FLAGS);

	/* Set priv state back */
	set_priv(priv);


#ifdef WIN32
	if ( mailerstream == NULL ) {
		/* On NT, the process acting as the service (in this case
		** the condor_master) fails on popen(), even though we did
		** an AllocConsole().  Thanks for nothing, Microsoft.  We
		** work around this by first confirming that the mailer 
		** program really exists.  If it does, we open a temp file
		** and return the FILE* to the temp file.  We also stash
		** away the final_command in a global static, so when we
		** do an email close, we call system() with "mailer < tmp_file".
		*/
		if (access(Mailer,0) != -1 ) {	
			/* we now know the mailer exists */
			char * email_tmp_file = NULL;

			/* now make certain there is not another outstanding email_open */
			if (EMAIL_FINAL_COMMAND) {
				dprintf(D_ALWAYS,
					"Cannot have more than one outstanding email_open()");				
			} else {
				/* note: result from tempnam has been malloced */
				email_tmp_file = tempnam("\\tmp","condoremail");
				if (email_tmp_file) {
					/* "T" flag to fopen() means short-lived file; delay
					** flush to disk as long as possible. */
					mailerstream = fopen(email_tmp_file,"wtT");
					if ( mailerstream ) {
						strcat(final_command," < ");
						strcat(final_command,email_tmp_file);
						EMAIL_FINAL_COMMAND = strdup(final_command);
					} 
					free(email_tmp_file);					
				}
			}
		}
	}
#endif

	if ( mailerstream == NULL ) {	
		dprintf(D_ALWAYS,"Failed to access email program \"%s\"\n",
			Mailer);
	}

	/* free up everything we strdup-ed and param-ed, and return result */
	free(Mailer);
	free(FinalAddr);
	return mailerstream;
}

FILE *
email_admin_open(char *subject)
{
	return email_open(NULL,subject);
}

FILE *
email_developers_open(char *subject)
{
	char *tmp;
	FILE *mailer;

	/* 
	** According to the docs, if CONDOR_DEVELOPERS is not
	** in the config file, it defaults to UW.  If it is "NONE", 
	** nothing should be emailed.
	*/
    tmp = param ("CONDOR_DEVELOPERS");
    if (tmp == NULL) {
		/* we strdup here since we always call free below */
        tmp = strdup("condor-admin@cs.wisc.edu");
    } else
    if (stricmp (tmp, "NONE") == 0) {
        free (tmp);
        return NULL;
    }

	mailer = email_open(tmp,subject);		

	/* Don't forget to free tmp! */
	free(tmp);
	return mailer;
}

/* 
** Close the stream to the mailer.  It'd be nice to return the exit
** status from the mailer here, but on many platforms we cannot safely
** do a pclose and need to do an fclose.
*/
void
email_close(FILE *mailer)
{
	char *temp;

	if ( mailer == NULL ) {
		return;
	}

	/* Put a signature on the bottom of the email */
	/* if ( (temp = param("CONDOR_ADMIN")) == NULL ) { */
	
	/* 
	** On many Unix platforms, we cannot use
	** 'pclose()' here: it does its own wait, and messes
    ** with our handling of SIGCHLD! So do fclose() instead.
	** Except on HPUX & Win32, pclose() is both safe and required.
	*/
#if defined(HPUX) 
	pclose( mailer );
#elif defined(WIN32)
	if (EMAIL_FINAL_COMMAND == NULL) {
		pclose( mailer );
	} else {
		char *email_filename = NULL;
		fclose( mailer );
		dprintf(D_FULLDEBUG,"Sending email via system(%s)\n",
			EMAIL_FINAL_COMMAND);
		system(EMAIL_FINAL_COMMAND);
		if ( (email_filename=strrchr(EMAIL_FINAL_COMMAND,'<')) ) {
			email_filename++;	/* go past the "<" */
			email_filename++;	/* go past the space after the < */
			if ( unlink(email_filename) == -1 ) {
				dprintf(D_ALWAYS,"email_close: cannot unlink temp file %s\n",
					email_filename);
			}
		}
		free(EMAIL_FINAL_COMMAND);
		EMAIL_FINAL_COMMAND = NULL;
	}
#else
	(void)fclose( mailer );
#endif
}
