/*
 * Copyright 2010 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "condor_common.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "store_cred.h"

char*
getBrokerPassword()
{
  char* password_file;
  char password[MAX_PASSWORD_LENGTH + 1];
  FILE* fp;

  password_file = param("QMF_BROKER_PASSWORD_FILE");
  if (NULL == password_file)
  {
    strcpy(password, "");
  }
  else
  {
    // open the password file with root privileges
    priv_state priv = set_root_priv();
    fp = safe_fopen_wrapper(password_file, "r");
    set_priv(priv);
    if (NULL == fp)
    {
      dprintf(D_ALWAYS, "Unable to open password file (%s)\n", password_file);
      strcpy(password, "");
    }
    else
    {
      size_t sz = fread(password, 1, MAX_PASSWORD_LENGTH, fp);
      fclose(fp);
      if ( 0 == sz )
      {
        dprintf(D_ALWAYS, "Error reading QMF broker password\n");
        strcpy(password, "");
      }

      // Remove trailing whitespace
      for (int i = sz-1; i >= 0 && isspace(password[i]); --i)
      {
        --sz;
      }
      password[sz] = '\0';
      free(password_file);
    }
  }
  return(strdup(password));
}
