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

#ifndef __libgxx_sys_dir_h

extern "C" {

#ifdef __sys_dir_h_recursive
#include_next <sys/dir.h>
#else
#define __sys_dir_h_recursive
#define opendir __hide_opendir
#define closedir __hide_closedir
#define readdir __hide_readdir
#define telldir __hide_telldir
#define seekdir __hide_seekdir
#if ! (defined(__ultrix__) || defined(__sun__) || defined(AIX32)) 
#define rewinddir __hide_rewinddir
#endif

#include_next <sys/dir.h>

#define __libgxx_sys_dir_h
#undef opendir
#undef closedir
#undef readdir
#undef telldir
#undef seekdir
#if ! (defined(__ultrix__) || defined(__sun__) || defined(AIX32))
#undef rewinddir
#endif

DIR *opendir(const char *);
int closedir(DIR *);
#ifdef __dirent_h_recursive
// Some operating systems we won't mention (such as the imitation
// of Unix marketed by IBM) implement dirent.h by including sys/dir.h,
// in which case sys/dir.h defines struct dirent, rather than
// the struct direct originally used by BSD.
struct dirent *readdir(DIR *);
#else
struct direct *readdir(DIR *);
#endif
long telldir(DIR *);
void seekdir(DIR *, long);
#ifndef rewinddir
void rewinddir(DIR *);
#endif
#endif
}

#endif
