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
#include "CollectorPlugin.h"

#include "condor_config.h"
#include "directory.h"
#include <dlfcn.h>


void
CollectorPluginManager::Initialize()
{
	CollectorPlugin *plugin;
	SimpleList<CollectorPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->initialize();
	}
}

void
CollectorPluginManager::Shutdown()
{
	CollectorPlugin *plugin;
	SimpleList<CollectorPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->shutdown();
	}
}

void
CollectorPluginManager::Update(int command, const ClassAd &ad)
{
	CollectorPlugin *plugin;
	SimpleList<CollectorPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->update(command, ad);
	}
}

void
CollectorPluginManager::Invalidate(int command, const ClassAd &ad)
{
	CollectorPlugin *plugin;
	SimpleList<CollectorPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->invalidate(command, ad);
	}
}

CollectorPlugin::CollectorPlugin()
{
    if (PluginManager<CollectorPlugin>::registerPlugin(this)) {
		dprintf(D_ALWAYS, "Plugin registration succeeded\n");
	} else {
		dprintf(D_ALWAYS, "Plugin registration failed\n");
	}
}

CollectorPlugin::~CollectorPlugin() { }
