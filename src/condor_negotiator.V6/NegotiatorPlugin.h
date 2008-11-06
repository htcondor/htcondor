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

#ifndef _NEGOTIATOR_PLUGIN_H
#define _NEGOTIATOR_PLUGIN_H

#include "condor_common.h"

#include "PluginManager.h"

#include "condor_classad.h"

/**
 * All Negotiator plugins must subclass NegotiatorPlugin. A Negotiator
 * plugin can receive notification of UPDATE_ commands sent from the
 * Negotiator.
 *
 * The plugin should have a statically initialized instance of itself,
 * which results in NegotiatorPlugin's constructor being called and the
 * plugin being registered. See ExampleNegotiatorPlugin.C for an example.
 *
 * Plugins should be compiled within the source of the Condor version
 * with which they wish to be loaded and with the -shared option,
 * e.g. compile ExampleNegotiatorPlugin.C with:
 *   $ make ExampleNegotiatorPlugin-plugin.so
 *   (in Imakefile)
 *   %-plugin.so: %.C:
 *     $(CPlusPlus) $(C_PLUS_FLAGS) -shared $< -o $@
 */
class NegotiatorPlugin
{
  public:
    /**
     * The default constructor registers the plug-in with
     * NegotiatorPluginManager::registerPlugin
     * 
     * All plugins should subclass NegotiatorPlugin and provide a
     * statically initialized instance of itself, so that it will be
     * registered when its object file is loaded
     */
    NegotiatorPlugin();

	virtual ~NegotiatorPlugin();

	virtual void initialize() = 0;

	virtual void shutdown() = 0;

		/**
		 * Receive the ClassAd sent to the Collector.
		 */
	virtual void update(const ClassAd &) = 0;
};


class NegotiatorPluginManager : public PluginManager<NegotiatorPlugin>
{
  public:
	static void Initialize();
	static void Shutdown();
	static void Update(const ClassAd &);
};

#endif /* _NEGOTIATOR_PLUGIN_H */
