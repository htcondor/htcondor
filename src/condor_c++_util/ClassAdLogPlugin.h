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

#ifndef _CLASSADLOG_PLUGIN_H
#define _CLASSADLOG_PLUGIN_H

#include "condor_common.h"

#include "PluginManager.h"

/**
 * All ClassAdLog plugins must subclass ClassAdLogPlugin. A ClassAdLog
 * plugin can receive notification of NewClassAd, SetAttribute,
 * DeleteAttribute and DestroyClassAd commands.
 *
 * The plugin should have a statically initialized instance of itself,
 * which results in ClassAdLogPlugin's constructor being called and
 * the plugin being registered.
 *
 * Plugins should be compiled within the source of the Condor version
 * with which they wish to be loaded and with the -shared option,
 * e.g. compile ExampleClassAdLogPlugin.C with:
 *   $ make ExampleClassAdLogPlugin-plugin.so
 *   (in Imakefile)
 *   %-plugin.so: %.C:
 *     $(CPlusPlus) $(C_PLUS_FLAGS) -shared $< -o $@
 */
class ClassAdLogPlugin
{
  public:
    /**
     * The default constructor registers the plug-in with
     * ClassAdLogPluginManager::registerPlugin
     * 
     * All plugins should subclass ClassAdLogPlugin and provide a
     * statically initialized instance of itself, so that it will be
     * registered when its object file is loaded
     */
    ClassAdLogPlugin();

	virtual ~ClassAdLogPlugin();

	virtual void earlyInitialize() = 0;
	virtual void initialize() = 0;
	virtual void shutdown() = 0;

	virtual void newClassAd(const char *key) = 0;
	virtual void destroyClassAd(const char *key) = 0;
	virtual void setAttribute(const char *key,
							  const char *name,
							  const char *value) = 0;
	virtual void deleteAttribute(const char *key,
								 const char *name) = 0;
};


class ClassAdLogPluginManager : public PluginManager<ClassAdLogPlugin>
{
  public:
	static void EarlyInitialize();
	static void Initialize();
	static void Shutdown();
	static void NewClassAd(const char *key);
	static void DestroyClassAd(const char *key);
	static void SetAttribute(const char *key,
							 const char *name,
							 const char *value);
	static void DeleteAttribute(const char *key,
								const char *name);
};

#endif /* _CLASSADLOG_PLUGIN_H */
