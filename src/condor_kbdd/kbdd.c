/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
** 			 Todd Tannenbaum, IRIX enhancements, UW CS
*/ 


#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>

#include <netdb.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <utmp.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <setjmp.h>
#include "except.h"
#include "sched.h"
#include "debug.h"
#include "condor_uid.h"


char *substr();

#define MINUTES 60

int xfd;           /* X file descriptor */
Display *dpy = 0;  /* display */
Display	*open_display();
int screen;        /* default screen */
Window rootwindow; /* default root window */
XEvent event;      /* For events received. */
jmp_buf	Env;
int		ButtonsGrabbed;	/* True when we have the mouse buttons grabbed */
int		KeyboardGrabbed;/* True when we have the keyboard grabbed */



static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char	*param();
FILE	*popen();

extern int	errno;

int		UdpSock;	/* Send datagrams to the startd */
char	ThisHost[512];
char	*MyName;

char	*Log;
int		PollingFrequency;
int		Foreground = 0;
int		Termlog;

#ifdef X_EXTENSIONS
int		UseXIdleExtension;
#endif X_EXTENSIONS

usage( name )
char	*name;
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t]\n", name );
	exit( 1 );
}

main( argc, argv)
int		argc;
char	*argv[];
{
	char	**ptr;
	int		error_handler(), io_error_handler();
#ifdef X_EXTENSIONS
	int		first_event, first_error;
#endif X_EXTENSIONS

	set_condor_priv();

	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] != '-' ) {
			usage( argv[0] );
		}
		switch( ptr[0][1] ) {
			case 'f':
				Foreground = 1;
				break;
			case 't':
				Termlog++;
				break;
			default:
				usage( argv[0] );
		}
	}

	MyName = *argv;
	config( 0 );
	init_params();

	if( argc > 5 ) {
		usage( argv[0] );
	}
	
		/* This is so if we dump core it'll go in the log directory */
	if( chdir(Log) < 0 ) {
		EXCEPT( "chdir to log directory <%s>", Log );
	}

		/* Arrange to run in background */
	if( !Foreground ) {
		if( fork() )
			exit( 0 );
	}

		/* Set up logging */
	dprintf_config( "KBDD", 2 );

	dprintf( D_ALWAYS, "**************************************************\n" );
	dprintf( D_ALWAYS, "***          CONDOR_KBDD STARTING UP           ***\n" );
	dprintf( D_ALWAYS, "**************************************************\n" );
	dprintf( D_ALWAYS, "\n" );

	if( gethostname(ThisHost,sizeof ThisHost) < 0 ) {
		EXCEPT( "gethostname" );
	}

	UdpSock = udp_connect( ThisHost, START_UDP_PORT );

	XSetErrorHandler( error_handler );
	XSetIOErrorHandler( io_error_handler );
	setjmp( Env );
	wait_until_X_active( PollingFrequency );

	/* X11R4 has new security features.  Need to be root here. */
	set_root_priv();

		/* open the display and determine some well-known parameters */
	dpy = open_display();

	set_condor_priv();

    screen = DefaultScreen(dpy);
    rootwindow = RootWindow(dpy, screen);
    xfd = ConnectionNumber(dpy);

#ifdef X_EXTENSIONS
	if( UseXIdleExtension ) {
		if( !XidleQueryExtension(dpy,&first_event,&first_error) ) {
			dprintf( D_FULLDEBUG, "XQueryExtension failed.\n");
			UseXIdleExtension = FALSE;
		}
	}
#endif X_EXTENSIONS

#ifdef X_EXTENSIONS
	if( UseXIdleExtension ) {
		dprintf( D_ALWAYS, "Using XGetIdleTime to monitor input events\n" );
	} else {
		dprintf( D_ALWAYS,
			"Using XGrabKey and XGrabButton to monitor input events\n" );
	}
#else
	dprintf( D_ALWAYS,
			"Using XGrabKey and XGrabButton to monitor input events\n" );
#endif

	for(;;) {
#ifdef X_EXTENSIONS
		if( UseXIdleExtension ) {
			poll_for_event();
		} else {
			wait_for_event();
		}
#else X_EXTENSIONS
		wait_for_event();
#endif X_EXTENSIONS
		update_startd();
		sleep( PollingFrequency );
	}
}

XDR		xdr, *xdrs, *xdr_Udp_Init();

update_startd()
{
	int		cmd;

	if( !xdrs ) {
		xdrs = xdr_Udp_Init( &UdpSock, &xdr );
		xdrs->x_op = XDR_ENCODE;
		dprintf( D_ALWAYS, "Initialized XDR stream\n" );
	}

		/* Send the command */
	cmd = X_EVENT_NOTIFICATION;
	if( !xdr_int(xdrs, &cmd) ) {
		xdr_destroy( xdrs );
		dprintf( D_ALWAYS, "xdr_int() failed, destroyed XDR stream\n" );
		xdrs = (XDR *)0;
		return -1;
	}

	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		xdr_destroy( xdrs );
		dprintf(D_ALWAYS,"xdrrec_endofrecord() failed, destroyed XDR stream\n");
		xdrs = (XDR *)0;
		return -1;
	}

	dprintf( D_FULLDEBUG, "Sent notification\n" );
}

init_params()
{
	char	*tmp;

	Log = param( "LOG" );
	if( Log == NULL )  {
		EXCEPT( "No log directory specified in config file\n" );
	}

	tmp = param( "POLLING_FREQUENCY" );
	if( tmp == NULL ) {
		PollingFrequency = 30;
	} else {
		PollingFrequency = atoi( tmp );
	}

	if( param("KBDD_DEBUG") == NULL ) {
		EXCEPT( "KBDD_DEBUG not defined in config file\n" );
	}
	Foreground += param_in_pattern( "KBDD_DEBUG", "Foreground" );

#ifdef X_EXTENSIONS
    tmp = param("USE_X_IDLE_EXTENSION");
    if( tmp && (*tmp == 't' || *tmp == 'T') ) {
		dprintf(D_ALWAYS, "Using XIdle extension\n");
        UseXIdleExtension = TRUE;
    } else {
        UseXIdleExtension = FALSE;
    }
#endif X_EXTENSIONS
}

#ifdef X_EXTENSIONS
#define MSECSPERSEC 1000	/* 1000 milliseconds per second */
/*
** If our X server implements the XGetIdleTime extension, we can just
** ask how long it has been since the last keyboard or mouse button event.
** This is really the way to go, and if your X server doesn't have
** this extension, I highly recomment you get it from MIT. -- mike
*/
poll_for_event()
{
	Time	idle_time;
	int		idle_secs;

	for(;;) {
		/* idle time returned in millisecs. */
		if( XGetIdleTime(dpy, &idle_time) ) {
			idle_secs = idle_time / MSECSPERSEC;
			dprintf( D_FULLDEBUG, "XGetIdleTime returns %d\n", idle_secs );
			if( idle_secs < PollingFrequency ) {
				return;
			}
			sleep( PollingFrequency );
		} else {
			dprintf( D_FULLDEBUG, "XGetIdleTime returned 0\n");
		}
	}

}
#endif X_EXTENSIONS

/*
** If our X server doesn't implement the XGetIdleTime extension, we can't
** get direct information on when the last event occured.  In that case
** we use a rather kludgy tactic.  We attempt to grab all keyboard and
** mouse events, then wait for one to occur.  Once this happens, we
** ungrab the keyboard and mouse, and replay those events we grabbed.
** By grabbing and replaying a few of the events we can determine whether
** the keyboard/mouse are active...
*/
wait_for_event()
{
		/* Grab key and mouse events */
	KeyboardGrabbed = TRUE; /* will be set FALSE if error occurs */
	XGrabKey(dpy, AnyKey, AnyModifier, rootwindow, False,
		 GrabModeSync, GrabModeSync);

	ButtonsGrabbed = TRUE;	/* will be set FALSE if error occurs */
	XGrabButton(dpy, AnyButton, AnyModifier, rootwindow, False,
		 ButtonPressMask, GrabModeSync, GrabModeSync, None, None );

	if( KeyboardGrabbed ) {
		dprintf( D_FULLDEBUG, "Keyboard grabbed\n" );
	}

	if( ButtonsGrabbed ) {
		dprintf( D_FULLDEBUG, "Buttons grabbed\n" );
	}

		/* Block here until we get an event */
	if( KeyboardGrabbed || ButtonsGrabbed ) {
		XNextEvent(dpy, &event);
	}

		/* Ungrab key and mouse, and replay any events we grabbed */
	if( KeyboardGrabbed ) {
		XUngrabKey(dpy, AnyKey, AnyModifier, rootwindow);
		XAllowEvents(dpy, ReplayKeyboard, CurrentTime);
		KeyboardGrabbed = FALSE;
	}

	if( ButtonsGrabbed ) {
		XUngrabButton(dpy, AnyButton, AnyModifier, rootwindow);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		ButtonsGrabbed = FALSE;
	}
	XFlush(dpy);

}

/*
** If we try to connect to the X server while it is grabbed by the init
** process, (during the time nobody is logged in via the monitor), we
** will time out.  X makes this difficult to handle in several ways,
** 1. we're left with an open file descriptor we can't use, but we don't
** know which one, 2. X insists we should exit when we return from the
** error handler, so we have to avoid that with a longjump, 3. if we
** do this enough times, it breaks the server, (probably it also leaves
** a dangling file descriptor or some such).  Since connecting to the
** X server while it is grabbed is such a bad thing to do, we try to avoid
** it here.
** 
** We do this by attempting to determine whether a user is logged on and
** using X by monitoring the process table.  We assume that if the user
** logs on to a system which is not already running X, then he or she
** would start X with "xinit".  If the user logs into a machine which
** is already running X, (via the login box), then X will have been
** started by "xdm".  In this case a childless "xdm" would indicate
** the presence of the login box, (X server grabbed), and an "xdm" with
** some child process would indicate a user already logged in, (X server
** not grabbed).  For portability, we monitor the process table by running
** "ps", but this is expensive.  To avoid this expense when the machine is busy
** we wait for the machine to appear idle by all other criteria.  In the
** absence of further information about keyboard activity from us, the
** startd will mark the machine as "idle" when tty's and pty's are idle,
** and the load average is low.  Only when that happens will we search the 
** process table for evidence of X activity.
** 
** This is all a bit arcane and ugly.  I will welcome any suggestions on a
** better way -- mike (mike@cs.wisc.edu).
*/
wait_until_X_active( period )
int		period;
{
	for(;;) {
		if( get_machine_status() != M_IDLE ) {
			dprintf( D_FULLDEBUG, "Machine not Idle, avoiding PS\n" );
		} else {
			if( X_is_active() ) {
				return;
			}
		}
		sleep( period );
	}
}

X_is_active()
{
	FILE    *fp;
	char	buf[1024];
	char	*command, *parse_ps_line();

	dprintf( D_FULLDEBUG, "Searching process table\n" );

#if defined(AIX31) || defined(AIX32) || defined(OSF1) 
	if( (fp=popen("ps -ef","r")) == NULL ) {
		EXCEPT( "popen(\"ps -ef\",\"r\")" );
	}
#elif defined(Solaris) | defined(IRIX62) | defined(IRIX53)
	if( (fp=popen("/bin/ps -ef","r")) == NULL ) {
		EXCEPT( "popen(\"/bin/ps -ef\",\"r\")" );
	} 
#else
	if( (fp=popen("ps -axlwww","r")) == NULL ) {
		EXCEPT( "popen(\"ps -axlwww\",\"r\")" );
	}
#endif

	(void)fgets( buf, sizeof(buf), fp );	/* throw away header */
	while( fgets(buf,sizeof(buf),fp) ) {
		command = parse_ps_line( buf );

			/* X started explicitly by user */
		if( substr(command,"xinit") ) {
			dprintf( D_ALWAYS, "Found xinit!\n" );
			pclose( fp );
			return TRUE;
		}

			/* User logged in via login box (X) */
			/* THIS LOOKS WRONG TO ME; AN XDM CHILD DOES NOT MEAN ANYTHING
			 * IMHO, -Todd 2/97 */
#ifdef BULLSHIT
		if( substr(command,"(xdm)") ) {
			dprintf( D_ALWAYS, "Found xdm child!\n" );
			pclose( fp );
			return TRUE;
		}
#endif

			/* User logged in via login box (DecWindows) */
		if( substr(command,"dxsession") && !substr(command,"login") ) {
			dprintf( D_ALWAYS, "Found dxsession!\n" );
			pclose( fp );
			return TRUE;
		}
#if defined(IRIX53) || defined(IRIX62)
			/* The SGI visuallogin process is running, so nobody on console */
			/* we should check for this before we check for 4Dwm */
		if( substr(command,"clogin") ) {
			dprintf(D_ALWAYS, "Found clogin, nobody on console\n");
			pclose(fp);
			return FALSE;
		}

			/* User running the SGI windows manager */
			/* this is a weak test, as the user could be running the window
			 * manager for a remote display like an xterm, but geeez, we dont
			 * have much choice .... */
		if( substr(command,"4Dwm") ) {
			dprintf(D_ALWAYS,"Found 4Dwm!\n");
			pclose(fp);
			return TRUE;
		}
#endif  /* IRIX */

	}
	pclose( fp );
	return FALSE;
}


/*
** Seems like the normal case is to get two X errors in a row.  If we get
** more than that in 2 minute, abort and leave a core so somebody can
** investigate.
*/
#define MIN_ERROR_INTERVAL  (10 * MINUTES) /* must be > 2 * X_STARTUP_INTERVAL*/
#define X_STARTUP_INTERVAL  (3 * MINUTES)
long	LastError, SecondLastError;

error_handler( d, event )
Display		*d;
XErrorEvent	*event;
{
	time_t	now;
	char	error_text[512];

	if( event->request_code == X_GrabButton && event->error_code == BadAccess) {
		dprintf( D_FULLDEBUG, "Can't grab mouse buttons\n" );
		ButtonsGrabbed = FALSE;
		return;
	}

	if( event->request_code == X_GrabKey && event->error_code == BadAccess) {
		dprintf( D_FULLDEBUG, "Can't grab keyboard\n" );
		KeyboardGrabbed = FALSE;
		return;
	}

	dprintf( D_ALWAYS, "Got X Error, errno = %d\n", errno );
	dprintf( D_ALWAYS, "\ttype = %d\n", event->type );
	dprintf( D_ALWAYS, "\tdisplay = 0x%x\n", event->display );
	dprintf( D_ALWAYS, "\tserial = %d\n", event->serial );
	dprintf( D_ALWAYS, "\terror_code = %d\n", event->error_code );
	dprintf( D_ALWAYS, "\trequest_code = %d\n", event->request_code );
	dprintf( D_ALWAYS, "\tminor_code = %d\n", event->minor_code );

	XGetErrorText( event->display, event->error_code, error_text,
														sizeof(error_text) );
	dprintf( D_ALWAYS, "\ttext = \"%s\"\n", error_text );

#if !defined(OSF1) && !defined(LINUX) && !defined(Solaris) && !defined(IRIX62) && !defined(IRIX53)
	if( close(d->fd) == 0 ) {
		dprintf( D_ALWAYS, "Closed display fd (%d)\n", d->fd );
	} else {
		dprintf( D_ALWAYS, "Can't close display fd (%d)\n", d->fd );
	}
#endif

		/* Try this for debugging */
	/*
	sleep( PollingFrequency );
	longjmp( Env, 1 );
	*/

	(void)time( &now );
	if( now - SecondLastError < MIN_ERROR_INTERVAL ) {
		abort();
	} else {
		SecondLastError = LastError;
		LastError = now;
		longjmp( Env, 1 );
	}
}

io_error_handler( d )
Display		*d;
{
	time_t	now;

	dprintf( D_ALWAYS, "Got X I/O Error, errno = %d\n", errno );

#if !defined(OSF1) && !defined(LINUX) && !defined(Solaris) && !defined(IRIX62) && !defined(IRIX53)
	if( close(d->fd) == 0 ) {
		dprintf( D_ALWAYS, "Closed display fd (%d)\n", d->fd );
	} else {
		dprintf( D_ALWAYS, "Can't close display fd (%d)\n", d->fd );
	}
#endif

	(void)time( &now );
	if( now - SecondLastError < MIN_ERROR_INTERVAL ) {
		exit( 1 );
	} else {
		SecondLastError = LastError;
		LastError = now;
		sleep( X_STARTUP_INTERVAL );
		longjmp( Env, 1 );
	}
}

SetSyscalls(){}

#define MAX_DISPLAY	50
Display	*
open_display()
{
	char    sock_name[512];
	int		i;

	for( i=0; i<MAX_DISPLAY; i++ ) {
		sprintf( sock_name, "%s:%d", ThisHost, i );
		dprintf( D_ALWAYS, "Attempting to connect to X server on '%s'\n",
																sock_name );
		if( dpy = XOpenDisplay(sock_name) ) {
			dprintf( D_ALWAYS, "Connection completed on '%s'\n", sock_name );
			return dpy;
		}
		dprintf( D_ALWAYS, "Connection failed on '%s'\n", sock_name );
	}
	EXCEPT( "unable to open display '%s:0' - '%s:%d'\n",
											ThisHost, ThisHost, MAX_DISPLAY );
}

/*
** Given a line of output from "ps", return just the command and its args.
*/
char *
parse_ps_line( str )
char	*str;
{
	char	*answer, *end, *strchr();

	dprintf( D_FULLDEBUG, "%s", str );
	answer = strchr( str, ':' ) + 4;
	if( end = strchr(answer,'\n') ) {
		*end = '\0';
	}
		
	dprintf( D_FULLDEBUG, "\"%s\"\n", answer );
	return answer;
}

#if 0 /* moved to "util_lib/strcmp.c" */
/*
** Return a pointer to the first occurence of pattern in string.
*/
char *
substr( string, pattern )
char	*string;
char	*pattern;
{
	char	*str, *s, *p;
	int		n;

	dprintf( D_FULLDEBUG,"String: \"%s\"\n", string );
	dprintf( D_FULLDEBUG,"Pattern: \"%s\"\n", pattern );

	n = strlen( pattern );
	for( str=string; *str; str++ ) {
		if( strncmp(str,pattern,n) == 0 ) {
			return str;
		}
	}
	return NULL;
}
#endif
