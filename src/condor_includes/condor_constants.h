#ifndef CONSTANTS_H
#define CONSTANTS_H


#if !defined(__STDC__) && !defined(__cplusplus)
#define const
#endif

/*
	Set up a boolean variable type.  Since this definition could conflict
	with other reasonable definition of BOOLEAN, i.e. using an enumeration,
	it is conditional.
*/
#ifndef BOOLEAN_TYPE_DEFINED
typedef int BOOLEAN;
typedef int BOOL_T;
#endif

#if defined(TRUE)
#	undef TRUE
#	undef FALSE
#endif

static const int	TRUE = 1;
static const int	FALSE = 0;

/*
	Useful constants for turning seconds into larger units of time.  Since
	these constants may have already been defined elsewhere, they are
	conditional.
*/
#ifndef TIME_CONSTANTS_DEFINED
static const int	MINUTE = 60;
static const int	HOUR = 60 * 60;
static const int	DAY = 24 * 60 * 60;
#endif

/*
  This is for use with strcmp() and related functions which will return
  0 upon a match.
*/
#ifndef MATCH
static const int	MATCH = 0;
#endif
#endif
