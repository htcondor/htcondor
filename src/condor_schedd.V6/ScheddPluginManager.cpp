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
#include "ScheddPlugin.h"

#include "condor_config.h"

void
ScheddPluginManager::EarlyInitialize()
{
	ScheddPlugin *plugin;
	SimpleList<ScheddPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->earlyInitialize();
	}
}

void
ScheddPluginManager::Initialize()
{
	ScheddPlugin *plugin;
	SimpleList<ScheddPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->initialize();
	}
}

void
ScheddPluginManager::Shutdown()
{
	ScheddPlugin *plugin;
	SimpleList<ScheddPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->shutdown();
	}
}

void
ScheddPluginManager::Update(int cmd, const ClassAd *ad)
{
	ScheddPlugin *plugin;
	SimpleList<ScheddPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->update(cmd, ad);
	}
}

void
ScheddPluginManager::Archive(const ClassAd *ad)
{
	ScheddPlugin *plugin;
	SimpleList<ScheddPlugin *> plugins = getPlugins();
	plugins.Rewind();
	while (plugins.Next(plugin)) {
		plugin->archive(ad);
	}
}

ScheddPlugin::ScheddPlugin()
{
    if (PluginManager<ScheddPlugin>::registerPlugin(this)) {
		dprintf(D_ALWAYS, "ScheddPlugin registration succeeded\n");
	} else {
		dprintf(D_ALWAYS, "ScheddPlugin registration failed\n");
	}
}

ScheddPlugin::~ScheddPlugin() { }


