/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
