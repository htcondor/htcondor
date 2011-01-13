/*
 * Copyright 2008 Red Hat, Inc.
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

#include "LoadPlugins.unix.h"

#include "condor_config.h"
#include "directory.h"
#include "simplelist.h"
#include <dlfcn.h>

void
LoadPlugins()
{
	static bool skip = false;

	const char *error;
	StringList plugins;
	char *plugin_files;
	MyString plugin_dir;
	const char *plugin_file;

		// Only initialize once /*, except for when we are force'd.*/
	if (skip /*&& !force*/) {
		return;
	}
	skip = true;

		// Setup the plugins StringList to contain the filenames for
		// dlopen. Either a PLUGINS config option is used, preferrably
		// setup as SUBSYSTEM.PLUGINS, or, in the absense of PLUGINS,
		// a PLUGINS_DIR config option is used, also
		// recommended SUBSYSTEM.PLUGINS_DIR.

	dprintf(D_FULLDEBUG, "Checking for PLUGINS config option\n");
	plugin_files = param("PLUGINS");
	if (!plugin_files) {
		char *tmp;
		dprintf(D_FULLDEBUG, "No PLUGINS config option, trying PLUGIN_DIR option\n");
		tmp = param("PLUGIN_DIR");
		if (!tmp) {
			dprintf(D_FULLDEBUG, "No PLUGIN_DIR config option, no plugins loaded\n");

			return;
		} else {
			plugin_dir = tmp;
			free(tmp); tmp = NULL;
			Directory directory(plugin_dir.Value());
			while (NULL != (plugin_file = directory.Next())) {
					// NOTE: This should eventually support .dll for
					// Windows, .dylib for Darwin, .sl for HPUX, etc
				if (0 == strcmp(".so", plugin_file + strlen(plugin_file) - 3)) {
					dprintf(D_FULLDEBUG,
							"PLUGIN_DIR, found: %s\n",
							plugin_file);
					plugins.append((plugin_dir + DIR_DELIM_STRING + plugin_file).Value());
				} else {
					dprintf(D_FULLDEBUG,
							"PLUGIN_DIR, ignoring: %s\n",
							plugin_file);
				}
			}
		}
	} else {
		plugins.initializeFromString(plugin_files);
		free(plugin_files); plugin_files = NULL;
	}

	dlerror(); // Clear error

	plugins.rewind();
	while (NULL != (plugin_file = plugins.next())) {
			// The plugin has a way to automatically register itself
			// when loaded.
			// XXX: When Jim's safe path checking code is available
			//      more generally in Condor the path to the
			//      plugin_file here MUST be checked!
		if (!dlopen(plugin_file, RTLD_NOW)) {
			error = dlerror();
			if (error) {
				dprintf(D_ALWAYS,
						"Failed to load plugin: %s reason: %s\n",
						plugin_file,
						error);
			} else {
				dprintf(D_ALWAYS,
						"Unknown error while loading plugin: %s\n",
						plugin_file);
			}
		} else {
			dprintf(D_ALWAYS, "Successfully loaded plugin: %s\n", plugin_file);
		}
	}

		// NOTE: The handle returned by dlopen is currently ignored,
		// which means dlclose() is not possible.
}
