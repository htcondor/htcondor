/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_uid.h"
#include "condor_email.h"
#include "my_popen.h"
#include "subsystem_info.h"
#include "ipv6_hostname.h"

#define EMAIL_SUBJECT_PROLOG "[Condor] "

#ifdef WIN32
	static char * EMAIL_FINAL_COMMAND = NULL;
#	define EMAIL_POPEN_FLAGS "wt"
#else
#	define EMAIL_POPEN_FLAGS "w"
#endif

/* how we actually get the FILE* for our mail program pipe varies vastly
	between NT and unix */
#ifdef WIN32
static FILE *email_open_implementation(char *Mailer,
									   const char * final_args[]);
#else
static FILE *email_open_implementation(const char * final_args[]);
#endif

extern DLL_IMPORT_MAGIC char **environ;

FILE *
email_open( const char *email_addr, const char *subject )
{
	char *Mailer;
	char *SmtpServer = NULL;
	char *FromAddress = NULL;
	char *FinalSubject;
	char *FinalAddr;
	char *temp;
	int token_boundary;
	int num_addresses;
	int arg_index;
	FILE *mailerstream;

	if ( (Mailer = param("MAIL")) == NULL ) {
		dprintf(D_FULLDEBUG,
			"Trying to email, but MAIL not specified in config file\n");
		return NULL;
	}

	/* Take care of the subject. */
	if ( subject ) {
		size_t prolog_length = strlen(EMAIL_SUBJECT_PROLOG);
		size_t subject_length = strlen(subject);
		FinalSubject = (char *)malloc(prolog_length + subject_length + 1);
		ASSERT( FinalSubject != NULL );
		memcpy(FinalSubject, EMAIL_SUBJECT_PROLOG, prolog_length);
		memcpy(&FinalSubject[prolog_length], subject, subject_length);
		FinalSubject[prolog_length + subject_length] = '\0';
	}
	else {
		FinalSubject = strdup(EMAIL_SUBJECT_PROLOG);
	}

	/** The following will not cause a fatal error, it just means
		that on Windows we may construct an invalid "from" address. */
	FromAddress = param("MAIL_FROM");
	
#ifdef WIN32
	/* On WinNT, we need to be given an SMTP server, and we must pass
	 * this servername to the Mailer with a -relay option.
	 */
	if ( (SmtpServer=param("SMTP_SERVER")) == NULL ) {
		dprintf(D_FULLDEBUG,
			"Trying to email, but SMTP_SERVER not specified in config file\n");
		free(Mailer);
		free(FinalSubject);
		if (FromAddress) free(FromAddress);
		return NULL;
	}
#endif 	

	/* Take care of destination email address.  If it is NULL, grab 
	 * the email of the Condor admin from the config file.
	 * We strdup this since we modify it (we split it into tokens so that
	 * each address is a separate argument to the mailer).
	 */
	if ( email_addr ) {
		FinalAddr = strdup(email_addr);
	} else {
		if ( (FinalAddr = param("CONDOR_ADMIN")) == NULL ) {
			dprintf(D_FULLDEBUG,
				"Trying to email, but CONDOR_ADMIN not specified in config file\n");
			free(Mailer);
			free(FinalSubject);
			if (FromAddress) free(FromAddress);
			if (SmtpServer) free(SmtpServer);
			return NULL;
		}
	}

	/* Now tokenize the list of addresses on commas and/or spaces (by replacing
	 * commas and spaces with nils). We also count the addresses here so we
	 * know how large to make our argument vector
	 */
	token_boundary = TRUE;
	num_addresses = 0;
	for (temp = FinalAddr; *temp != '\0'; temp++) {
		if (*temp == ',' || *temp == ' ') {
			*temp = '\0';
			token_boundary = TRUE;
		}
		else if (token_boundary) {
			num_addresses++;
			token_boundary = FALSE;
		}
	}
	if (num_addresses == 0) {
		dprintf(D_FULLDEBUG, "Trying to email, but address list is empty\n");
		free(Mailer);
		free(FinalSubject);
		if (FromAddress) free(FromAddress);
		if (SmtpServer) free(SmtpServer);
		free(FinalAddr);
		return NULL;
	}

	/* construct the argument vector for the mailer */
	//char const * const * final_args;
	const char * * final_args;
	final_args = (char const * *)malloc((8 + num_addresses) * sizeof(char*));
	if (final_args == NULL) {
		EXCEPT("Out of memory");
	}
	arg_index = 0;
	final_args[arg_index++] = Mailer;
	final_args[arg_index++] = "-s";
	final_args[arg_index++] = FinalSubject;
	if (FromAddress) {
		final_args[arg_index++] = "-f";
		final_args[arg_index++] = FromAddress;
	}
	if (SmtpServer) {
		final_args[arg_index++] = "-relay";
		final_args[arg_index++] = SmtpServer;
	}
	temp = FinalAddr;
	for (;;) {
		while (*temp == '\0') temp++;
		final_args[arg_index++] = temp;
		if (--num_addresses == 0) break;
		while (*temp != '\0') temp++;
	}
	final_args[arg_index] = NULL;

/* NEW CODE */
	/* open a FILE* so that the mail we get will end up from condor,
		and not from root */
#ifdef WIN32
	mailerstream = email_open_implementation(Mailer, final_args);
#else
	mailerstream = email_open_implementation(final_args);
#endif

	if ( mailerstream ) {
		fprintf(mailerstream,"This is an automated email from the Condor "
			"system\non machine \"%s\".  Do not reply.\n\n",get_local_fqdn().Value());
	}

	/* free up everything we strdup-ed and param-ed, and return result */
	free(Mailer);
	free(FinalSubject);
	if (FromAddress) free(FromAddress);
	if (SmtpServer) free(SmtpServer);
	free(FinalAddr);
	free(final_args);

	return mailerstream;
}

#ifdef WIN32
FILE *
email_open_implementation(char *Mailer, const char * final_args[])
{
	priv_state priv;
	int prev_umask;
	FILE *mailerstream;

	/* Want the letter to come from "condor" if possible */
	priv = set_condor_priv();
	/* there are some oddities with how popen can open a pipe. In some
		arches, popen will create temp files for locking and they need to
		be of the correct perms in order to be deleted. So the umask is
		set to something useable for the open operation. -pete 9/11/99
	*/
	prev_umask = umask(022);
	mailerstream = my_popenv(final_args,EMAIL_POPEN_FLAGS,FALSE);
	umask(prev_umask);

	/* Set priv state back */
	set_priv(priv);

	if ( mailerstream == NULL ) {	
		dprintf(D_ALWAYS,"Failed to access email program \"%s\"\n",
			Mailer);
	}

	return mailerstream;
}

#else /* unix */

FILE *
email_open_implementation( const char * final_args[])
{

	FILE *mailerstream;
	pid_t pid;
	int pipefds[2];

	/* The gist of this code is to exec a mailer whose stdin is dup2'ed onto
		the write end of a pipe. The parent gets the fdopen'ed read end
		so it looks like a FILE*. The child goes out of its
		way to set its real uid to condor and prop up the environment so
		that any mail that gets sent from the condor daemons ends up as
		comming from the condor account instead of superuser. 

		On some OS'es, the child cannot write to the logs even though the
		mailer process is ruid condor. So I turned off logging in the
		child. I have no clue why this behaviour happens.

		-pete 04/14/2000
	*/

	if (pipe(pipefds) < 0)
	{
		dprintf(D_ALWAYS, "Could not open email pipe!\n");
		return NULL;
	}

	dprintf(D_FULLDEBUG, "Forking Mailer process...\n");
	if ((pid = fork()) < 0)
	{
		dprintf(D_ALWAYS, "Could not fork email process!\n");
		return NULL;
	}
	else if (pid > 0) /* parent */
	{
		/* SIGCLD, SIGPIPE are ignored elsewhere in the code.... */

		/* close read end of pipe */
		close(pipefds[0]);

		mailerstream = fdopen(pipefds[1], EMAIL_POPEN_FLAGS);
		if (mailerstream == NULL)
		{
			dprintf(D_ALWAYS, "Could not open email FILE*: %s\n", 
				strerror(errno));
			return NULL;
		}
		return mailerstream;
	}
	else /* child mailer process */
	{
		static char pe_logname[256]; /* Sorry, putenv wants it this way */
		static char pe_user[256];
		const char *condor_name;
		int i;

		/* Disable any EXCEPT_Cleanup code installed by the parent process.
		   Otherwise, for example, in the master, any call to EXCEPT in
		   the following code will cause us to kill the master's children. */
		_EXCEPT_Cleanup = NULL;

		/* XXX This must be the FIRST thing in this block of code. For some
			reason, at least on IRIX65, this forked process
			will not be able to open the shadow lock file,
			or be able to use dprintf or do any sort of
			logging--even if the ruid hasn't changed. I do
			not know why and this should be investigated. So
			for now, I've turned off logging for this child
			process. Thankfully it is a short piece of code
			before the exec.  -pete 03-05-2000
		*/
		dprintf_set_tool_debug("TOOL", 0);

		/* this is a simple daemon that if it needs to stat . should be
			able to. You might not be able to if the shadow's cwd is in the
			user dir somewhere and not readable by the Condor Account. */
		int ret = chdir("/");
		if (ret == -1) {
			EXCEPT("EMAIL PROCESS: Could not cd /\n");
		}
		umask(0);

		/* Change my userid permanently to "condor" */
		/* WARNING  This code must happen before the close/dup operation. */
		set_condor_priv_final();

		/* close write end of pipe */
		close(pipefds[1]);

		/* connect the write end of the pipe to the stdin of the mailer */
		if (dup2(pipefds[0], STDIN_FILENO) < 0)
		{
			/* I hope this EXCEPT gets recorded somewhere */
			EXCEPT("EMAIL PROCESS: Could not connect stdin to child!\n");
		}

		/* close all other unneeded file descriptors including stdout and
			stderr, just leave the stdin open to this process. */
		for(i = 0; i < sysconf(_SC_OPEN_MAX); i++)
		{
			if (i != pipefds[0] && i != STDIN_FILENO)
			{
				(void)close(i);
			}
		}

		/* prop up the environment with goodies to get the Mailer to do the
			right thing */
		condor_name = get_condor_username();

		/* Should be snprintf() but we don't have it for all platforms */
		sprintf(pe_logname,"LOGNAME=%s", condor_name);
		if (putenv(pe_logname) != 0)
		{
			EXCEPT("EMAIL PROCESS: Unable to insert LOGNAME=%s into "
				" environment correctly: %s\n", pe_logname, strerror(errno));
		}

		/* Should be snprintf() but we don't have it for all platforms */
		sprintf(pe_user,"USER=%s", condor_name);
		if( putenv(pe_user) != 0)
		{
			/* I hope this EXCEPT gets recorded somewhere */
			EXCEPT("EMAIL PROCESS: Unable to insert USER=%s into "
				" environment correctly: %s\n", pe_user, strerror(errno));
		}

		/* invoke the mailer */
		execvp(final_args[0], const_cast<char *const*>(final_args) );

		/* I hope this EXCEPT gets recorded somewhere */
		EXCEPT("EMAIL PROCESS: Could not exec mailer using '%s' with command "
			"'%s' because of error: %s.", "/bin/sh", 
			(final_args[0]==NULL)?"(null)":final_args[0], strerror(errno));
	}

	/* for completeness */
	return NULL;
}
#endif /* UNIX */

FILE *
email_admin_open(const char *subject)
{
	return email_open(NULL,subject);
}

FILE *
email_developers_open(const char *subject)
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
#ifdef NO_PHONE_HOME
		tmp = strdup("NONE");
#else
        tmp = strdup("condor-admin@cs.wisc.edu");
#endif
    } else
    if (strcasecmp (tmp, "NONE") == 0) {
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
	mode_t prev_umask;
	priv_state priv;
	char *customSig;

	if ( mailer == NULL ) {
		return;
	}

	/* Want the letter to come from "condor" if possible */
	priv = set_condor_priv();

	customSig = NULL;
	if ((customSig = param("EMAIL_SIGNATURE")) != NULL) {
		fprintf( mailer, "\n\n");
		fprintf( mailer, "%s", customSig);
		fprintf( mailer, "\n");
		free(customSig);
	} else {
		
		/* Put a signature on the bottom of the email */
		fprintf( mailer, "\n\n-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n" );
		fprintf( mailer, "Questions about this message or Condor in general?\n" );

			/* See if there's an address users should use for help */
		temp = param( "CONDOR_SUPPORT_EMAIL" );
		if( ! temp ) {
			temp = param( "CONDOR_ADMIN" );
		}
		if( temp ) {
			fprintf( mailer, "Email address of the local Condor administrator: "
					 "%s\n", temp );
			free( temp );
		}
		fprintf( mailer, "The Official Condor Homepage is "
				 "http://www.cs.wisc.edu/condor\n" );
	}

	fflush(mailer);
	/* there are some oddities with how pclose can close a file. In some
		arches, pclose will create temp files for locking and they need to
		be of the correct perms in order to be deleted. So the umask is
		set to something useable for the close operation. -pete 9/11/99
	*/
	prev_umask = umask(022);
	/* 
	** we fclose() on UNIX, pclose on win32 
	*/
#if defined(WIN32)
	if (EMAIL_FINAL_COMMAND == NULL) {
		my_pclose( mailer );
	} else {
		char *email_filename = NULL;
		/* Should this be a pclose??? -Erik 9/21/00 */ 
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
	umask(prev_umask);

	/* Set priv state back */
	set_priv(priv);

}




