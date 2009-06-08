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

#ifndef _SCHEDD_PLUGIN_H
#define _SCHEDD_PLUGIN_H

#include "condor_common.h"

#include "PluginManager.h"

#include "condor_classad.h"

/**
 * All Schedd plugins must subclass ScheddPlugin. A Schedd plugin can
 * receive notification an update is set to the pool's main Collector.
 *
 * The plugin should have a statically initialized instance of itself,
 * which results in ScheddPlugin's constructor being called and the
 * plugin being registered.
 *
 * Plugins should be compiled within the source of the Condor version
 * with which they wish to be loaded and with the -shared option,
 * e.g. compile ExampleScheddPlugin.C with:
 *   $ make ExampleScheddPlugin-plugin.so
 *   (in Imakefile)
 *   %-plugin.so: %.C:
 *     $(CPlusPlus) $(C_PLUS_FLAGS) -shared $< -o $@
 */
class ScheddPlugin
{
  public:
    /**
     * The default constructor registers the plug-in with
     * ScheddPluginManager::registerPlugin
     * 
     * All plugins should subclass ScheddPlugin and provide a
     * statically initialized instance of itself, so that it will be
     * registered when its object file is loaded
     */
    ScheddPlugin();

	virtual ~ScheddPlugin();

		// Called before the Scheduler is initialized
	virtual void earlyInitialize() = 0;

		// Called after the Scheduler is initialized
	virtual void initialize() = 0;

	virtual void shutdown() = 0;

	virtual void update(int cmd, const ClassAd *ad) = 0;

	virtual void archive(const ClassAd *ad) = 0;
};


class ScheddPluginManager : public PluginManager<ScheddPlugin>
{
  public:
	static void EarlyInitialize();

	static void Initialize();

	static void Shutdown();

	static void Update(int cmd, const ClassAd *ad);

	static void Archive(const ClassAd *ad);
};

#endif /* _SCHEDD_PLUGIN_H */
