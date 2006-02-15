/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/*	
	Portions of this file may also be  
	Copyright (C) 2000-2003 Robert A. van Engelen, Genivia inc.
	All Rights Reserved.
*/

#include "condor_common.h"
#include "httpget.h"

char * http_get_id = HTTP_GET_ID;

static int http_get_init(struct soap *soap, struct http_get_data *data, int (*handler)(struct soap*));
static int http_get_copy(struct soap *soap, struct soap_plugin *dst, struct soap_plugin *src);
static void http_get_delete(struct soap *soap, struct soap_plugin *p);
static int http_get_parse(struct soap *soap);
static int http_connect(struct soap*, const char*, const char*, int, const char*, const char*, size_t);

int http_get(struct soap *soap, struct soap_plugin *p, void *arg)
{ p->id = http_get_id;
  p->data = (void*)malloc(sizeof(struct http_get_data));
  p->fcopy = http_get_copy;
  p->fdelete = http_get_delete;
  if (p->data)
    if (http_get_init(soap, (struct http_get_data*)p->data, (int (*)(struct soap*))arg))
    { free(p->data); /* error: could not init */
      return SOAP_EOM; /* return error */
    }
  return SOAP_OK;
}

int soap_get_connect(struct soap *soap, const char *endpoint, const char *action)
{ struct http_get_data *data = (struct http_get_data*)soap_lookup_plugin(soap, http_get_id);
  if (!data)
    return soap->error = SOAP_PLUGIN_ERROR;
  soap_begin(soap);
  data->fpost = soap->fpost;
  soap->fpost = http_connect;
  return soap_connect(soap, endpoint, action);
}

static int http_connect(struct soap *soap, const char *endpoint, const char *host, int port, const char *path, const char *action, size_t count)
{ struct http_get_data *data = (struct http_get_data*)soap_lookup_plugin(soap, http_get_id);
  if (!data)
    return SOAP_PLUGIN_ERROR;
  soap->status = SOAP_GET;
  soap->fpost = data->fpost;
  return soap->fpost(soap, endpoint, host, port, path, action, count);
}

static int http_get_init(struct soap *soap, struct http_get_data *data, int (*handler)(struct soap*))
{ data->fparse = soap->fparse; /* save old HTTP header parser callback */
  data->fget = handler;
  data->stat_get = 0;
  data->stat_post = 0;
  data->stat_fail = 0;
  soap->fparse = http_get_parse; /* replace HTTP header parser callback with ours */
  return SOAP_OK;
}

static int http_get_copy(struct soap *soap, struct soap_plugin *dst, struct soap_plugin *src)
{ *dst = *src;
  return SOAP_OK;
}

static void http_get_delete(struct soap *soap, struct soap_plugin *p)
{ free(p->data); /* free allocated plugin data (this function is not called for shared plugin data) */
}

static int http_get_parse(struct soap *soap)
{ struct http_get_data *data = (struct http_get_data*)soap_lookup_plugin(soap, http_get_id);
  if (!data)
    return SOAP_PLUGIN_ERROR;
  soap->error = data->fparse(soap); /* parse HTTP header */
  if (soap->error == SOAP_OK)
    data->stat_post++;
  else if (soap->error == SOAP_GET_METHOD && data->fget)
  { soap->error = SOAP_OK;
    if ((soap->error = data->fget(soap))) /* call user-defined HTTP GET handler */
    { data->stat_fail++;
      return soap->error;
    }
    data->stat_get++;
    return SOAP_STOP; /* stop processing the request and do not return SOAP Fault */
  }
  else
    data->stat_fail++;
  return soap->error;
}

char *query(struct soap *soap)
{ return strchr(soap->path, '?');
}

char *query_key(struct soap *soap, char **s)
{ char *t = *s;
  if (t && *t)
  { *s = (char*)soap_decode_string(t, soap->path - t, t + 1);
    return t;
  }
  return *s = NULL;
}

char *query_val(struct soap *soap, char **s)
{ char *t = *s;
  if (t && *t == '=')
  { *s = (char*)soap_decode_string(t, soap->path - t, t + 1);
    return t;
  }
  return NULL;
}

int soap_encode_string(const char *s, char *t, int len)
{ register int c;
  register int n = len;
  while ((c = *s++) && --n > 0)
  { if (c == ' ') 
      *t++ = '+';
    else if (c == '!'
          || c == '$'
          || (c >= '(' && c <= '.')
          || (c >= '0' && c <= '9')
          || (c >= 'A' && c <= 'Z')
          || c == '_'
          || (c >= 'a' && c <= 'z'))
      *t++ = c;
    else if (n > 2)
    { *t++ = '%';
      *t++ = (c >> 4) + (c > 159 ? '7' : '0');
      c &= 0xF;
      *t++ = c + (c > 9 ? '7' : '0');
      n -= 2;
    }
    else
      break;
  }
  *t = '\0';
  return len - n;
}

const char* soap_decode_string(char *buf, int len, const char *val)
{ const char *s;
  char *t;
  for (s = val; *s; s++)
    if (*s != ' ' && *s != '=')
      break;
  if (*s == '"')
  { t = buf;
    s++;
    while (*s && *s != '"' && --len)
      *t++ = *s++;
    *t = '\0';
    do s++;
    while (*s && *s != '&' && *s != '=');
  }
  else
  { t = buf;
    while (*s && *s != '&' && *s != '=' && --len)
      switch (*s)
      { case '+':
          *t++ = ' ';
        case ' ':
          s++;
          break;
        case '%':
          *t++ = ((s[1] >= 'A' ? (s[1]&0x7) + 9 : s[1] - '0') << 4) + (s[2] >= 'A' ? (s[2]&0x7) + 9 : s[2] - '0');
          s += 3;
          break;
        default:
          *t++ = *s++;
      }
    *t = '\0';
  }
  return s;
}

/******************************************************************************/

