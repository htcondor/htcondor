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
#ifndef PROC_OBJ_H
#define PROC_OBJ_H
#include "condor_constants.h"
#include "list.h"

	// A function which takes a pointer to a PROC struct and returns
	// a boolean, e.g. whether to add the PROC to a list.
typedef BOOLEAN	(*P_FILTER) ( PROC * );

char 			*string_copy( const char *str );
GENERIC_PROC	*GenericFetchProc( DBM *, PROC_ID * );


class ProcObj {
public:
		// Constructors and destructors
	virtual	~ProcObj() {;}
	static	ProcObj * create( GENERIC_PROC * );
	static	ProcObj * create( DBM *, int, int );
	static	ProcObj * create( XDR * );

		// Create and delete lists of proc objs
	static	List<ProcObj> * create_list( DBM *, P_FILTER );
	static	List<ProcObj> * create_list( XDR *, P_FILTER );
	static void	delete_list( List<ProcObj> * );

	static void short_header();
	static void special_header();

		// "Read-Only" access functions
	virtual void	display() = 0;
	virtual void	display_short() = 0;
	virtual void	display_special() = 0;
	virtual void    print_special() = 0;
	virtual int		get_status() = 0;
	virtual int		get_prio() = 0;
	virtual char	*get_owner() = 0;
	virtual char	*get_arch() = 0;
	virtual char	*get_opsys() = 0;
	virtual char    *get_date() = 0;
	virtual time_t  get_time() = 0;
	virtual char    *get_cmd() = 0;
	virtual char    *get_requirements() = 0;
	virtual int		get_cluster_id() = 0;
	virtual int		get_proc_id() = 0;
	virtual float	get_local_cpu() = 0;
	virtual float	get_remote_cpu() = 0;
	virtual int     get_procId() = 0;
	virtual float   get_procCpu() = 0;
	virtual char    get_procStatus() = 0;
	virtual float   get_procSize() = 0;
	virtual char**  get_proc_long_info() = 0;
	virtual BOOLEAN perm_to_modify();


		// Modification functions
	virtual void	set_prio( int prio ) = 0;
	virtual void	store( DBM *Q ) = 0;
};

class V2_ProcObj : public ProcObj {
public:
	V2_ProcObj( const V2_PROC *proc );
	~V2_ProcObj();
		// "Read-Only" access functions
	void	display();
	void	display_short();
	void	display_special();
	void    print_special() {};
	int		get_status();
	int		get_prio();
	char	*get_owner();
	char	*get_arch();
	char	*get_opsys();
	char    *get_requirements();
	char    *get_date() {return (char *) NULL;};
	time_t  get_time() {return (time_t) 0;};
	char    *get_cmd() {return (char *) NULL;};
	int		get_cluster_id();
	int		get_proc_id();
	float	get_local_cpu();
	float	get_remote_cpu();

	int     get_procId() {return 0;};
	float   get_procCpu() {return 0.0;};
	char    get_procStatus() {return '\0';};
	float   get_procSize() {return 0.0;};
	char**  get_proc_long_info() {return (char **) NULL;};
	
	// Modification functions
	void	set_prio( int prio );
	void	store( DBM *Q );
private:
	V2_PROC	*p;
};

class V3_ProcObj : public ProcObj {
public:
	V3_ProcObj( const V3_PROC *proc );
	V3_ProcObj() { p = NULL; }
	~V3_ProcObj();

		// "Read-Only" access functions
	void	display();
	void	display_short();
	void	display_special();
	void    print_special();
	int		get_status();
	int		get_prio();
	char	*get_owner();
	char	*get_arch();
	char	*get_opsys();
	char    *get_requirements();
	char    *get_date();
	time_t	get_time();
	char    *get_cmd();
	int		get_cluster_id();
	int		get_proc_id();
	float	get_local_cpu();
	float	get_remote_cpu();

	int     get_procId();
	float   get_procCpu();
	char    get_procStatus();
	float   get_procSize();
	
	char**  get_proc_long_info();

		// Modification functions
	void	set_prio( int prio );
	void	store( DBM *Q );
private:
	V3_PROC	*p;
};

#endif /* PROC_OBJ_H */



