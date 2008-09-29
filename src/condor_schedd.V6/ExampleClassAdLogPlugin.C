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

#include "ClassAdLogPlugin.h"

struct ExampleClassAdLogPlugin : public ClassAdLogPlugin
{
	void
	initialize()
	{
		printf("Init\n");
	}

	void
	NewClassAd(const char *key)
	{
		printf("NewClassAd: %s\n", key);
	}

	void
	DestroyClassAd(const char *key)
	{
		printf("DestroyClassAd: %s\n", key);
	}

	void
	SetAttribute(const char *key,
				 const char *name,
				 const char *value)
	{
		printf("SetAttribute: %s[%s] = %s\n", key, name, value);
	}

	void
	DeleteAttribute(const char *key,
					const char *name)
	{
		printf("DeleteAttribute: %s[%s]\n", key, name);
	}
};

static ExampleClassAdLogPlugin instance;
