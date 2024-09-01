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

/*
 * This file contains functions and constants related to negotiation
 * that are needed outside of the negotiator daemon.
 */

/*
 * Parse the limit into a limit and its increment. The input limit is
 * transformed and the increment is 1 if not specified or if < 0.
 *
 * An input limit is a limit plus, optionally, ':' + increment,
 * e.g. m:3 or f.
 */
bool ParseConcurrencyLimit(char *limit, double &increment);

/* Validate a submitter name for use in negotiation.
 * The negotiator will ignore submitters with invalid names.
 * Currently, the name can't contain any whitespace.
 */
bool IsValidSubmitterName( const char *name );
