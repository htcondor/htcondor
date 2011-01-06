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

extern "C" 
{

	enum ClassAdSharedValueType
	{
		ClassAdSharedType_Integer,
		ClassAdSharedType_Float,
		ClassAdSharedType_String,
		ClassAdSharedType_Undefined,
		ClassAdSharedType_Error
	};
	
	struct ClassAdSharedValue
	{
		ClassAdSharedValueType  type;
		union
		{
			int    integer;
			float  real;
			char   *text; // Callee should allocate memory for this using new
		};
	};
	
	typedef void(*ClassAdSharedFunction)(const int number_of_arguments,
										 const ClassAdSharedValue *arguments,
										 ClassAdSharedValue *result);

}
