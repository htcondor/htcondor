/****************************************************************
Purpose:
	Look for a "core" file at a given location.  Return TRUE if it exists
	and appears complete and correct, and FALSE otherwise.  The ULTRIX
	core consists of the uarea followed by the data and stack segments.
	We check the sizes of the data and stack segments recorded in the
	uarea and calculate the size of the core file.  We then check to
	see that the actual core file size matches our calculation.
Bugs:
    OSF1 has problems with sys/user.h and the C++ compiler, this is just a 
    dummy.
******************************************************************/

int
core_is_valid( char *name )
{
	return 1;
}
