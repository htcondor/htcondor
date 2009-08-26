#ifndef _HASHWIN32_
#define _HASHWIN32_
typedef struct _ENTRY
{
	char *key;
	void *data;
} ENTRY;

typedef struct _BUCKET
{
	struct _BUCKET *next;
	ENTRY entry;
} BUCKET;

static BUCKET **tree;
static size_t HASHSIZE;
static size_t count;

enum ACTION
{
	FIND,
	ENTER,
};

enum VISIT
{
	preorder,
	postorder,
	endorder,
	leaf
};

unsigned int hash(const char *s);

int hcreate(size_t);
void hdestroy();
struct ENTRY *hsearch(ENTRY, enum ACTION);
#endif