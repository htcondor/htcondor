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
	int		is_mapped;
	struct node	*next;
	struct node *prev;
};
	
union yystype {
	struct token tok;
	struct node *node;
};
#define YYSTYPE union yystype
