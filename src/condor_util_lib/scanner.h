/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define DUMMY			0
#define FUNC			1
#define PARAM			2
#define XFER_FUNC		3
#define ALLOC_FUNC		4
#define ACTION_PARAM	5
#define NOSUPP_FUNC		6
#define RETURN_FUNC		7


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
	int		is_mapped;
	int		is_array;
	int		is_ref;
	int		in_param;
	int		out_param;
	int		extract;
	int		dl_extract;
	int		sys_chk;
	int		pseudo;
	struct node	*next;
	struct node *prev;
	struct node *param_list;
	struct node *action_func_list;
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
