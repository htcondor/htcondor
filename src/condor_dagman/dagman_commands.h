/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef DAGMAN_COMMANDS_H
#define DAGMAN_COMMANDS_H

bool handle_command_generic(const ClassAd& request, ClassAd& response, Dagman& dm);

Node* AddNode( Dag *dag, const char *name,
			  const char* directory,
			  const char* submitFileOrSubmitDesc,
			  bool noop,
			  bool done, NodeType type, std::string &failReason );

/** Set the DAG file (if any) for a node.
	@param dag: the DAG this node is part of
	@param nodeName: the name of the node
	@param dagFile: the name of the DAG file
	@param whynot: holds error message if something went wrong
	@return true if successful, false otherwise
*/
bool SetNodeDagFile( Dag *dag, const char *nodeName, const char *dagFile,
			std::string &whynot );

bool IsValidNodeName( Dag *dm, const char *name, std::string &whynot );
bool IsValidSubmitName( const char *name, std::string &whynot );

#endif	// ifndef DAGMAN_COMMANDS_H
