JOB_SUCCESS = 0
JOB_FAILURE = 1

TEST_SKIPPED = -1
TEST_SUCCESS = 0
TEST_FAILURE = 1

#
# FIXME: These should probably be in the bindings proper, whatever the
# appropriately Pythonic way to represent/implement them may be.
#

class JobStatus:
	Number = {
		"Idle": 1,
		"Running": 2,
		"Removed": 3,
		"Completed": 4,
		"Held": 5,
		"TransferringOutput": 6,
		"Suspended": 7
	}

	Name = {
		1: "Idle",
		2: "Running",
		3: "Removed",
		4: "Completed",
		5: "Held",
		6: "TransferringOutput",
		7: "Suspended"
	}

class JobUniverse:
	Number = {
		"standard": 1,
		"vanilla": 5,
		"docker": 5,
		"scheduler": 7,
		"MPI": 8,
		"grid": 9,
		"java": 10,
		"parallel": 11,
		"local": 12,
		"vm": 13
	}

	Name = {
		1: "standard",
		5: "vanilla",
		7: "scheduler",
		8: "MPI",
		9: "grid",
		10: "java",
		11: "parallel",
		12: "local",
		13: "vm"
	}
