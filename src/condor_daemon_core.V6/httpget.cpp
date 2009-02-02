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

#include "condor_common.h"
#include "condor_debug.h"

#include "httpget.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define HEXTOINT(x) ((x >= '0' && x <= '9') ? (x - '0') : ((x >= 'a' && x <= 'f') ? (x - 'a' + 10) : (x - 'A' + 10)))

#define STATE_NORMAL 0
#define STATE_DECODING_FIRST (STATE_NORMAL + 2)
#define STATE_DECODING_SECOND (STATE_NORMAL + 1)

typedef int (*get_handler_t)(struct soap*);

char *websrv_id = WEBSRV_ID;

static char *urldecode(const char*);

/* static int websrv_copy(struct soap*, struct soap_plugin*, struct soap_plugin*); */
static void websrv_delete(struct soap*, struct soap_plugin*);

static int websrv_parse(struct soap*);

int websrv(struct soap* soap, struct soap_plugin* plug, void *get_handler) {
  plug->id = websrv_id;
  plug->data = calloc(sizeof(struct websrv_data), 1);

  struct websrv_data *pdata = (struct websrv_data*)plug->data;

  if (!(plug->data)) {
    dprintf(D_ALWAYS, "FAILED to allocate plugin data!");
    return SOAP_EOM;
  }

  plug->fdelete = websrv_delete;
  pdata->fparse = soap->fparse;
  pdata->fget = (get_handler_t)get_handler;
  
  soap->fparse = websrv_parse;

  return SOAP_OK;
}

int websrv_parse(struct soap *soap) {
  struct websrv_data *d = (struct websrv_data*)(soap_lookup_plugin(soap,websrv_id));
  if (!d) {
    dprintf(D_ALWAYS, "FAILED to find plugin data for [[%s]]", websrv_id);
    return SOAP_PLUGIN_ERROR;
  }
  
  soap->error = d->fparse(soap);

  if (!(d->fget)) {
    /* no GET handler, bailing */
    return soap->error;
  }

  if (soap->error == SOAP_GET_METHOD) {
    soap->error = d->fget(soap);
    if (soap->error == SOAP_OK) {
      return SOAP_STOP;
    }
  }
  
  return soap->error;
}

void websrv_delete(struct soap*, struct soap_plugin* p) {
  free(p->data);
}

/* we're not using this now, but we might need it */
char *urldecode(const char *input) {
  char *result = (char*)calloc(strlen(input), sizeof(char));
  int inpos = 0, outpos = 0;
  int state = STATE_NORMAL;
  char decoded_char = '\0';

  if (!result) goto fail;

  while (input[inpos]) {
    switch(state) {
    case STATE_NORMAL:
      switch(input[inpos]) {
      case '%':
	decoded_char = '\0';
	state = STATE_DECODING_FIRST;
	break;
      case '+':
	result[outpos++] = ' ';
	break;
      default:
	result[outpos++] = input[inpos];
	break;
      }
      break;

    default:
      state--;
      if(!isxdigit(input[inpos])) { goto fail; }

      decoded_char = decoded_char << 4;
      decoded_char += HEXTOINT(toupper(input[inpos]));
      if (state == STATE_NORMAL) {
	result[outpos++] = decoded_char;
      }
    }
    inpos++;
  }
  if(state != STATE_NORMAL) { goto fail; }
  
  return result;

 fail:
  /* decoding failed, either (1) we couldn't allocate memory, 
     (2) we had a %-escaped char that didn't have two digits, or 
     (3) we had a %-escaped char that didn't use hex digits. */
  if (result) {
    free(result);
    result = NULL;
  }
  return NULL;
}
