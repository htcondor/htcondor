#include "startd.h"
static char *_FileName_ = __FILE__;

int
calc_disk()
{
	int free_disk;

	free_disk = free_fs_blocks(exec_path);
	if (free_disk <= 512)
		return 0;
	return free_disk - 512;
}


#if defined(OSF1)
#	define MAXINT ((1<<30)-1)
#else
#	define MAXINT ((1<<31)-1)
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#if defined(OSF1)
static char *UtmpName = "/var/adm/utmp";
static char *AltUtmpName = "/etc/utmp";
#elif defined(LINUX)
static char *UtmpName = "/var/run/utmp";
static char *AltUtmpName = "/var/adm/utmp";
#else
static char *UtmpName = "/etc/utmp";
static char *AltUtmpName = "/var/adm/utmp";
#endif

/* calc_idle_time fills in user_idle and console_idle with the number
 * of seconds since there has been activity detected on any tty or on
 * just the console, respectively.  therefore, console_idle will always
 * be equal to or greater than user_idle.  think about it.  also, note
 * that user_idle and console_idle are passed by reference.  Also,
 * on some platforms console_idle is always -1 because it cannot reliably
 * be determined.
 */
void
calc_idle_time( int & user_idle, int & console_idle )
{
	int tty_idle;
	int now;
	struct utmp utmp;
	ELEM temp;
#if defined(HPUX9) || defined(LINUX)	/*Linux uses /dev/mouse	*/
	int i;
	char devname[MAXPATHLEN];
#endif

	console_idle = -1;  // initialize

	user_idle = tty_pty_idle_time();

	if (kbd_dev) {
		tty_idle = tty_idle_time(kbd_dev);
		user_idle = MIN(tty_idle, user_idle);
		console_idle = tty_idle;
	}

	if (mouse_dev) {
		tty_idle = tty_idle_time(mouse_dev);
		user_idle = MIN(tty_idle, user_idle);
		if (console_idle != -1)
			console_idle = MIN(tty_idle, console_idle);
	}
/*
** On HP's "/dev/hil1" - "/dev/hil7" are mapped to input devices such as
** a keyboard, mouse, touch screen, digitizer, etc.We can then conveniently
** stat those, and avoid having to run the condor_kbd at all.
** Note: the "old-style" hp keyboards go to /dev/hil?, but the newer
** PC 101-key style keyboard gets mapped to /dev/ps2kbd and /dev/ps2mouse
*/
#if defined(HPUX9)
	for(i = 1; i <= 7; i++) {
		sprintf(devname, "hil%d", i);
		tty_idle = tty_idle_time(devname);
		if (i == 1)
			console_idle = tty_idle;
		else
			console_idle = MIN(tty_idle, console_idle);
	}
	tty_idle = tty_idle_time("ps2kbd");
	console_idle = MIN(tty_idle, console_idle);
	tty_idle = tty_idle_time("ps2mouse");
	console_idle = MIN(tty_idle, console_idle); 

	user_idle = MIN(console_idle, user_idle);
#endif

/*
 * Linux usally has a /dev/mouse linked to ttySx or psaux, etc.  Use it!
 *
 * stat() will follow the link to the apropriate ttySx
 */
#if defined(LINUX)
	sprintf(devname, "mouse");
	tty_idle = tty_idle_time(devname);
	user_idle = MIN( tty_idle, user_idle );
#endif

	now = (int)time((time_t *)0);
	user_idle = MIN(now - last_x_event, user_idle);

	/* if Last_X_Event != 0, then condor_kbdd told us someone did something
	 * on the console. but always believe a /dev device over the kbdd */
	if ( (console_idle == -1) && (last_x_event != 0) ) {
		console_idle = now - last_x_event;
	}

	dprintf(D_FULLDEBUG, "Idle Time: user= %d , console= %d seconds\n",
			user_idle,console_idle);
	
	return;
}

#if !defined(OSF1) /* in tty_idle.OSF1.C for that platform */
#include <limits.h>
int
tty_pty_idle_time()
{
	FILE *fp;
	int tty_idle;
	int answer = INT_MAX;
	int now;
	static int saved_now;
	static int saved_idle_answer = -1;
	struct utmp utmp;

	if ((fp=fopen(UtmpName,"r")) == NULL) {
		if ((fp=fopen(AltUtmpName,"r")) == NULL) {
			EXCEPT("fopen of \"%s\"", UtmpName);
		}
	}

	while (fread((char *)&utmp, sizeof utmp, 1, fp)) {
#if defined(AIX31) || defined(AIX32) || defined(IRIX331) || defined(IRIX53) || defined(LINUX)
		if (utmp.ut_type != USER_PROCESS)
#else
			if (utmp.ut_name[0] == '\0')
#endif
				continue;

		tty_idle = tty_idle_time(utmp.ut_line);
		answer = MIN(tty_idle, answer);
	}
	fclose(fp);

	/* Here we check to see if we are about to return INT_MAX.  If so,
	 * we recompute via the last pty access we knew about.  -Todd, 2/97 */
	now = (int)time( (time_t *)0 );
	if ( (answer == INT_MAX) && ( saved_idle_answer != -1 ) ) {
		answer = (now - saved_now) + saved_idle_answer;
		if ( answer < 0 )
			answer = 0; /* someone messed with the system date */
	} else {
		if ( answer != INT_MAX ) {
			/* here we are returning an answer we discovered; save it */
			saved_idle_answer = answer;
			saved_now = now;
		}
	}

	return answer;
}
#endif

int
tty_idle_time(char* file)
{
	struct stat buf;
	long now;
	long answer;
	static char pathname[100] = "/dev/";

	strcpy( &pathname[5], file );
	if (stat(pathname,&buf) < 0) {
		dprintf( D_FULLDEBUG, "Error on stat(%s,0x%x), errno = %d\n",
				 pathname, &buf, errno );
		buf.st_atime = 0;
	}

	now = (int)time((time_t *)0);
	answer = now - buf.st_atime;

	/*
	 * This only happens if somebody screws up the date on the
	 * machine :-<
	 */
	if (answer < 0)
		answer = 0;

	return answer;
}


int
calc_ncpus()
{
#ifdef sequent
	int     cpus = 0;
	int     eng;

	if ((eng = tmp_ctl(TMP_NENG,0)) < 0) {
		perror( "tmp_ctl(TMP_NENG,0)" );
		exit(1);
	}

	while (eng--) {
		if( tmp_ctl(TMP_QUERY,eng) == TMP_ENG_ONLINE )
			cpus++;
	}
	return cpus;
#elif defined(Solaris)
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(IRIX53)
	return sysmp(MP_NPROCS);
#else sequent
	return 1;
#endif sequent
}


int
calc_mips()
{
	return dhry_mips();
}


int
calc_kflops()
{
	return clinpack_kflops();
}
