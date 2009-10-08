%{
#include "y.tab.h"
#include <string.h>
extern char * yylval;
extern FILE * output_file;
float fval;

struct name_value_pair
{
  char name[2048];
  char value[2048];
  struct name_value_pair * next;
};

char * lookupVarValue(char * var);
char * find_value(struct name_value_pair * list, char * name);
char* copy_to_buffer(char *, int);

extern struct name_value_pair * vars_list;


%}

%%

[0-9]+	yylval = (char*)atoi(yytext); return NUM;
[0-9]+\.[0-9]+ {float *ptr = (float*)malloc(sizeof(float)); *ptr  = atof(yytext); yylval = (char*)ptr; return FNUM;}
[-+\*\(\)/\n']	yylval = yytext; return yytext[0];
\|\< yylval = yytext; return PIPE;
\|\> yylval = yytext; return WPIPE;
\<*[A-Za-z/'=\.[:space:]]+\>*|\>|\< yylval = copy_to_buffer(yytext, yyleng); return SSTRING;
\$[A-Za-z0-9\_]+ yylval = lookupVarValue(yytext+sizeof(char)); return lookupVarType(vars_list, yytext+sizeof(char));
\-\> yylval = ":"; return COLONSEP;


%%
char * lookupVarValue(char * var)
{
	int var_type;
	char * var_str = find_value(vars_list, var);
	var_type = lookupVarType(vars_list, var);
	if(var_type == VSTRING) return var_str;
	else if(var_type == NUM) return (char*)atoi(var_str);
	else if(var_type == FNUM) 
	{
		float *val = malloc(sizeof(float));	
		*val = atof(var_str);
		return (char*)val;
	}
	else 
	{
             int ret = (strcasecmp(var_str, "true")) ? -1 : 0;    
	     return (char*)ret;
	}
}

struct buffer_list
{
char * data;
struct buffer_list * next;
};

struct buffer_list * buffers;

char * copy_to_buffer(char * txt, int len)
{
	struct buffer_list * buf = (struct buffer_list*)malloc(sizeof(struct buffer_list));
	buf->data = (char*)malloc(sizeof(char)*(len+1));
	strncpy(buf->data, txt, len);
	buf->next = buffers;
	return buf->data;
}

void flush_buffers()
{
	struct buffer_list * list, *tmp;
	list = buffers;
	while(list != NULL)
	{
		free(list->data);
		tmp = list;
		list = list->next;
		free(tmp);
	}
	buffers = NULL;
}

void initialize_buffers()
{
	buffers = NULL;
}

int lookupVarType(struct name_value_pair *, char * var);
/*{
	//	OK, what we need to do is pass the exact type using the variable name, perhaps prefixes?
	if(var[0] == 'S') return VSTRING;
	else if(var[0] == 'I') return NUM;
	else if(var[0] == 'F') return FNUM;
	else return BOOL;
}*/