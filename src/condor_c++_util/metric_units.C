
#include "condor_common.h"
#include "metric_units.h"

#define METRIC_UNITS_BUFFER_SIZE 80
#define METRIC_POWER_COUNT 5

extern "C" char *metric_units( double bytes )
{
	static char buffer[METRIC_UNITS_BUFFER_SIZE];
	static char *suffix[METRIC_POWER_COUNT] =
		{ "B ", "KB", "MB", "GB", "TB" };

	double value=0;
	int power=0;

	value = bytes;

	while( (value>1024.0) && (power<METRIC_POWER_COUNT) ) {
		value = value / 1024.0;
		power++;
	}

	sprintf( buffer, "%.1f %s", value, suffix[power] );

	return buffer;
}




