#define DUMMY			0
#define FUNC			1
#define PARAM			2
#define XFER_FUNC		3
#define ALLOC_FUNC		4
#define ACTION_PARAM	5
#define NOSUPP_FUNC		6


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
	int		pseudo;
	struct node	*next;
	struct node *prev;
	struct node *param_list;
	struct node *action_func_list;
};

#define XDR_FUNC id

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
