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
*/ 


	/* struct for command and option tables */
struct table {
	char	*t_string;
	int 	t_code;
};
typedef struct table TABLE;

	/* range of values in a TABLE */
struct rnge {
	int	high;
	int low;
};
typedef struct rnge RNGE;

#define to_lower(a) (((a)>='A'&&(a)<='Z')?((a)|040):(a))


#define DELSTR	"\177\177\177\177\177\177\177\177\177\177"
#define NOCOM	 		998
#define AMBIG			999

/* Commands */
#define		C_COMMAND		1
#define		C_ARGUMENTS		2
#define		C_ERROR			3
#define		C_INPUT			4
#define		C_OUTPUT		5

#define		C_PREFERENCES	7
#define		C_REQUIREMENTS	8
#define		C_PRIO			9
#define		C_ROOTDIR		10
#define		C_QUEUE			11
#define		C_NOTIFICATION	12
