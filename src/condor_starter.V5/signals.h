/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#include <signal.h>
#define N_POSIX_SIGS 19

void display_sigset( char * msg, sigset_t * mask );

/* pointer to function taking one integer arg and returning void */
typedef void (*SIGNAL_HANDLER)(int);

class EventHandler {
public:
	EventHandler( void (*f)(int), sigset_t m );
	void install();
	void de_install();
	void allow_events( sigset_t & );
	void block_events( sigset_t & );
	void display();
private:
	SIGNAL_HANDLER		func;
	sigset_t			mask;
	struct sigaction	o_action[ N_POSIX_SIGS ];
	int  				is_installed;
};


#if 0
class CriticalSection {
public:
	begin();
	end();
	deliver_one();
private:
	setset_t		save_mask;
	int				is_critical;
}
#endif
