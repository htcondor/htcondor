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

#ifndef _COLLECTOR_PLUGIN_H
#define _COLLECTOR_PLUGIN_H

#include "condor_common.h"

#include "PluginManager.h"

#include "condor_classad.h"

/**
 * All Collector plugins must subclass CollectorPlugin. A Collector
 * plugin can receive notification of UPDATE_ and INVALIDATE_ commands
 * received by the Collector.
 *
 * The plugin should have a statically initialized instance of itself,
 * which results in CollectorPlugin's constructor being called and the
 * plugin being registered. See ExampleCollectorPlugin.C for an example.
 *
 * Plugins should be compiled within the source of the Condor version
 * with which they wish to be loaded and with the -shared option,
 * e.g. compile ExampleCollectorPlugin.C with:
 *   $ make ExampleCollectorPlugin-plugin.so
 *   (in Imakefile)
 *   %-plugin.so: %.C:
 *     $(CPlusPlus) $(C_PLUS_FLAGS) -shared $< -o $@
 */
class CollectorPlugin
{
  public:
    /**
     * The default constructor registers the plug-in with
     * CollectorPluginManager::registerPlugin
     * 
     * All plugins should subclass CollectorPlugin and provide a
     * statically initialized instance of itself, so that it will be
     * registered when its object file is loaded
     */
    CollectorPlugin();

	virtual ~CollectorPlugin();

	virtual void initialize() = 0;

	virtual void shutdown() = 0;

		/**
		 * Receive a ClassAd sent as part of an UPDATE_ command,
		 * command int is provided.
		 */
	virtual void update(int command, const ClassAd &) = 0;

		/**
		 * Recieve a ClassAd INVALIDATE_ command, command int
		 * provided.
		 */
	virtual void invalidate(int command, const ClassAd &) = 0;
};


class CollectorPluginManager : public PluginManager<CollectorPlugin>
{
  public:
	static void Initialize();
	static void Shutdown();
	static void Update(int command, const ClassAd &);
	static void Invalidate(int command, const ClassAd &);
	
};

#endif /* _COLLECTOR_PLUGIN_H */
