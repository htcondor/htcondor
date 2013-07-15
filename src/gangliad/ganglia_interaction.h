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

#ifndef __GANGLIAD_INTERACTION_H__
#define __GANGLIAD_INTERACTION_H__

// dummy type definitions
typedef struct Ganglia_pool_s* Ganglia_pool;
typedef struct Ganglia_gmond_config_s* Ganglia_gmond_config;
typedef struct Ganglia_udp_send_channels_s* Ganglia_udp_send_channels;

// From ganglia.h
enum ganglia_slope {
	GANGLIA_SLOPE_ZERO = 0,
	GANGLIA_SLOPE_POSITIVE,
	GANGLIA_SLOPE_NEGATIVE,
	GANGLIA_SLOPE_BOTH,
	GANGLIA_SLOPE_UNSPECIFIED,
	GANGLIA_SLOPE_DERIVATIVE
};

bool ganglia_load_library(const char *libfile);

// This function _or_ ganglia_load_library must succeed in order to use ganglia_send
bool ganglia_init_gmetric(char const *gmetric_path);

bool ganglia_reconfig(const char *config_file, Ganglia_pool *context, Ganglia_gmond_config *config, Ganglia_udp_send_channels *channels);

void ganglia_config_destroy(Ganglia_pool *context, Ganglia_gmond_config *config, Ganglia_udp_send_channels *send_channels);

bool ganglia_send(Ganglia_pool context, Ganglia_udp_send_channels channels, const char *group, const char *name, const char *value, const char *type, const char *units,int slope, const char *title, const char *desc, const char *spoof_host, const char *cluster, int tmax, int dmax);

#endif

