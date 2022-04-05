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
#include "condor_config.h"
#include "my_popen.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include "ganglia_interaction.h"

/*
 * There are two ways of interfacing with ganglia.  One is to dlopen
 * libganglia.  Ther other is to execute the command-line tool
 * gmetric.  Old versions of libganglia such as SL5 vintage 3.0.7
 * cannot be dlopened in a reasonable way, because they depend on
 * specific versions of libconfuse and libapr being linked into the
 * caller (not necessarily the same version as is installed on the
 * system).  Therefore, gmetric is the only option in such cases.
 * However, gmetric is about 200 times slower than direct calls
 * to libganglia, so for best performance, we retain support
 * for libganglia.
 */

void *libganglia = NULL;
std::string libganglia_libfile;

std::string gmetric_path;
std::string gmetric_conf_file;
bool gmetric_supports_desc = false;
bool gmetric_supports_title = false;
bool gmetric_supports_group = false;
bool gmetric_supports_cluster = false;
bool gmetric_supports_derivative = false;

// dummy type definitions
typedef struct Ganglia_metric_s *Ganglia_metric;

/* libganglia function pointers */

#ifdef __cplusplus
extern "C" {
#endif

void (*Ganglia_pool_destroy_dl)( Ganglia_pool pool );
Ganglia_pool (*Ganglia_pool_create_dl)( Ganglia_pool parent );
Ganglia_gmond_config (*Ganglia_gmond_config_create_dl)(char const *path, int fallback_to_default);
Ganglia_udp_send_channels (*Ganglia_udp_send_channels_create_dl)(Ganglia_pool p, Ganglia_gmond_config config);
Ganglia_metric (*Ganglia_metric_create_dl)( Ganglia_pool parent_pool );
void (*Ganglia_metadata_add_dl)( Ganglia_metric gmetric, char const *name, char const *value );
int (*Ganglia_metric_set_dl)( Ganglia_metric gmetric, char const *name, char const *value, char const *type, char const *units, unsigned int slope, unsigned int tmax, unsigned int dmax);
int (*Ganglia_metric_send_dl)( Ganglia_metric gmetric, Ganglia_udp_send_channels send_channels );
void (*Ganglia_metric_destroy_dl)( Ganglia_metric gmetric );

#ifdef __cplusplus
}
#endif

// Work around C++ casting issues with dlsym()
static bool
dlsym_assign(void *lib,char const *symname,void **fn)
{
	*fn = dlsym(lib,symname);
	return *fn != NULL;
}

bool
ganglia_load_library(const char *libfile)
{
	if( libganglia ) {
		if( libganglia_libfile == libfile ) {
			return true; // already have this library loaded
		}

		dlclose(libganglia);
		libganglia = NULL;
	}
	libganglia = dlopen(libfile,RTLD_NOW);
	if( !libganglia ) {
		char const *e = dlerror();
		dprintf(D_ALWAYS,"Failed to load %s: %s\n",libfile,e ? e : "");
		return false;
	}
	libganglia_libfile = libfile;

	char const *s = NULL;
	if( !(dlsym_assign(libganglia,(s="Ganglia_pool_destroy"),(void **)&Ganglia_pool_destroy_dl)) ||
		!(dlsym_assign(libganglia,(s="Ganglia_pool_create"),(void **)&Ganglia_pool_create_dl)) ||
		!(dlsym_assign(libganglia,(s="Ganglia_gmond_config_create"),(void **)&Ganglia_gmond_config_create_dl)) ||
		!(dlsym_assign(libganglia,(s="Ganglia_udp_send_channels_create"),(void **)&Ganglia_udp_send_channels_create_dl)) ||
		!(dlsym_assign(libganglia,(s="Ganglia_metric_create"),(void **)&Ganglia_metric_create_dl)) ||
		!(dlsym_assign(libganglia,(s="Ganglia_metadata_add"),(void **)&Ganglia_metadata_add_dl)) ||
		!(dlsym_assign(libganglia,(s="Ganglia_metric_set"),(void **)&Ganglia_metric_set_dl)) ||
		!(dlsym_assign(libganglia,(s="Ganglia_metric_send"),(void **)&Ganglia_metric_send_dl)) ||
		!(dlsym_assign(libganglia,(s="Ganglia_metric_destroy"),(void **)&Ganglia_metric_destroy_dl)) )
	{
		dprintf(D_ALWAYS,"Failed to find symbol %s in ganglia library %s: %s",s,libfile,dlerror());
		dlclose(libganglia);
		libganglia = NULL;
		return false;
	}
	return true;
}

bool
ganglia_init_gmetric(char const *_gmetric_path)
{
	char const *argv[] = {
		_gmetric_path,
		"-h",
		NULL
	};

	FILE *fp = my_popenv(argv,"r",MY_POPEN_OPT_WANT_STDERR);
	if( !fp ) {
		dprintf(D_ALWAYS,"Failed to execute %s -h: %s\n",_gmetric_path,strerror(errno));
		return false;
	}

	gmetric_path = _gmetric_path;

	char line[1024];
	while( fgets(line,sizeof(line),fp) ) {
		if( strstr(line,"--desc=") ) {
			gmetric_supports_desc = true;
			dprintf(D_FULLDEBUG,"gmetric supports --desc\n");
		}
		if( strstr(line,"--title=") ) {
			gmetric_supports_title = true;
			dprintf(D_FULLDEBUG,"gmetric supports --title\n");
		}
		if( strstr(line,"--group=") ) {
			gmetric_supports_group = true;
			dprintf(D_FULLDEBUG,"gmetric supports --group\n");
		}
		if( strstr(line,"--cluster=") ) {
			gmetric_supports_cluster = true;
			dprintf(D_FULLDEBUG,"gmetric supports --cluster\n");
		}
		if( strstr(line,"--slope=") && strstr(line,"derivative") ) {
			gmetric_supports_derivative = true;
			dprintf(D_FULLDEBUG,"gmetric supports --slope=derivative\n");
		}
	}

	my_pclose(fp);
	return true;
}

void
ganglia_config_destroy(Ganglia_pool *context, Ganglia_gmond_config *config, Ganglia_udp_send_channels *send_channels)
{
    if (*config)
    {
        // This function is defined in the header but does not actually exist in the library.
        //Ganglia_gmond_config_destroy(*config);
        *config = NULL;
    }
    if (*send_channels)
    {
        // This function is defined in the header but does not actually exist in the library.
        //Ganglia_udp_send_channels_destroy(*send_channels);
        *send_channels = NULL;
    }
    if (*context)
    {
        (*Ganglia_pool_destroy_dl)(*context);
        *context = NULL;
    }
}

bool
ganglia_reconfig(const char *config_file, Ganglia_pool *context, Ganglia_gmond_config *config, Ganglia_udp_send_channels *send_channels)
{
	ASSERT( config_file );
	gmetric_conf_file = config_file;
	if( !libganglia ) {
		if( !gmetric_path.empty() ) {
			return true;
		}
		return false;
	}

	ganglia_config_destroy(context,config,send_channels);

    *context = (*Ganglia_pool_create_dl)(NULL);
    if (!*context)
    {
        return false;
    }

    *config = (*Ganglia_gmond_config_create_dl)(config_file, 0);

    if (!*config)
    {
        return false;
    }

    *send_channels = (*Ganglia_udp_send_channels_create_dl)(*context, *config);
    if (!*send_channels)
    {
        return false;
    }

    return true;
}

static bool
gmetric_send(const char *group, const char *name, const char *value, const char *type, const char *units, int slope, const char *title, const char *desc, const char *spoof_host, const char *cluster, int tmax, int dmax)
{
	ArgList args;
	args.AppendArg(gmetric_path.c_str());

	if( !gmetric_conf_file.empty() ) {
		args.AppendArg("--conf");
		args.AppendArg(gmetric_conf_file.c_str());
	}
	if( group && gmetric_supports_group ) {
		args.AppendArg("--group");
		// gmetric treats spaces in the group name as delimiters between multiple groups.
		// Since we do not want that, replace spaces with dots
		std::string g(group);
		for(size_t i=0;i<g.size();i++) {
			if( g[i] == ' ' ) {
				g[i] = '.';
			}
		}
		args.AppendArg(g.c_str());
	}
	if( name ) {
		args.AppendArg("--name");
		args.AppendArg(name);
	}
	if( value ) {
		args.AppendArg("--value");
		args.AppendArg(value);
	}
	if( type ) {
		args.AppendArg("--type");
		args.AppendArg(type);
	}
	if( units ) {
		args.AppendArg("--units");
		args.AppendArg(units);
	}
	switch(slope) {
	case GANGLIA_SLOPE_ZERO:
		args.AppendArg("--slope");
		args.AppendArg("zero");
		break;
	case GANGLIA_SLOPE_POSITIVE:
		args.AppendArg("--slope");
		args.AppendArg("positive");
		break;
	case GANGLIA_SLOPE_NEGATIVE:
		args.AppendArg("--slope");
		args.AppendArg("negative");
		break;
	case GANGLIA_SLOPE_BOTH:
		args.AppendArg("--slope");
		args.AppendArg("both");
		break;
	case GANGLIA_SLOPE_UNSPECIFIED:
		break;
	case GANGLIA_SLOPE_DERIVATIVE:
		if( gmetric_supports_derivative ) {
			args.AppendArg("--slope");
			args.AppendArg("derivative");
		}
		else {
			dprintf(D_ALWAYS,"Not publishing %s, because gmetric does not support --slope=derivative\n",name);
			return false;
		}
		break;
	}

	if( title && gmetric_supports_title ) {
		args.AppendArg("--title");
		args.AppendArg(title);
	}
	if( desc && gmetric_supports_desc ) {
		args.AppendArg("--desc");
		args.AppendArg(desc);
	}
	if( spoof_host ) {
		args.AppendArg("--spoof");
		args.AppendArg(spoof_host);
	}
	if( cluster && gmetric_supports_cluster ) {
		args.AppendArg("--cluster");
		args.AppendArg(cluster);
	}

	args.AppendArg("--tmax");
	args.AppendArg(tmax);

	args.AppendArg("--dmax");
	args.AppendArg(dmax);

	FILE *fp = my_popen(args,"r",MY_POPEN_OPT_WANT_STDERR);
	char line[1024];
	std::string output;
	if( !fp ) {
		std::string display_args;
		args.GetArgsStringForDisplay( display_args);
		dprintf(D_ALWAYS,"Failed to execute %s: %s\n",display_args.c_str(),strerror(errno));
		return false;
	}
	while( fgets(line,sizeof(line),fp) ) {
		output += line;
	}
	int rc = my_pclose(fp);
	if( rc != 0 ) {
		std::string display_args;
		args.GetArgsStringForDisplay( display_args);
		dprintf(D_ALWAYS,"Failed to execute %s: %s\n",display_args.c_str(),output.c_str());
		return false;
	}
	return true;
}

bool
ganglia_send(Ganglia_pool context, Ganglia_udp_send_channels channels, const char *group, const char *name, const char *value, const char *type, const char *units, int slope, const char *title, const char *desc, const char *spoof_host, const char *cluster, int tmax, int dmax)
{
	if( !libganglia ) {
		return gmetric_send(group,name,value,type,units,slope,title,desc,spoof_host,cluster,tmax,dmax);
	}
    Ganglia_metric metric = (*Ganglia_metric_create_dl)(context);
    if (!metric) return false;

    int retval = 0;
	if (!(*Ganglia_metric_set_dl)(metric, name, value, type, units, slope, tmax, dmax))
    {
        (*Ganglia_metadata_add_dl)(metric, const_cast<char*>("GROUP"), group);
		if( title && *title ) {
			(*Ganglia_metadata_add_dl)(metric, const_cast<char*>("TITLE"), title);
		}
		if( desc && *desc ) {
			(*Ganglia_metadata_add_dl)(metric, const_cast<char*>("DESC"), desc);
		}
		if( spoof_host && *spoof_host ) {
			(*Ganglia_metadata_add_dl)(metric, const_cast<char*>("SPOOF_HOST"), spoof_host);
		}
		if( cluster && *cluster ) {
			(*Ganglia_metadata_add_dl)(metric, const_cast<char*>("CLUSTER"), cluster);
		}
        if ((*Ganglia_metric_send_dl)(metric, channels))
        {
            retval = 2;
        }
    } else
    {
        retval = 3;
    }
	(*Ganglia_metric_destroy_dl)(metric);

    return retval==0;
}

static bool
gmetric_send_heartbeat(const char *spoof_host)
{
	ArgList args;
	args.AppendArg(gmetric_path.c_str());

	if( !gmetric_conf_file.empty() ) {
		args.AppendArg("--conf");
		args.AppendArg(gmetric_conf_file.c_str());
	}
	if( spoof_host ) {
		args.AppendArg("--spoof");
		args.AppendArg(spoof_host);
	}

	args.AppendArg("--heartbeat");

	FILE *fp = my_popen(args,"r",MY_POPEN_OPT_WANT_STDERR);
	char line[1024];
	std::string output;
	if( !fp ) {
		std::string display_args;
		args.GetArgsStringForDisplay( display_args);
		dprintf(D_ALWAYS,"Failed to execute %s: %s\n",display_args.c_str(),strerror(errno));
		return false;
	}
	while( fgets(line,sizeof(line),fp) ) {
		output += line;
	}
	int rc = my_pclose(fp);
	if( rc != 0 ) {
		std::string display_args;
		args.GetArgsStringForDisplay( display_args);
		dprintf(D_ALWAYS,"Failed to execute %s: %s\n",display_args.c_str(),output.c_str());
		return false;
	}
	return true;
}

bool
ganglia_send_heartbeat(Ganglia_pool context, Ganglia_udp_send_channels channels, const char *spoof_host)
{
	if( !libganglia ) {
		return gmetric_send_heartbeat(spoof_host);
	}
    Ganglia_metric metric = (*Ganglia_metric_create_dl)(context);
    if (!metric) return false;

    int retval = 0;
	if (!(*Ganglia_metric_set_dl)(metric, "heartbeat", 0, "uint32", "", 0, 0, 0))
    {
		if( spoof_host && *spoof_host ) {
			(*Ganglia_metadata_add_dl)(metric, const_cast<char*>("SPOOF_HOST"), spoof_host);
		}
        if ((*Ganglia_metric_send_dl)(metric, channels))
        {
            retval = 2;
        }
    } else
    {
        retval = 3;
    }
	(*Ganglia_metric_destroy_dl)(metric);

    return retval==0;
}
