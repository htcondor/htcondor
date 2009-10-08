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

#include "PluginManager.h"
#include "ClassAdLogPlugin.h"

#include "condor_config.h"
#include <dlfcn.h>


void
ClassAdLogPluginManager::EarlyInitialize()
{
	ClassAdLogPlugin *plugin;
	SimpleList<ClassAdLogPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->earlyInitialize();
	}
}

void
ClassAdLogPluginManager::Initialize()
{
	ClassAdLogPlugin *plugin;
	SimpleList<ClassAdLogPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->initialize();
	}
}

void
ClassAdLogPluginManager::Shutdown()
{
	ClassAdLogPlugin *plugin;
	SimpleList<ClassAdLogPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->shutdown();
	}
}

void
ClassAdLogPluginManager::NewClassAd(const char *key)
{
	ClassAdLogPlugin *plugin;
	SimpleList<ClassAdLogPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->newClassAd(key);
	}
}

void
ClassAdLogPluginManager::DestroyClassAd(const char *key)
{
	ClassAdLogPlugin *plugin;
	SimpleList<ClassAdLogPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->destroyClassAd(key);
	}
}

void
ClassAdLogPluginManager::SetAttribute(const char *key,
								  const char *name,
								  const char *value)
{
	ClassAdLogPlugin *plugin;
	SimpleList<ClassAdLogPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->setAttribute(key, name, value);
	}
}

void
ClassAdLogPluginManager::DeleteAttribute(const char *key,
									 const char *name)
{
	ClassAdLogPlugin *plugin;
	SimpleList<ClassAdLogPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->deleteAttribute(key, name);
	}
}

ClassAdLogPlugin::ClassAdLogPlugin()
{
    if (PluginManager<ClassAdLogPlugin>::registerPlugin(this)) {
		dprintf(D_ALWAYS, "ClassAdLogPlugin registration succeeded\n");
	} else {
		dprintf(D_ALWAYS, "ClassAdLogPlugin registration failed\n");
	}
}

ClassAdLogPlugin::~ClassAdLogPlugin() { }


