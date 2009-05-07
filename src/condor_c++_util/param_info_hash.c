
#include <stdlib.h>
#include <string.h>
#include "param_info.h"

static unsigned int
param_info_hash(const char *str)
{
	unsigned int hash = 5381;
	unsigned char c;

	while ((c = *str++) != 0) {
		hash = (hash * 33) + c;
	}

	return hash % PARAM_INFO_TABLE_SIZE;
}

void
param_info_hash_insert(param_info_hash_t param_info, param_info_t* p) {

	unsigned int key;
	bucket_t* b;

	key = param_info_hash(p->name);

	if (!param_info[key]) {

		param_info[key] = malloc(sizeof(bucket_t));
		param_info[key]->param = p;
		param_info[key]->next = NULL;

	} else {

		b = param_info[key];
		while(b->next) {
			b = b->next;
		}
		b->next = malloc(sizeof(bucket_t));
		b = b->next;
		b->param = p;
		b->next = NULL;

	}
}

param_info_t*
param_info_hash_lookup(param_info_hash_t param_info, const char* param) {

	if(param_info == NULL) {
		return NULL;
	}
	
	unsigned int key;
	bucket_t* b;

	key = param_info_hash(param);

	b = param_info[key];
	while(b != NULL) {
		if (strcmp(b->param->name,param) == 0) {
			return b->param;
		}
		b = b->next;
	}

	return NULL;
}

// of type to be used by param_info_hash_iterate
int
param_info_hash_dump_value(param_info_t* param_value, void* unused) {
	printf("%s:  default=", param_value->name);
	switch (param_value->type) {
		case TYPE_STRING:
			printf("%s", param_value->default_val.str_val);
			break;
		case TYPE_DOUBLE:
			printf("%f", param_value->default_val.dbl_val);
			break;
		case TYPE_INT:
			printf("%d", param_value->default_val.int_val);
			break;
		case TYPE_BOOL:
			printf("%s", param_value->default_val.int_val == 0 ? "false" : "true");
			break;
	}
	printf("\n");
	return 0;
}

void
param_info_hash_dump(param_info_hash_t* param_info) {
	param_info_hash_iterate(param_info, &param_info_hash_dump_value, NULL);
}

void
param_info_hash_iterate(param_info_hash_t* param_info, int (*callPerElement)
			(param_info_t* /*value*/, void* /*user data*/), void* user_data) {
	int i;
	int stop = 0;
	for(i = 0; i < PARAM_INFO_TABLE_SIZE && stop == 0; i++) {
		bucket_t* this_param = param_info + i * sizeof(bucket_t*);
		while(this_param != NULL && stop == 0) {
			stop = callPerElement(this_param->param, user_data);
			this_param = this_param->next;
		}
	}
}

void 
param_info_hash_create(param_info_hash_t* param_info) {
	*param_info = malloc(sizeof(bucket_t*)*PARAM_INFO_TABLE_SIZE);
	memset(*param_info,0,sizeof(bucket_t*)*PARAM_INFO_TABLE_SIZE);
}

