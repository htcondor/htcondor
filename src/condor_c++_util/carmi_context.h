/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "manager.h"

enum {                  // how to get the information
        COLLECTOR,
        STARTD,
        SCHEDD,
        BOTH_DAEMONS
};

enum {          // types of values stored in the context
        CARMI_INT,
        CARMI_FLOAT,
        CARMI_STRING,
        CARMI_BOOL
};

class CARMI_Context {
public:
	CARMI_Context( const char *machine_name, int how = COLLECTOR, 
			int timeout = 0 );
#if 0
	CARMI_Context( unsigned int ip_addr, int how = COLLECTOR, 
				  int timeout = 0 );
	CARMI_Context( int how = COLLECTOR, int timeout = 0 );           // self 
#endif
    ~CARMI_Context();
	void display();

	int value( char *name, char *&val );
	int value( char *name, int &val );
	int value( char *name, float &val );
	int bool_value( char *name, int &val );

	void start_scan();
	int next( char *&name, int &type );

private:

MACH_REC		rec;
int				last_scan;
};
