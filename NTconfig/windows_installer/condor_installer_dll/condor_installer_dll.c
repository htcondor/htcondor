#include <Winsock2.h>
#include <windows.h>

#include   <msi.h>
#include   <msiquery.h>
#include   <stdio.h>

/* This is a utility DLL for the Condor installer. It's basically a
 * collection of handy functions we call up from the MSI installer as
 * Custom Actions. If you add a function, be sure to add it to the
 * .def file so its exported.
 */

BOOL get_hostname(char *buf, int sz);


BOOL __stdcall set_daemon_list(LPTSTR buf, int submit_jobs, int run_jobs, int new_pool){

	int offset, bufsiz;

	bufsiz = strlen(buf);

	offset = _snprintf(buf, bufsiz, "MASTER");
	
	if ( new_pool > 0 ) {
		offset += _snprintf(buf+offset, (bufsiz-offset), " COLLECTOR");
		offset += _snprintf(buf+offset, (bufsiz-offset), " NEGOTIATOR");
	}

	if ( submit_jobs > 0 ) {
		offset += _snprintf(buf+offset, (bufsiz-offset), " SCHEDD");
	}

	if ( run_jobs > 0 ) {
		offset += _snprintf(buf+offset, (bufsiz-offset), " STARTD");
	}

	return TRUE;
}

#define MAXHOSTNAMELEN 64
UINT __stdcall 
get_full_hostname(LPTSTR input) {
	char buf[MAXHOSTNAMELEN];
	int len;

	if ( get_hostname(buf, MAXHOSTNAMELEN) ) {
		
		// we don't want to trample memory, so we 
		// only write as many characters as there are
		// in the string we were passed.
		
		len = strlen(input);

		strncpy(input, buf, len);
		return TRUE;
	}
	
	// set input string to the empty string.
	input[0] = '\0';
	return FALSE;
}


	/* tell us the DNS domain for this machine. */
BOOL __stdcall 
get_domain(LPTSTR input) {

	char buf[MAXHOSTNAMELEN];
	int len;

	if ( get_hostname(buf, MAXHOSTNAMELEN) ) {
		char* ptr;
		
		ptr = strchr(buf, '.');
		
		if ( ptr ) {
			ptr++;

			// make sure when you call this the input string is really long 
			// so we don't wind up trampling memory.
			len = strlen(input);

			strncpy(input, ptr, len);
			return TRUE;
		} else {
			len = strlen(input);
			strncpy(input, "your.domain.here", len);
		}
	}

	// set input string to the empty string.
	input[0] = '\0';
	return FALSE;	
}

	/* opens up a file and appends the given string */
UINT __stdcall 
append_to_file( LPTSTR file, LPTSTR str ) {
	return 0;

}

	/* takes a string. If it ends with '\', we snip it off */
UINT __stdcall 
chomp_backslash ( LPTSTR input) {
	int a;
	
	if ( input != NULL ) {
		a = strlen(input);

		if ( a > 0 ) {
			if ( input[a-1] == '\\' ) {
				input[a-1] = '\0';
			}
		}
	}

	return 0;
}


BOOL 
get_hostname(char *buf, int sz) {

	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	char hostbuf[MAXHOSTNAMELEN];

	
	wVersionRequested = MAKEWORD( 2, 0 );
 
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		return FALSE;
	}
	
	if ( gethostname(hostbuf, sizeof(hostbuf)) == 0 ) {
		HOSTENT *a;

		a = gethostbyname(hostbuf);
		
		if ( a ) {
			return _snprintf(buf, sz, "%s", a->h_name);
		} else {
			return FALSE;
		}
	}

	return FALSE;
}

BOOL APIENTRY DllMain( HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved )
{
    return TRUE;
}

UINT __stdcall SampleFunction2 ( MSIHANDLE hModule )
{
    MessageBox(NULL, "Hello world", "CodeProject.com", MB_OK);
    return ERROR_SUCCESS;
}
