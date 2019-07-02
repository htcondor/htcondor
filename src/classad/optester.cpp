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


#include <stdio.h>
#include "predefContexts.h"
#include "classad/source.h"

using namespace std;

int main (void)
{
    char        buffer[1024];
    int         buflen=1024;
    Source      source;
    Sink        sink;
    LocalContext le;
    Value       val;
	int			op;
	int			type1, type2, type3;
	ExprTree	*tree;
	char		*values[] = {"undefined", "error", "10", "3.141", "\"strVal\""};

	sink.setSink (stdout);

	// for each op
	for (op = __FIRST_OP__;	op <= __LAST_OP__; op++)
	{
		printf ("\n\nTesting operator: %s\n", Operation::opString[op]);

		// take care of unary ops
		if (op == UNARY_PLUS_OP 	|| 
			op == UNARY_MINUS_OP 	|| 
			op == LOGICAL_NOT_OP	||
			op == BITWISE_NOT_OP)
		{
			// only one operand required
			for (type1 = 0; type1 < 5; type1++)
			{
				sprintf (buffer,"%s %s", Operation::opString[op],values[type1]);
				printf ("%s => ", buffer);
				source.setSource (buffer, buflen);
				tree = ExprTree::fromSource (source);
				if (!tree)	
				{
					printf ("parse error!\n");
					continue;
				}
				le.evaluate (tree, val);
				val.toSink (sink);
				sink.flushSink();
				delete tree;
				printf ("\n");
			}
		}
		else
		if (op == PARENTHESES_OP)
		{
			// very similar to above
			for (type1 = 0; type1 < 5; type1++)
            {
                sprintf (buffer,"( %s )", values[type1]);
                printf ("%s => ", buffer);
                source.setSource (buffer, buflen);
                tree = ExprTree::fromSource (source);   
                if (!tree)  
                {
                    printf ("parse error!\n");        
                    continue;
                }
                le.evaluate (tree, val);
                val.toSink (sink);
                sink.flushSink();
                delete tree;
                printf ("\n");
            }
        }
		else
		if (op == TERNARY_OP)
		{
			// need three operands
			for (type1 = 0; type1 < 5; type1++)
			{
				for (type2 = 0; type2 < 5; type2++)
				{
					for (type3 = 0; type3 < 5; type3++)
					{
						sprintf (buffer, "%s ? %s : %s", values[type1], 
									values[type2], values[type3]);
						printf ("%s => ", buffer);
						source.setSource (buffer, buflen);
						tree = ExprTree::fromSource (source);
						if (!tree)
						{
							printf ("parse error!\n");
							continue;
						}
						le.evaluate (tree, val);
						val.toSink (sink);
						sink.flushSink ();
						delete tree;
						printf ("\n");
					}
				}
			}
		}
		else
		{
			// a binary operation ---  need two operands
            for (type1 = 0; type1 < 5; type1++)
            {
                for (type2 = 0; type2 < 5; type2++)
                {
					sprintf (buffer, "%s %s %s", values[type1], 
								Operation::opString[op], values[type2]);
					printf ("%s => ", buffer);
					source.setSource (buffer, buflen);
					tree = ExprTree::fromSource (source);
					if (!tree)
					{
						printf ("parse error!\n");
						continue;
					}
					le.evaluate (tree, val);
					val.toSink (sink);
					sink.flushSink ();
					delete tree;
					printf ("\n");
				}
            }

		}
	}
	
	return 0;
}
