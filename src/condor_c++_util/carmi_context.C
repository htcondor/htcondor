#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_fix_string.h"
#include "condor_xdr.h"
#include "carmi_context.h"
#include "util_lib_proto.h"
#include "condor_network.h"		/* For COLLECTOR_PORT */
#include "condor_debug.h"
#include "sched.h"

#undef ASSERT
#define ASSERT(x) x

extern "C" {
	int xdr_mach_rec(XDR *, MACH_REC *);
	int my_xdr_destroy(XDR *);
}


CARMI_Context::CARMI_Context(const char *machine_name, int how, int timeout )
{
	char	*CollectorHost;
	int		sock;
	XDR		xdr, *xdrs = NULL;
	int		cmd;
	int		found_it;

	if (how != COLLECTOR) {
		fprintf(stderr, 
				"Sorry, only know how to talk to the collector now!\n");
		exit(1);
	}

	config( "", (CONTEXT *) 0);

	CollectorHost = param("COLLECTOR_HOST");
	if (CollectorHost == 0) {
		fprintf(stderr, "Can't identify collector host!\n");
		exit(1);
	}

	sock = do_connect(CollectorHost, "condor_collector", COLLECTOR_PORT);

	if (sock < 0) {
		perror("Can't connect to the collector!");
		exit(1);
	}

	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

	cmd = GIVE_STATUS;
	ASSERT( xdr_int(xdrs, &cmd) );
	ASSERT( xdrrec_endofrecord(xdrs,TRUE) );

	xdrs->x_op = XDR_DECODE;
	found_it = 0;
	while( !found_it ) {
		rec.machine_context = create_context();
		ASSERT( xdr_mach_rec(xdrs, &rec) );
		if ( !rec.name || !(rec.name[0]) ) {
			break;
		}
		if (!strcmp(rec.name, machine_name)) {
			found_it = 1;
			break;
		}
		free_context( rec.machine_context );
	}
	my_xdr_destroy(xdrs);
	close(sock);
}


CARMI_Context::~CARMI_Context()
{
	free_context( rec.machine_context );
	free( rec.name );
}


void
CARMI_Context::display()
{

	extern int Terse;
	int	o_debug_flags;
	int o_Terse;

	o_debug_flags = DebugFlags;
	o_Terse = Terse;
	DebugFlags |= (D_EXPR | D_NOHEADER);
	Terse = 1;

	fprintf(stderr, "Machine = %s\n", rec.name);
	fprintf(stderr, "CONTEXT\n");
	fprintf(stderr, "-------\n");
	display_context( rec.machine_context );
	fprintf(stderr, "time_stamp: %s\n", ctime( (time_t *) &rec.time_stamp));

	DebugFlags = o_debug_flags;
	Terse = o_Terse;
}


int
CARMI_Context::value( char *name, char *&val)
{
	return evaluate_string(name, &val, rec.machine_context, 
						   rec.machine_context);
}


int
CARMI_Context::value( char *name, int &val)
{
	return evaluate_int(name, &val, rec.machine_context, 
						rec.machine_context);
}


int
CARMI_Context::value( char *name, float &val)
{
	return evaluate_float(name, &val, rec.machine_context, 
						  rec.machine_context);
}


int
CARMI_Context::bool_value( char *name, int &val)
{
	return evaluate_bool(name, &val, rec.machine_context, 
						  rec.machine_context);
}


void
CARMI_Context::start_scan()
{
	last_scan = 0;
}


int
CARMI_Context::next( char *&name, int &type)
{
	int		dummy_int;
	float	dummy_float;
	char	*dummy_str;

	if (last_scan >= rec.machine_context->len) {
		return 0;
	}
	name = rec.machine_context->data[last_scan]->data[0]->val.string_val;

	if (!value(name, dummy_int)) {
		type = INT;
	} else if (!value(name, dummy_str)) {
		type = STRING;
	} else if (!value(name, dummy_float)) {
		type = FLOAT;
	} else if (!bool_value(name, dummy_int)) {
		type = BOOL;
	} else {
		type = -1;
	}

	last_scan++;
	return 1;
}
