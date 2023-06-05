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
	for (auto plugin: getPlugins()) {
		plugin->earlyInitialize();
	}
}

void
ClassAdLogPluginManager::Initialize()
{
	for (auto plugin: getPlugins()) {
		plugin->initialize();
	}
}

void
ClassAdLogPluginManager::Shutdown()
{
	for (auto plugin: getPlugins()) {
		plugin->shutdown();
	}
}

void
ClassAdLogPluginManager::NewClassAd(const char *key)
{
	for (auto plugin: getPlugins()) {
		plugin->newClassAd(key);
	}
}

void
ClassAdLogPluginManager::DestroyClassAd(const char *key)
{
	for (auto plugin: getPlugins()) {
		plugin->destroyClassAd(key);
	}
}

void
ClassAdLogPluginManager::SetAttribute(const char *key,
								  const char *name,
								  const char *value)
{
	for (auto plugin: getPlugins()) {
		plugin->setAttribute(key, name, value);
	}
}

void
ClassAdLogPluginManager::DeleteAttribute(const char *key,
									 const char *name)
{
	for (auto plugin: getPlugins()) {
		plugin->deleteAttribute(key, name);
	}
}

void
ClassAdLogPluginManager::BeginTransaction() {
	for (auto plugin: getPlugins()) {
		plugin->beginTransaction();
	}
}

void
ClassAdLogPluginManager::EndTransaction() {
	for (auto plugin: getPlugins()) {
		plugin->endTransaction();
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

ClassAdLogPlugin::~ClassAdLogPlugin() = default;


