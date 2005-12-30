/*
 *	This file is licensed under the terms of the Apache License Version 2.0 
 *	http://www.apache.org/licenses. This notice must appear in modified or not 
 *	redistributions of this file.
 *
 *	Redistributions of this Software, with or without modification, must
 *	reproduce the Apache License in: (1) the Software, or (2) the Documentation
 *	or some other similar material which is provided with the Software (if any).
 *
 *	Copyright 2005 Argonne National Laboratory. All rights reserved.
 */

#include "headers.h"

/*
 *	FILE CONTENT:
 *	Global functions
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/8/05	--	coding and testing finished
 *	8/13/05	--	updated randExponential
 *	9/17/05	--	added "strlwr" and "stricmp" for easier porting from Win to Unix
 *	9/23/05	--	added "strnicmp", "skipWhite" and "skipNonWhite"
 */


/*
 *	Method:
 *	void alert(const char* s)
 *
 *	Purpose:
 *	Prints a message. Invoked when an exception is caught.
 *
 *	Input:
 *	string
 *
 *	Output:
 *	none
 */
void alert(const char* s)
{
	fprintf(stderr,">>>>ERROR %s\n",s);
}


/*
 *	Method:
 *	float randExponential( float mean )
 *
 *	Purpose:
 *	Draws a value from the exponential distribution with given mean
 *	using the logarithm method, from Knuth Vol.2 page 133.
 *
 *	Input:
 *	mean
 *
 *	Output:
 *	exponentially distributed value
 */
float randExponential( float mean )
{

	// produce a uniform on an open interval (0,1)
	// RAND_MAX is large in Unix, so addition may exceed bound 
	// thus convert to float, and then add
	double r;
	r = rand();
	r += 1;
	double d = RAND_MAX;
	d += 2;
	r = r/d;

	return - mean * logf((float)r);
}


/*
 *	Method:
 *	float randNormal( float mean, float stdev )
 *
 *	Purpose:
 *	Draws a value from the normal distribution with given mean
 *	and standard deviation using Algorithm P from Knuth Vol.2 page 122.
 *
 *	Input:
 *	mean and standard deviation
 *
 *	Output:
 *	normally distributed value
 */
float randNormal( float mean, float stdev )
{
	double V_1, V_2, S, X_1;

	// draw X_1 from normal with mean 0 and standard deviation 1
	do {
		V_1 = (2*(double)rand())/RAND_MAX - 1 ;	// uniform on [-1,1]
		V_2 = (2*(double)rand())/RAND_MAX - 1 ;	// uniform on [-1,1]
		S = (V_1*V_1)+(V_2*V_2);
	} while( S >= 1 );

	if( 0==S )
		X_1 = 0;
	else
		X_1 = V_1 * sqrt(-2*log(S)/S);

	// scale
	return (float)(X_1*stdev + mean);
}


/*
 *	Method:
 *	char* strlwr(char * tab)
 *
 *	Purpose:
 *	Turns characters of a string into lower case.
 *	This function is only defined in Unix mode,
 *	because Visual Studio .NET already provides a definition.
 *
 *	Input:
 *	string
 *
 *	Output:
 *	string
 */
#ifdef UNIX
char* strlwr(char * tab)
{
	char * beg=tab;
	if( NULL!=tab ) {
		while( *tab ) {
			if( isupper(*tab) )
				*tab = tolower( *tab );
			tab++;
		};
	};
	return beg;
}
#endif


/*
 *	Method:
 *	int stricmp( const char *string1, const char *string2 )
 *
 *	Purpose:
 *	Performs a lowercase comparison between two strings.
 *	This function is only defined in Unix mode,
 *	because Visual Studio .NET already provides a definition,
 *	while Unix has a different name for that function.
 *
 *	Input:
 *	two strings
 *
 *	Output:
 *	0 if equal
 */
#ifdef UNIX
int stricmp( const char *string1, const char *string2 )
{
	return strcasecmp(string1,string2);
}
#endif


/*
 *	Method:
 *	int strnicmp( const char *string1, const char *string2, int len )
 *
 *	Purpose:
 *	Performs a lowercase comparison between two strings up to certain length.
 *	This function is only defined in Unix mode,
 *	because Visual Studio .NET already provides a definition,
 *	while Unix has a different name for that function.
 *
 *	Input:
 *	two strings
 *
 *	Output:
 *	0 if equal
 */
#ifdef UNIX
int strnicmp( const char *string1, const char *string2, int len )
{
	return strncasecmp(string1,string2,len);
}
#endif


/*
 *	Method:
 *	char *skipWhite(char *tab)
 *
 *	Purpose:
 *	Skips white characters of a string, and returns where a non-white begins.
 *
 *	Input:
 *	string
 *
 *	Output:
 *	string beginning at non-while
 */
char *skipWhite(char *tab)
{
	if( NULL==tab )
		return NULL;

	while( *tab==' ' || *tab=='\t' || *tab=='\n' )
		tab++;

	return tab;
}


/*
 *	Method:
 *	char *skipNonWhite(char *tab)
 *
 *	Purpose:
 *	Skips non-white characters of a string, and returns where a white begins.
 *
 *	Input:
 *	string
 *
 *	Output:
 *	string beginning at white
 */
char *skipNonWhite(char *tab)
{
	if( NULL==tab )
		return NULL;

	while( *tab!=' ' && *tab!='\t' && *tab!='\n')
		tab++;

	return tab;
}


/*
 *	Method:
 *	void global_test(void)
 *
 *	Purpose:
 *	Tests the global functions. The pseudorandom distributions
 *	were tested by porting the result to Excel, and looking
 *	at the histogram, the average, and the stdev.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void global_test(void)
{
	// only about 3 values should be beyond the [0,60] interval of 3*stdev around the mean for the normal
	for( int i=0; i<1000; i++ )
		printf("%f\t%f\n", randNormal(30,10), randExponential(10));
	alert("test");

	char *str = strdup("1Ux:");
	printf("%s\n",strlwr(str));

	printf("%d\n", stricmp( "A", "a" ) );
	printf("%d\n", stricmp( "B", "a" ) );
	printf("%d\n", stricmp( "aabDDD", "Edsa" ) );
}
