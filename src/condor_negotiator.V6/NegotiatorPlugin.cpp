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
#include "NegotiatorPlugin.h"

#include "condor_config.h"


void
NegotiatorPluginManager::Initialize()
{
	NegotiatorPlugin *plugin;
	SimpleList<NegotiatorPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->initialize();
	}
}

void
NegotiatorPluginManager::Shutdown()
{
	NegotiatorPlugin *plugin;
	SimpleList<NegotiatorPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->shutdown();
	}
}

void
NegotiatorPluginManager::Update(const ClassAd &ad)
{
	NegotiatorPlugin *plugin;
	SimpleList<NegotiatorPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->update(ad);
	}
}

NegotiatorPlugin::NegotiatorPlugin()
{
    if (PluginManager<NegotiatorPlugin>::registerPlugin(this)) {
		dprintf(D_ALWAYS, "Plugin registration succeeded\n");
	} else {
		dprintf(D_ALWAYS, "Plugin registration failed\n");
	}
}

NegotiatorPlugin::~NegotiatorPlugin() { }

