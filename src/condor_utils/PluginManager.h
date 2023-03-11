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

#ifndef _PLUGIN_MANAGER_H
#define _PLUGIN_MANAGER_H

#include "condor_common.h"

#include <vector>

#include "LoadPlugins.h"

/**
 * NOTE: dlopen and dlfcn.h must be available for this code to work.
 *
 * This plugin manager is primarily to provide a single place for the
 * code in initialize(), and to hold the registered plugins. A daemon
 * specific plugin manager should be created for each daemon. For
 * instance, a daemon that has plugins will need to have a
 * DaemonPlugin class that provides an API the plugins for that daemon
 * must implement, and a DaemonPluginManager that provides a place for
 * plugins to register and may implement functions that apply to all
 * registered plugins. Example:
 *
 *   class ExamplePlugin {
 *     public:
 *       ExamplePlugin() { PluginManager<ExamplePlugin>::registerPlugin(this); }
 *       virtual void action() = 0;
 *   };
 *
 *   class ExamplePluginManager : public PluginManager<ExamplePlugin> {
 *     public:
 *       static void actionAll() {
 *         ExamplePlugin *plugin;
 *         std::vector<ExamplePlugin *> plugins = getPlugins();
 *         plugins.Rewind();
 *         while (plugins.Next(plugin)) {
 *           plugin->action();
 *         }
 *       }
 *   };
 *
 *   template PluginManager<ExamplePlugin>;
 *   template std::vector<ExamplePlugin *>;
 *
 * Then somewhere in the Example daemon's initialization:
 *
 *   ExamplePluginManager::initialize();
 *
 * And somewhere in the Example daemon:
 *
 *   ExamplePluginManager::actionAll();
 *
 * An ExamplePlugin may look like this:
 *
 *   struct ExampleExamplePlugin : public ExamplePlugin {
 *     void action() { printf("Action\n"); }
 *   };
 *   static ExampleExamplePlugin instance;
 *
 * The Load() function checks param("PLUGINS") for a list of plugin
 * object files, failing that it checks for param("PLUGIN_DIR") and
 * takes all files in the directory ending in ".so" as possible
 * plugins. If neither param is found, nothing is done. It is advised
 * that configurations contain SUBSYSTEM.PLUGINS or
 * SUBSYSTEM.PLUGIN_DIR to avoid trying to load plugins for one daemon
 * in another. Once initialize() has a list of plugins it tries to
 * dlopen them and reports errors. The plugins themselves must have a
 * static instance of themselves and the plugin class they implement
 * must have a constructor that registers the plugins. initialize()
 * does not lookup a symbol in the plugins object file to call for
 * registration.
 */
template <class PluginType>
class PluginManager
{
  public:
		/**
		 * Load all plugins using config options, PLUGINS and
		 * PLUGIN_DIR.
		 */
	static void Load(/*bool force = false*/);

		/**
		 * Add a plugin to the set of plugins returned by getPlugins
		 */
	static bool registerPlugin(PluginType *);

		/**
		 * Return a list of plugins for iterating
		 */
	static std::vector<PluginType *> & getPlugins();
};


template <class PluginType>
std::vector<PluginType *> &
PluginManager<PluginType>::getPlugins() {
    static std::vector<PluginType *> plugins;
    return plugins;
}

template<class PluginType>
bool
PluginManager<PluginType>::registerPlugin(PluginType *plugin)
{
	getPlugins().push_back(plugin);
	return true;
}

template <class PluginType>
void
PluginManager<PluginType>::Load(/*bool force*/)
{
	LoadPlugins();
}

#endif /* _PLUGIN_MANAGER_H */
