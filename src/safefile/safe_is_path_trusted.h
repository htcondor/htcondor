#ifndef SAFE_IS_PATH_TRUSTED_H_
#define SAFE_IS_PATH_TRUSTED_H_

/*
 * safefile package    http://www.cs.wisc.edu/~kupsch/safefile
 *
 * Copyright 2007-2008, 2011 James A. Kupsch
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


#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C"  {
#endif

struct safe_id_range_list;

#ifndef SAFE_IS_PATH_TRUSTED_RETRY_MAX
#define SAFE_IS_PATH_TRUSTED_RETRY_MAX 50
#endif



enum {
	SAFE_PATH_ERROR = -1,
	SAFE_PATH_UNTRUSTED,
	SAFE_PATH_TRUSTED_STICKY_DIR,
	SAFE_PATH_TRUSTED,
	SAFE_PATH_TRUSTED_CONFIDENTIAL
    };

int safe_is_path_trusted(
	    const char			*pathname,
	    struct safe_id_range_list	*trusted_uids,
	    struct safe_id_range_list	*trusted_gids
	);
int safe_is_path_trusted_fork(
	    const char			*pathname,
	    struct safe_id_range_list	*trusted_uids,
	    struct safe_id_range_list	*trusted_gids
	);
int safe_is_path_trusted_r(
	    const char			*pathname,
	    struct safe_id_range_list	*trusted_uids,
	    struct safe_id_range_list	*trusted_gids
	);


#ifdef __cplusplus
}
#endif

#endif
