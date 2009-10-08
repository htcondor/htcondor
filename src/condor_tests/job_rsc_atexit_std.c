#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* Allow us to checkpoint and exit */
#if defined(__cplusplus)
extern "C" void ckpt_and_exit();
#else
extern void ckpt_and_exit();
#endif

void success(void);

/* The purpose of this program is to test that atexit() is handled properly.
	On glibc26 and later ports of stduniv, glibc mangles the runtime pointers
	for the functions registered with atexit (see condor_ckpt/machdep.h).
	I just want to make sure it all still works.
*/

void success(void)
{
	printf("SUCCESS\n");
	/* get my message out to disk, since _exit() might not flush fds */
	fflush(NULL);
	sync();

	/* One might argue that a checkpoint and restart should happen inside
		one of these handlers for more robustness testing.

		However, so sayeth posix:

		"POSIX.1-2001 says that the result is undefined if
		longjmp(3) is used to terminate execution of
		one of the functions registered atexit()."

		Of course, this does raise the question of what happens during
		a checkpoint and restart of a process in an atexit() handler...
	*/

	/* While exit() may not be called (posix says it is undefined), _exit()
		may be called. So here we terminate the process, leaving any
		additionally registered function calls (of which there are none in
		this test) uninvoked.
	*/

	_exit(EXIT_SUCCESS);
}

int main(void)
{
	atexit(success);

	/* save the pointers, exit, restart, restore the pointers */
	ckpt_and_exit();

	/* If this next statement actually happens, of a segfault happens, this
		test is a failure.
	*/
	return EXIT_FAILURE;
}


