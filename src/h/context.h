/*
 * context.h
 * defines a linked list of trees.
 */

class Context_node
{
 public:
  Context_node(char *name, class Expression *expr)
  {
    tree_name = new char[strlen(name) + 1];
    strcpy(tree_name, name);
    tree = expr;
    next = NULL;
  }

  ~Context_node()
  {
    delete tree_name;
    delete tree;
    delete next;
  }

  friend class Context;
  
 private:
  char *tree_name;
  class Expression *tree;
  class Context_node *next;
};

class Context
{
 public:
  Context() { list = NULL; }
  ~Context() { delete list; }
  Expression *Look_up(char *name);
  void Insert(char *name, class Expression *tree);

 private:
  Context_node *list;
};
