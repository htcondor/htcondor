#ifndef PROC_OBJ_H
#define PROC_OBJ_H
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include "list.h"
#include "filter.h"

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
	virtual int		get_status() = 0;
	virtual int		get_prio() = 0;
	virtual char	*get_owner() = 0;
	virtual char	*get_arch() = 0;
	virtual char	*get_opsys() = 0;
	virtual int		get_cluster_id() = 0;
	virtual int		get_proc_id() = 0;
	virtual float	get_local_cpu() = 0;
	virtual float	get_remote_cpu() = 0;
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
	int		get_status();
	int		get_prio();
	char	*get_owner();
	char	*get_arch();
	char	*get_opsys();
	int		get_cluster_id();
	int		get_proc_id();
	float	get_local_cpu();
	float	get_remote_cpu();

		// Modification functions
	void	set_prio( int prio );
	void	store( DBM *Q );
private:
	V2_PROC	*p;
};

class V3_ProcObj : public ProcObj {
public:
	V3_ProcObj( const V3_PROC *proc );
	~V3_ProcObj();

		// "Read-Only" access functions
	void	display();
	void	display_short();
	void	display_special();
	int		get_status();
	int		get_prio();
	char	*get_owner();
	char	*get_arch();
	char	*get_opsys();
	int		get_cluster_id();
	int		get_proc_id();
	float	get_local_cpu();
	float	get_remote_cpu();

		// Modification functions
	void	set_prio( int prio );
	void	store( DBM *Q );
private:
	V3_PROC	*p;
};

#endif /* PROC_OBJ_H */
