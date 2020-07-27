:mod:`htcondor.dags` API Reference
==================================

.. module:: htcondor.dags

.. py:currentmodule:: htcondor.dags

.. attention::
    This is not documentation for DAGMan itself! If you run into DAGMan jargon
    that isn't explained here, see :ref:`dagman-workflows`.


Creating DAGs
-------------

.. autoclass:: DAG
   :members:

.. autoclass:: WalkOrder
   :members:

   .. autoattribute:: BREADTH_FIRST
   .. autoattribute:: DEPTH_FIRST


Nodes and Node-likes
++++++++++++++++++++

.. autoclass:: BaseNode
   :members:

.. autoclass:: NodeLayer
   :show-inheritance:
   :members:

.. autoclass:: SubDAG
   :show-inheritance:
   :members:

.. autoclass:: FinalNode
   :show-inheritance:
   :members:

.. autoclass:: Nodes
   :members:


Edges
+++++

.. autoclass:: BaseEdge
   :members:

.. autoclass:: OneToOne
.. autoclass:: ManyToMany
.. autoclass:: Grouper
.. autoclass:: Slicer


Node Configuration
++++++++++++++++++

.. autoclass:: Script

.. autoclass:: DAGAbortCondition


Writing a DAG to Disk
+++++++++++++++++++++

.. autofunction:: write_dag

.. autoclass:: NodeNameFormatter
   :members:

.. autoclass:: SimpleFormatter


DAG Configuration
-----------------

.. autoclass:: DotConfig

.. autoclass:: NodeStatusFile


Rescue DAGs
-----------

:mod:`htcondor.dags` can read information from a DAGMan rescue file and apply
it to your DAG as it is being constructed.

See :ref:`rescue-dags` for more information on Rescue DAGs.

.. autofunction:: rescue

.. autofunction:: find_rescue_file
