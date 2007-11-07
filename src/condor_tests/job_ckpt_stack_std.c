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
#include <stdlib.h>

/* This program analyzes the stack at run time to see if it is ok(i.e.
	checkpointed and restored at the right boundaries). It does
	this by calling a recursive function which allocates some
	memory on the stack, fills it with a known value, checks to see
	if the previous values were all correct, and then recurses to
	do the process over again. It continues until it accumulates a
	target size of the runtime stack(counting only the memory it
	allocates on the stack). During these calls, it checkpoints and
	restarts all the way checking the integrity of the run time
	stack. In fact, when the recursion finishes, it back checks the
	runtime stack for each frame it falls backwards. */

/* XXX Right now this program only checkpoints at the deepest level and
	then backchecks all the way. It should checkpoint more often to make
	sure everything is being saved/restored correctly, but it eats up
	far too much time to do it ever stack level. I need to fix it so it 
	does it evey N stack pushes/pops. */

/* This might need adjustment per OS revision */
#define STACKSIZETARGET (1024*384) /* 384K run time stack */

#define STACKINCREMENT 1024 /* bytes per allocated stack array */
#define FALSE 0
#define TRUE 1

#include "x_fake_ckpt.h"


/* This represents an activation record on the run time stack. In fact, this
	is kept on the runtime stack along with the data it points to to really
	make this test depend on a working stack. */
struct Frame
{
	/* This represents the number stored in the array */
	int value;
	/* This points to a stack allocated array */
	int *data;
	/* This size of the stack allocates array */
	int size;
	/* This is null at the root of the activation records */
	struct Frame *previous;
};

void init_data(int *data, int size, int value);
int deepen(struct Frame *pf, int old_value);
int is_stack_valid(struct Frame *f, int value);
int data_valid(int *data, int size, int value);

/* size of the runtime stack at various intervals */
int g_sum = 0;

int main(void)
{
	/* Need to start this process as close to the start of main() as I can */
	int data[STACKINCREMENT];
	struct Frame f;
	int validity;
	int a, b, c;

	init_data(data, STACKINCREMENT, 0);
	/* the stopping condition for is_stack_valid()'s search through the
		previous pointers. It stops when f.value is zero and the data
		array associated with it is zero. Otherwise during the recursion value
		increases by one for each deeper stack frame */
	f.value = 0; 
	f.data = &data[0];
	f.size = STACKINCREMENT;
	f.previous = NULL;
	g_sum += STACKINCREMENT; /* record the fact I added to the stack */

	a = is_stack_valid(&f, 0);
	printf("Deepness Level: %d\n", 0);
	b = deepen(&f, 0);
	printf("Backchecking %d\n", 0), 
	c = is_stack_valid(&f, 0);
	validity = a && b && c;
	if (validity == FALSE)
	{
		printf("FAILED\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("SUCCESS\n");
		exit(EXIT_SUCCESS);
	}

	return EXIT_FAILURE;
}

/* fill the data array with a known value */
void init_data(int *data, int size, int value)
{
	int i;
	
	for (i = 0; i < size; i++)
	{
		data[i] = value;
	}
}

/* keep recursing down until I use up the amount of run time stack space I've
	been allocated */
int deepen(struct Frame *pf, int old_value)
{
	int test;
	int data[STACKINCREMENT];
	struct Frame f;
	int new_value = old_value + 1;
	int a, b;

	printf("Deepness Level: %d\n", new_value);
	init_data(data, STACKINCREMENT, new_value);
	f.value = new_value;
	f.data = &data[0];
	f.size = STACKINCREMENT;
	f.previous = pf;
	g_sum += STACKINCREMENT; /* record the fact I added to the stack */

	test = is_stack_valid(&f, new_value);

	if (test == TRUE)
	{
		if (g_sum < STACKSIZETARGET)
		{
			/* recurse deeper and backcheck the results */
				a = deepen(&f, new_value);
				printf("Backchecking %d\n", new_value), 
				b = is_stack_valid(&f, new_value);

				return a && b;
		}
		/* save the big run time stack in a checkpoint */
		printf("About to checkpoint....\n");
		ckpt_and_exit();
		printf("Returning from checkpoint....\n");
		/* This *should* be true because we just checked it earlier, however
			if it is not, we'll catch the error here. */
		test = is_stack_valid(&f, new_value);
		return test;
	}

	return FALSE;
}

/* check to see of the stack frame and all previous ones are correct given
	a decrementing value for each previous stack frame */
int is_stack_valid(struct Frame *f, int value)
{
	int check;

	check = data_valid(f->data, f->size, value);

	if (value == 0)
	{
		if (check == TRUE)
		{
			return TRUE;
		}
		return FALSE;
	}

	/* check until the value is zero */
	return is_stack_valid(f->previous, value - 1);
}


/* Check the data with a known value */
int data_valid(int *data, int size, int value)
{
	int i;
	
	for (i = 0; i < size; i++)
	{
		if (data[i] != value)
		{
			return FALSE;
		}
	}

	return TRUE;
}


