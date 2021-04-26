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

static void email_write_headers(FILE *stream,
				const char *FromAddress,
				const char *FinalSubject,
				const char *Addresses,
				int NumAddresses);
static void email_write_header_string(FILE *stream, const char *data);


extern DLL_IMPORT_MAGIC char **environ;

FILE *
email_nonjob_open( const char *email_addr, const char *subject )
{
	char *Sendmail = NULL;
	char *Mailer = NULL;
#ifdef WIN32
	char *SmtpServer = NULL;
#endif
	char *FromAddress = NULL;
	char *FinalSubject;
	char *FinalAddr;
	char *temp;
	int token_boundary;
	int num_addresses;
	int arg_index;
	FILE *mailerstream;

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
			free(FinalSubject);
			if (FromAddress) free(FromAddress);
#ifdef WIN32
			if (SmtpServer) free(SmtpServer);
#endif
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
		free(FinalSubject);
		if (FromAddress) free(FromAddress);
#ifdef WIN32
		if (SmtpServer) free(SmtpServer);
#endif
		free(FinalAddr);
		return NULL;
	}

	Sendmail = param_with_full_path("SENDMAIL");
	Mailer = param("MAIL");

	if ( Mailer == NULL && Sendmail == NULL ) {
		dprintf(D_FULLDEBUG,
			"Trying to email, but MAIL and SENDMAIL not specified in config file\n");
		free(FinalSubject);
		free(FromAddress);
#ifdef WIN32
		free(SmtpServer);
#endif
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
	if (Sendmail != NULL) {
		final_args[arg_index++] = Sendmail;
		// Obtain addresses from the header.
		final_args[arg_index++] = "-t";
		// No special treatment of dot-starting lines.
		final_args[arg_index++] = "-i";
	} else {
		final_args[arg_index++] = Mailer;
		final_args[arg_index++] = "-s";
		final_args[arg_index++] = FinalSubject;

		if (FromAddress) {
#ifdef WIN32
			// condor_mail.exe uses this flag
			final_args[arg_index++] = "-f";
#else
			// modern mailx uses this flag
			final_args[arg_index++] = "-r";
#endif
			final_args[arg_index++] = FromAddress;
		}
#ifdef WIN32
		if (SmtpServer) {
			// SmtpServer is only set on windows
			// condor_mail.exe uses this flag
			final_args[arg_index++] = "-relay";
			final_args[arg_index++] = SmtpServer;
		}
#endif
		temp = FinalAddr;
		for (;;) {
			while (*temp == '\0') temp++;
			final_args[arg_index++] = temp;
			if (--num_addresses == 0) break;
			while (*temp != '\0') temp++;
		}
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

	if ( !mailerstream ) {
		dprintf( D_ALWAYS, "Failed to launch mailer process: %s\n", final_args[0] );
	}
	if ( mailerstream ) {
		if (Sendmail != NULL) {
			email_write_headers(mailerstream,
					    FromAddress,
					    FinalSubject,
					    FinalAddr,
					    num_addresses);
		}

		fprintf(mailerstream,"This is an automated email from the Condor "
			"system\non machine \"%s\".  Do not reply.\n\n",get_local_fqdn().c_str());
	}

	/* free up everything we strdup-ed and param-ed, and return result */
	free(Sendmail);
	free(Mailer);
	free(FinalSubject);
	if (FromAddress) free(FromAddress);
#ifdef WIN32
	if (SmtpServer) free(SmtpServer);
#endif
	free(FinalAddr);
	free(final_args);

	return mailerstream;
}

static void
email_write_header_string(FILE *stream, const char *data)
{
	for (; *data; ++data) {
		char ch = *data;
		if (ch < ' ') {
			fputc(' ', stream);
		} else {
			fputc(ch, stream);
		}
	}
}

static void
email_write_headers(FILE *stream,
		    const char *FromAddress,
		    const char *FinalSubject,
		    const char *Addresses,
		    int NumAddresses)
{
	if (FromAddress) {
		fputs("From: ", stream);
		email_write_header_string(stream, FromAddress);
		fputc('\n', stream);
	}
	fputs("Subject: ", stream);
	email_write_header_string(stream, FinalSubject);
	fputc('\n', stream);

	fputs("To: ", stream);
	for (int i = 0; i < NumAddresses; ++i) {
		if (i > 0) {
			fputs(", ", stream);
		}
		while (*Addresses == '\0') {
			Addresses++;
		}
		email_write_header_string(stream, Addresses);
		Addresses += strlen(Addresses) + 1;
	}
	fputs("\n\n", stream);
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
	mailerstream = my_popenv(final_args,EMAIL_POPEN_FLAGS,FALSE);

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
	ArgList args;
	Env env;
	TemporaryPrivSentry guard( PRIV_CONDOR );

	for ( int i = 0; final_args[i]; i++ ) {
		args.AppendArg( final_args[i] );
	}

	env.Import();
	// I don't think these environment variables are needed.
	// Old commit messages suggest they were necessary at one time for
	// the email to come from the 'condor' user instead of 'root'.
	// Recent testing indicates this is not true on modern Linux.
	env.SetEnv( "LOGNAME", get_condor_username() );
	env.SetEnv( "USER", get_condor_username() );

	dprintf(D_FULLDEBUG, "Forking Mailer process...\n");

	return my_popen( args, "w", 0, &env );
}
#endif /* UNIX */

FILE *
email_admin_open(const char *subject)
{
	return email_nonjob_open(NULL,subject);
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
		fprintf( mailer, "Questions about this message or HTCondor in general?\n" );

			/* See if there's an address users should use for help */
		temp = param( "CONDOR_SUPPORT_EMAIL" );
		if( ! temp ) {
			temp = param( "CONDOR_ADMIN" );
		}
		if( temp ) {
			fprintf( mailer, "Email address of the local HTCondor administrator: "
					 "%s\n", temp );
			free( temp );
		}
		fprintf( mailer, "The Official HTCondor Homepage is "
				 "http://www.cs.wisc.edu/htcondor\n" );
	}

	fflush(mailer);
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

	/* Set priv state back */
	set_priv(priv);

}




