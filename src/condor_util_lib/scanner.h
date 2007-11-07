/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 

#define DUMMY			0
#define FUNC			1
#define PARAM			2
#define XFER_FUNC		3
#define ALLOC_FUNC		4
#define ACTION_PARAM	5
#define NOSUPP_FUNC		6
#define RETURN_FUNC		7
#define NULL_ALLOC_FUNC 8

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
