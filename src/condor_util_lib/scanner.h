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

 

#define DUMMY			0
#define FUNC			1
#define PARAM			2
#define XFER_FUNC		3
#define ALLOC_FUNC		4
#define ACTION_PARAM	5
#define NOSUPP_FUNC		6
#define RETURN_FUNC		7

#define NAME_LENGTH		80

struct token {
	int		tok_type;
	char	*val;
};

struct node {
	int		node_type;
	char	*type_name;
	char	*id;
	int		is_ptr;
	int		is_const;
	int		is_const_ptr;
	int		is_tabled;
	int		is_array;
	int		is_ref;
	int		is_vararg;
	int		is_mapped;
	int		is_map_name;
	int		is_map_name_two;
	int		is_indirect;
	int		in_param;
	int		out_param;
	int		extract;
	int		dl_extract;
	int		sys_chk;
	int		pseudo;
	int		ldiscard;
	int		rdiscard;
	struct node	*next;
	struct node *prev;
	struct node *param_list;
	struct node *action_func_list;
	char		table_name[NAME_LENGTH];
	char		remote_name[NAME_LENGTH];
	char		local_name[NAME_LENGTH];
	char		sender_name[NAME_LENGTH];
	char		also_implements[10 * NAME_LENGTH];
};

#define XDR_FUNC id
#define SOCK_FUNC id

struct p_mode {
	int		in;
	int		out;
};


union yystype {
	struct	token tok;
	struct	node *node;
	struct	p_mode param_mode;
	int		bool;
};
#define YYSTYPE union yystype
