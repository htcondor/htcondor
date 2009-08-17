%{
#define YYSTYPE int
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
  int yylex (void);
  void yyerror (char const *);
  float *addf(float, float);
  float *subf(float, float);
  float *mulf(float, float);
  float *divf(float, float);

  void flush_buffers();
  void initialize_buffers();

  FILE * output_file, *flog;
  char * colonseparate(char *, int);
  char colonbuf[1024];
  char empty_string[] = "\0";
%}

%token NUM
%token SSTRING
%token VSTRING
%token FNUM
%token BOOL
%token COLONSEP
%token PIPE
%token WHITESPACE
%token WPIPE
%left '-' '+' 
%left '*' '/'

%%
input:
       | input exp
;

exp:  '\n' {fprintf(output_file, "\n");}
| strexp {fprintf(output_file, "%s", $1); fflush(output_file); flush_buffers();}
| '\'' strexp '\'' {fprintf(output_file, "\'%s\'", $2);}
| BOOL {fprintf(output_file, "%s", $1 ? "true" : "false");}
| '\'' BOOL '\'' {fprintf(output_file, "\'%s\'", $2 ? "true" : "false");}
| mexp {fprintf(output_file, "%d", $1);}
| '\'' mexp '\'' {fprintf(output_file, "\'%d\'", $2);}
| fexp {float * ptr = (float *)$1; fprintf(output_file, "%f", *ptr); free(ptr);}
| '\'' fexp '\'' {float * ptr = (float *)$2; fprintf(output_file, "\'%f\'", *ptr); free(ptr);}
;

strexp:
sstrexp {$$ = $1;}
| PIPE '(' SSTRING ')' {$$ = (int)read_pipe_line((char*)$3);}
| strexp WPIPE '(' SSTRING ')' {write_pipe_line((char*)$4, (char*)$1); $$ = (int)empty_string;}
;

sstrexp:
SSTRING {$$ = $1;}
| VSTRING {$$ = $1;}
| VSTRING COLONSEP NUM {$$ = (int)colonseparate((char *)$1, $3);}
;

mexp:    sexp {$$ = $1;}
| mexp '+' mexp {$$ = $1 + $3;}
| mexp '-' mexp {$$ = $1 - $3;}
;
sexp:  ssexp {$$ = $1;}    
| sexp '*' sexp {$$ = $1 * $3;}
| sexp '/' sexp {$$ = $1 / $3;}
;

ssexp: NUM {$$ = $1;}
| '(' mexp ')' {$$ = $2;}
;

fexp:   sfexp {$$ = $1;}
| fexp '+' fexp {float * ptr1 = (float*)$1; float * ptr2 = (float*)$3; $$ = (int)addf(*ptr1, *ptr2); free(ptr1); free(ptr2);}
| fexp '-' fexp {float * ptr1 = (float*)$1; float * ptr2 = (float*)$3; $$ = (int)subf(*ptr1, *ptr2); free(ptr1); free(ptr2);}
;

sfexp:  ssfexp {$$ = $1;}
| sfexp '*' sfexp {float * ptr1 = (float*)$1; float * ptr2 = (float*)$3; $$ = (int)mulf(*ptr1, *ptr2); free(ptr1); free(ptr2);}
| sfexp '/' sfexp {float * ptr1 = (float*)$1; float * ptr2 = (float*)$3; $$ = (int)divf(*ptr1, *ptr2); free(ptr1); free(ptr2);}
;

ssfexp:  FNUM {$$ = $1;}
       | '(' fexp ')' {$$ = $2;}
       
;
;
%%

struct input_file_list
{
  char filename[PATH_MAX];
  char line_buffer[LINE_MAX];
  FILE * pipe_input;
  FILE * pipe_output;
  int pid;
  struct input_file_list * next;
};

struct input_file_list * open_pipes;

void cleanup_subprocs()
{
  struct input_file_list * n = open_pipes;
  while(n != NULL)
    {
      fclose(n->pipe_input);
      fclose(n->pipe_output);
      n = n->next;
    }
}

struct input_file_list * open_input_pipe(char * pipe_name)
{
  int pipes_r[2];
  int pipes_w[2];
  int pid;
  struct input_file_list * n = (struct input_file_list *)malloc(sizeof(struct input_file_list));
  strncpy(n->filename, pipe_name, PATH_MAX);

  if(strcmp(n->filename, pipe_name) != 0)
    {
      fprintf(stderr, "Fail\n");
      exit(-1);
    }

  if(pipe(pipes_r) < 0)
    {
      fprintf(stderr, "Error opening pipe: %s\n", strerror(errno));
      exit(-1);
    }
  if(pipe(pipes_w) < 0)
    {
      fprintf(stderr, "Error opening pipe: %s\n", strerror(errno));
      exit(-1);
    }
  pid = fork();
  if(pid < 0)
    {
      fprintf(stderr, "Unable to fork: %s\n", strerror(errno));
      exit(-1);
    }
  if(pid == 0)
    {
      close(pipes_r[0]);
      close(pipes_w[1]);
      if(dup2(pipes_r[1], STDOUT_FILENO) < 0)
	{
	  fprintf(stderr, "Error in opening stdout pipe: %s\n", strerror(errno));
	  exit(-1);
	}
      if(dup2(pipes_w[0], STDIN_FILENO) < 0)
	{
	  fprintf(stderr, "Error in opening stdin pipe: %s\n", strerror(errno));
	  exit(-1);
	}

      execl(pipe_name, pipe_name, NULL);
      fprintf(stderr, "Error: %s (while trying to execute %s)\n", strerror(errno), pipe_name);
      exit(-1);
    }
  close(pipes_r[1]);
  close(pipes_w[0]);
  if((n->pipe_input = fdopen(pipes_r[0], "r")) == NULL)
    {
      fprintf(stderr, "Error while opening stdout pipe: %s\n", strerror(errno));
      exit(-1);
    }
  if((n->pipe_output = fdopen(pipes_w[1], "w")) == NULL)
    {
      fprintf(stderr, "Error while opening stdin pipe: %s\n", strerror(errno));
      exit(-1);
    }
  n->pid = pid;
  n->next = open_pipes;
  open_pipes = n;
  return n;
}

char * read_pipe_line(char * pipe_name)
{
  struct input_file_list * n = open_pipes;
  if(pipe_name[strlen(pipe_name)-1] == '\n') pipe_name[strlen(pipe_name)-1] = '\0';
  fprintf(stderr, "Reading a line from %s\n", pipe_name);
  while(n != NULL)
    {
      if(strcmp(pipe_name, n->filename) == 0)
	{
	  if(fgets(n->line_buffer, LINE_MAX, n->pipe_input) != NULL)
	    {
	      if(n->line_buffer[strlen(n->line_buffer)-1] == '\n') n->line_buffer[strlen(n->line_buffer)-1] = '\0';
	      fprintf(stderr, "Read: %s\n", n->line_buffer);
	      return n->line_buffer;
	    }
	  else
	    {
	      fprintf(stderr, "Reached end of file.\n");
	      return "\n";
	    }
	}
      n = n->next;
    }
  n = open_input_pipe(pipe_name);
  if(fgets(n->line_buffer, LINE_MAX, n->pipe_input) != NULL)
    {
      if(n->line_buffer[strlen(n->line_buffer)-1] == '\n') n->line_buffer[strlen(n->line_buffer)-1] = '\0';
      return n->line_buffer;
    }
  else
    {
      fprintf(stderr, "Reached end of file.\n");
      return "";
    }
}

int write_pipe_line(char * pipe_name, char * line)
{
  int ret;
  struct input_file_list * n = open_pipes;
  if(pipe_name[strlen(pipe_name)-1] == '\n') pipe_name[strlen(pipe_name)-1] = '\0';
  fprintf(stderr, "Writing %s to %s\n", line, pipe_name);
  while(n != NULL)
    {
      if(strcmp(pipe_name, n->filename) == 0)
	{
	  ret = fprintf(n->pipe_output, "%s\n", line);
	  fflush(n->pipe_output);
	  return ret;
	}
      n = n->next;
    }
  n = open_input_pipe(pipe_name);
  ret = fprintf(n->pipe_output, "%s\n", line);
  fflush(n->pipe_output);
  return ret;
}
 

float *addf(float a, float b) {float * ret =malloc(sizeof(float)); *ret = a+b; return ret;}
float *subf(float a, float b) {float * ret =malloc(sizeof(float)); *ret = a-b; return ret;}
float *mulf(float a, float b) {float * ret =malloc(sizeof(float)); *ret = a*b; return ret;}
float *divf(float a, float b) {float * ret =malloc(sizeof(float)); *ret = a/b; return ret;}
 
struct name_value_pair
{
  char name[2048];
  char value[2048];
  char type;
  struct name_value_pair * next;
};

struct name_value_pair * vars_list;

extern char * yytext;

void yyerror(const char *c)
{
  fprintf(stderr, "%s\n", c);
  fprintf(stderr, "On: %s\n", yytext);
  exit(-1);
}

char * colonseparate(char * str, int col)
{
  int colons = 0;
  int i = 0, j = 0;
  colonbuf[0] ='\0';
  for(i = 0; i < strlen(str); i++)
    {
      if(str[i] == ':') colons++;
      if(colons == col) break;
    }
  if(i == strlen(str)) return colonbuf;
  if(col != 0) i++;
  j = i;
  for(; i < strlen(str); i++)
    {
      if(str[i] == ':') break;
      if(str[i] != '"') colonbuf[i - j] = str[i];
      else j++;
    }
  colonbuf[i - j] = '\0';
  return colonbuf;
}

char * find_value(struct name_value_pair * list, char * name)
{
  while(list != 0)
    {
      if(strcasecmp(name, list->name) == 0) return list->value;
      list = list->next;
    }
  fprintf(stderr,"Unknown variable: %s\n", name);
  fprintf(stderr, "Variables are:\n");
  list = vars_list;
  while(list != 0)
    {
      fprintf(stderr, "\t%s\n", list->name);
      list = list->next;
    }
  exit(-1);
  return 0;
}

int lookupVarType(struct name_value_pair * list, char * name)
{
 while(list != 0)
    {
      if(strcasecmp(name, list->name) == 0)
	{
	  if(list->type == 'S') return VSTRING;
	  else if(list->type == 'I') return NUM;
	  else if(list->type == 'F') return FNUM;
	  else return BOOL;
	}
      list = list->next;
    }
 fprintf(stderr, "Unknown variable: %s\n", name);
 fprintf(stderr, "Variables are:\n");
 list = vars_list;
 while(list != 0)
   {
     fprintf(stderr, "\t%s\n", list->name);
     list = list->next;
   }

 exit(-1);
 return 0;
}

int open_xml_file(void)
{
  FILE * config = fopen("/etc/condor/condor_config", "r");
  char buf[1024], buf2[12];
  while(fgets(buf, 1024, config) != NULL)
    {
      if(buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';
      strncpy(buf2, buf, 11);
      if(strcmp(buf2, "LIBVIRT_XML") == 0)
	{
	  int i;
	  int fd;
	  for(i = 0; i < strlen(buf); i++)
	    {
	      if(buf[i] == '=')
		{
		  i++;
		  break;
		}
	    }
	  while(i < strlen(buf))
	    {
	      if((buf[i] != ' ') && (buf[i] != '\t')) break;
	      i++;
	    }
	  
	  fd = open(buf+i, O_RDONLY);
	  if(fd < 0)
	    {
	      fprintf(stderr, "Error while opening XML file: %s\n", strerror(errno));
	      exit(-1);
	    }
	  fprintf(stderr, "%s\n", buf+i);
	  dup2(fd, STDIN_FILENO);
	  return 0;
	}
    }
  return -1;
}

char determine_type_from_value(char * value)
{
  char * endp;

  strtod(value, &endp);
  if(value != endp) return 'F';

  strtol(value, &endp, 10);
  if(value != endp) return 'I';

  if((strcasecmp(value, "true") == 0) || (strcasecmp(value, "false") == 0)) return 'B';

  return 'S';
}

int main(int argc, char ** argv)
{
  //  floatlist = 0;
  // First, build the table of variables
  char line[8192], * xml, * execute_dir;
  FILE * generic_xml_file;
  struct name_value_pair * list = 0, * cur = 0;
  int i, j;
  char * tmpfile = "/tmp/libvirtXXXXXX";
  output_file = stdout;
  if(output_file == 0)
    {
      fprintf(stderr, "Unable to open file %s\n", argv[1]);
      return -1;
    }

  fgets(line, 8192, stdin);
  while(line[0] != '\n')
    {
      struct name_value_pair * nvp = malloc(sizeof(struct name_value_pair));
      if(list == 0) 
	{
	  list = nvp;
	  cur = nvp;
	}
      else
	{
	  cur->next = nvp;
	  cur = nvp;
	}
      //    nvp->type = line[0];
      for(i = 0; i < strlen(line); i++)
	{
	  if((line[i] == ' ') || (line[i] == '=')) break;
	  nvp->name[i] = line[i];
	}
      nvp->name[i] = '\0';


      for(; i<strlen(line); i++)
	{
	  if((line[i] != ' ') && (line[i] != '=')) break;
	}
      j = i;

      for(; i < strlen(line)-1; i++)
	{
	  nvp->value[i - j] = line[i];
	}
      nvp->value[i-j] = '\0';
      fprintf(stderr,"%s %s\n", nvp->name, nvp->value);
      nvp->type = determine_type_from_value(nvp->value);

      nvp->next = 0;
      if(fgets(line, 8192, stdin) == NULL) break;
    }
  vars_list = list;
  // Find the xml file.
  if(open_xml_file() < 0)
    {
      fprintf(stderr, "Could not open xml file, trying for classad\n");
    }
  else
    {
      int xml_fd;
      FILE * xml_file;

      char * xml = find_value(list, "LIBVIRT_XML");
      xml_fd = mkstemp(tmpfile);
      xml_file = fdopen(xml_fd, "w");
      fprints(xml_file, "%s", xml);
      fclose(xml_file);
      xml_fd = open(tmpfile, O_RDONLY);
      dup2(xml_fd, STDIN_FILENO);
    }
  initialize_buffers();
  int ret = yyparse();
  cleanup_subprocs();
  if(ret != 0)
    {
      fprintf(stderr, "Unknown error.\n");
    }
  // If the tempfile was created for XML, delete it
  if(tmpfile[strlen(tmpfile)-1] != 'X')
    {
      unlink(tmpfile);
    }
  return ret;
}
