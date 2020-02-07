from __future__ import absolute_import

from .Globals import *
from .Utils import Utils

import unittest

class EventMemory(object):

	def __init__(self):
		self._list = [ ]

	#
	# Add events
	#
	def Append(self, event):
		self._list.append( event )

	#
	# Get events
	#
	# @return A tuple of events optionally filtered by cluster and/or proc.
	#
	def trace(self, cluster=None, proc=None):
		if cluster is None:
			if proc is None:
				return tuple(self._list)
			else:
				return (event for event in self._list if event.proc == proc)
		else:
			if proc is None:
				return (event for event in self._list if event.cluster == cluster)
			else:
				return (event for event in self._list if event.cluster == cluster and event.proc == proc)


class TestEvent(object):
	def __init__(self, cluster, proc, id):
		self.cluster = cluster
		self.proc = proc
		self.id = id

# This may not be exhaustive in the adversarial sense, but it should do.
class TestEventMemory(unittest.TestCase):
	def setUp(self):
		self._memory = EventMemory()

		self._memory.Append( TestEvent( 1, 0, 0 ) )
		self._memory.Append( TestEvent( 1, 0, 1 ) )
		self._memory.Append( TestEvent( 2, 0, 2 ) )
		self._memory.Append( TestEvent( 2, 0, 3 ) )
		self._memory.Append( TestEvent( 1, 1, 4 ) )
		self._memory.Append( TestEvent( 2, 1, 5 ) )
		self._memory.Append( TestEvent( 1, 1, 6 ) )
		self._memory.Append( TestEvent( 2, 1, 7 ) )
		self._memory.Append( TestEvent( 1, 1, 8 ) )
		self._memory.Append( TestEvent( 2, 1, 9 ) )
		self._memory.Append( TestEvent( 1, 1, 10 ) )
		self._memory.Append( TestEvent( 2, 1, 11 ) )
		self._memory.Append( TestEvent( 1, 0, 12 ) )
		self._memory.Append( TestEvent( 1, 0, 13 ) )
		self._memory.Append( TestEvent( 2, 0, 14 ) )
		self._memory.Append( TestEvent( 2, 0, 15 ) )

	def test_append(self):
		self.assertEqual(
			[event.id for event in self._memory._list],
			range(16)
		)

	def test_trace(self):
		self.assertEqual(
			[event.id for event in self._memory.trace()],
			range(16)
		)
		self.assertEqual(
			[event.id for event in self._memory.trace(cluster=3)],
			[]
		)
		self.assertEqual(
			[event.id for event in self._memory.trace(cluster=1)],
			[0, 1, 4, 6, 8, 10, 12, 13]
		)
		self.assertEqual(
			[event.id for event in self._memory.trace(cluster=2)],
			[2, 3, 5, 7, 9, 11, 14, 15]
		)
		self.assertEqual(
			[event.id for event in self._memory.trace(cluster=1, proc=0)],
			[0,1,12,13]
		)
		self.assertEqual(
			[event.id for event in self._memory.trace(cluster=1, proc=1)],
			[4,6,8,10]
		)
		self.assertEqual(
			[event.id for event in self._memory.trace(cluster=2, proc=0)],
			[2,3,14,15]
		)
		self.assertEqual(
			[event.id for event in self._memory.trace(cluster=2, proc=1)],
			[5,7,9,11]
		)
		self.assertEqual(
			[event.id for event in self._memory.trace(proc=0)],
			[0, 1, 2, 3, 12, 13, 14, 15]
		)
		self.assertEqual(
			[event.id for event in self._memory.trace(proc=1)],
			[4, 5, 6, 7, 8, 9, 10, 11]
		)


if __name__ == "__main__":
    unittest.main()
