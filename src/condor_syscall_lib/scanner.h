#define FUNC		1
#define PARAM		2
#define PTR_PARAM	3

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
	int		in_param;
	int		out_param;
	int		extract;
	struct node	*next;
	struct node *prev;
};

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
