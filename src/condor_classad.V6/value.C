#include "condor_common.h"
#include "operators.h"
#include "value.h"

namespace classad {

StringSpace Value::stringSpace( 256 );

Value::
Value() : strValue( )
{
	valueType = UNDEFINED_VALUE;
	booleanValue = false;
	integerValue = 0;
	realValue = 0.0;
	listValue = NULL;
	classadValue = NULL;
	timeValueSecs = 0;
	timeValueUSecs = 0;
}


Value::
~Value()
{
	Clear();
}


void Value::
Clear()
{
	switch( valueType ) {
		case LIST_VALUE:
			// list values live in the evaluation environment, so they must
			// never be explicitly destroyed
			listValue = NULL;
			break;

		case CLASSAD_VALUE:
			// classad values live in the evaluation environment, so they must 
			// never be explicitly destroyed
			classadValue = NULL;
			break;

		case STRING_VALUE:
			strValue.dispose( );
			break;

		default:
			valueType = UNDEFINED_VALUE;
	}
	valueType 	= UNDEFINED_VALUE;
}


bool Value::
IsNumber (int &i)
{
	switch (valueType) {
		case INTEGER_VALUE:
			i = integerValue;
			return true;

		case REAL_VALUE:
			i = (int) realValue;	// truncation	
			return true;

		default:
			return false;
	}
}


bool Value::
IsNumber (double &r)
{
	switch (valueType) {
		case INTEGER_VALUE:
			r = (double) integerValue;
			return true;

		case REAL_VALUE:
			r = realValue;	
			return true;

		default:
			return false;
	}
}


void Value::
CopyFrom( Value &val )
{
	switch (val.valueType) {
		case STRING_VALUE:
			Clear( );
			strValue.copy( val.strValue );
			valueType = STRING_VALUE;
			return;

		case BOOLEAN_VALUE:
			SetBooleanValue( val.booleanValue );
			return;

		case INTEGER_VALUE:
			SetIntegerValue( val.integerValue );
			return;

		case REAL_VALUE:
			SetRealValue( val.realValue );
			return;

		case UNDEFINED_VALUE:
			SetUndefinedValue( );
			return;

		case ERROR_VALUE:
			SetErrorValue( );
			return;
	
		case LIST_VALUE:
			SetListValue( val.listValue );
			return;

		case CLASSAD_VALUE:
			SetClassAdValue( val.classadValue );
			return;

		case ABSOLUTE_TIME_VALUE:
			SetAbsoluteTimeValue( val.timeValueSecs );
			return;

		case RELATIVE_TIME_VALUE:
			SetRelativeTimeValue( val.timeValueSecs, val.timeValueUSecs );
			return;

		default:
			SetUndefinedValue ();
	}	
}


bool Value::
ToSink (Sink &dest )
{
	char 	tempBuf[512];
	time_t 	clock;
	FormatOptions *p = dest.GetFormatOptions( );
	bool	wantQuotes = p ? p->GetWantQuotes( ) : true;

    switch (valueType) {
      	case UNDEFINED_VALUE:
        	return (dest.SendToSink((void*)"undefined", strlen("undefined")));

      	case ERROR_VALUE:
        	return (dest.SendToSink ((void*)"error", strlen("error")));

		case BOOLEAN_VALUE:
			sprintf( tempBuf, "%s", booleanValue ? "true" : "false" );
			return( dest.SendToSink( (void*)tempBuf, booleanValue?4:5 ) );

      	case INTEGER_VALUE:
			sprintf (tempBuf, "%d", integerValue);
			return (dest.SendToSink ((void*)tempBuf, strlen(tempBuf)));

      	case REAL_VALUE:
        	sprintf (tempBuf, "%f", realValue);
			return (dest.SendToSink ((void*)tempBuf, strlen(tempBuf)));

      	case STRING_VALUE:
			return( WriteString( strValue.getCharString( ), dest ) );

		case LIST_VALUE:
			// sanity check
			if (!listValue) {
				Clear();
				return false;
			}
			listValue->ToSink( dest );
			return true;

		case CLASSAD_VALUE:
			// sanity check
			if (!classadValue) {
				Clear();
				return false;
			}
			return classadValue->ToSink( dest );

		case ABSOLUTE_TIME_VALUE:
			{
				struct	tm tms;
				char	ascTimeBuf[32], timeZoneBuf[32], timeOffSetBuf[64];
				extern	time_t timezone;

					// format:  `Wed Nov 11 13:11:47 1998 (CST) -6:00`

					// we use localtime()/asctime() instead of ctime() because 
					// we need the timezone name from strftime() which needs
					// a 'normalized' struct tm.  localtime() does this for us.

					// get the asctime()-like segment; but remove \n
				clock = (time_t) timeValueSecs;
				localtime_r( &clock, &tms );
				asctime_r( &tms, ascTimeBuf, 31 );
				ascTimeBuf[24] = '\0';

					// get the timezone name
				if( !strftime( timeZoneBuf, 31, "%Z", &tms ) ) return false;
				
					// output the offSet (use the relative time format)
					// wierd:  POSIX says regions west of GMT have positive
					// offSets.  We use negative offSets to not confuse users.
				WriteRelativeTime( timeOffSetBuf, true, -timezone, 0 );

					// assemble fields together
				sprintf( tempBuf, "%s (%s) %s", ascTimeBuf, timeZoneBuf, 
							timeOffSetBuf );

				return( ( wantQuotes ? dest.SendToSink((void*)"\'",1):true ) &&
					dest.SendToSink( tempBuf, strlen( tempBuf ) ) &&
					( wantQuotes ? dest.SendToSink( (void*) "\'", 1 ):true ) );
			}

		case RELATIVE_TIME_VALUE:
			WriteRelativeTime( tempBuf, false, timeValueSecs, timeValueUSecs );
			return( ( wantQuotes ? dest.SendToSink( (void*) "\'", 1 ):true ) &&
				dest.SendToSink( tempBuf, strlen( tempBuf ) ) &&
				( wantQuotes ? dest.SendToSink( (void*) "\'", 1 ):true ) );
				
      	default:
        	return false;
    }

	// should not reach here
	return false;
}


void Value::
SetRealValue (double r)
{
    Clear();
    valueType=REAL_VALUE;
    realValue = r;
}

void Value::
SetBooleanValue( bool b )
{
	Clear();
	valueType = BOOLEAN_VALUE;
	booleanValue = b;
}

void Value::
SetIntegerValue (int i)
{
    Clear();
    valueType=INTEGER_VALUE;
    integerValue = i;
}

void Value::
SetUndefinedValue (void)
{
    Clear();
    valueType=UNDEFINED_VALUE;
}

void Value::
SetErrorValue (void)
{
    Clear();
    valueType=ERROR_VALUE;
}

void Value::
SetStringValue( const char *s )
{
    Clear();
	valueType = STRING_VALUE;
	(void) stringSpace.getCanonical( s, strValue, SS_ADOPT_CPLUS_STRING );
}

void Value::
SetListValue (ExprList *l)
{
    Clear();
    valueType = LIST_VALUE;
    listValue = l;
}

void Value::
SetClassAdValue( ClassAd *ad )
{
	Clear();
	valueType = CLASSAD_VALUE;
	classadValue = ad;
}

void Value::
SetRelativeTimeValue( int rsecs, int rusecs ) 
{
	Clear( );
	valueType = RELATIVE_TIME_VALUE;
	if( ( rsecs >= 0 && rusecs < 0 ) || ( rsecs < 0 && rusecs > 0 ) ) {
		rusecs = -rusecs;
	}
	timeValueSecs = rsecs;
	timeValueUSecs = rusecs;
}


void Value::
SetAbsoluteTimeValue( int asecs ) 
{
	Clear( );
	valueType = ABSOLUTE_TIME_VALUE;
	timeValueSecs = asecs;
	timeValueUSecs = 0;
}	


void Value::
WriteRelativeTime( char *buf, bool sign, int timeSecs, int timeUSecs )
{
	char tempBuf[64];
	int days, hrs, mins, secs, usecs;

		// write the sign if necessary
	if( sign || timeSecs < 0 ) {
		strcpy( buf, ( timeSecs < 0 ) ? "-" : "+" );
	} else {
		buf[0] = '\0';
	}

    if( timeSecs < 0 ) {
        days = -timeSecs;
        usecs = -timeUSecs;
    } else {
        days = timeSecs;
        usecs = timeUSecs;
    }

    hrs  = days % 86400;
    mins = hrs  % 3600;
    secs = mins % 60;
    days = days / 86400;
    hrs  = hrs  / 3600;
    mins = mins / 60;

    if( days ) {
        sprintf( tempBuf, "%d#", days );
		strcat( buf, tempBuf );
    }

    sprintf( tempBuf, "%02d:%02d", hrs, mins );
	strcat( buf, tempBuf );
    
    if( secs ) {
        sprintf( tempBuf, ":%02d", secs );
		strcat( buf, tempBuf );
    }

    if( usecs ) {
        sprintf( tempBuf, ".%d", usecs );
		strcat( buf, tempBuf );
    }

}

bool Value::
IsStringValue( char *b, int l, int &al )
{
    if( valueType == STRING_VALUE ) {
        const char *s = strValue.getCharString();
        if( s ) {
            strncpy( b, s, l );
            al = strlen( s );
            return( true );
        } else {
        	b[0] = '\0';
            al = 0;
            return false;
        }
    }
    return false;
}

bool Value::
WriteString( const char* s, Sink &sink )
{
	const char *ptr = s;
	char	buf[32];
	int		len;

	if( !sink.SendToSink( (void*)"\"", 1 ) ) return false;
	while( *ptr ) {
		switch( *ptr ) {
			case '\a': strcpy( buf, "\\a" ); len = 2; break;
			case '\b': strcpy( buf, "\\b" ); len = 2; break;
			case '\f': strcpy( buf, "\\f" ); len = 2; break;
			case '\n': strcpy( buf, "\\n" ); len = 2; break;
			case '\r': strcpy( buf, "\\r" ); len = 2; break;
			case '\t': strcpy( buf, "\\t" ); len = 2; break;
			case '\v': strcpy( buf, "\\v" ); len = 2; break;
			case '\\': strcpy( buf, "\\\\" );len = 2; break;
			case '\?': strcpy( buf, "\\?" ); len = 2; break;
			case '\'': strcpy( buf, "\\\'" );len = 2; break;
			case '\"': strcpy( buf, "\\\"" );len = 2; break;

			default:
				if( !isprint( *ptr ) ) {
						// print hexadecimal representation
					sprintf( buf, "\\%x", (int)*ptr );
					len = strlen( buf );
				} else {
					buf[0] = *ptr;
					len = 1;
				}
		}
		if( !sink.SendToSink( (void*)buf, len ) ) return false;
		ptr++;
	}
	return( sink.SendToSink( (void*)"\"", 1 ) );
}

} // namespace classad
