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
   :noindex:

.. autoclass:: WalkOrder
   :members:
   :noindex:

   .. autoattribute:: BREADTH_FIRST
      :noindex:
   .. autoattribute:: DEPTH_FIRST
      :noindex:


Nodes and Node-likes
++++++++++++++++++++

.. autoclass:: BaseNode
   :members:
   :noindex:

.. autoclass:: NodeLayer
   :show-inheritance:
   :members:
   :noindex:

.. autoclass:: SubDAG
   :show-inheritance:
   :members:
   :noindex:

.. autoclass:: FinalNode
   :show-inheritance:
   :members:
   :noindex:

.. autoclass:: Nodes
   :members:
   :noindex:


Edges
+++++

.. autoclass:: BaseEdge
   :members:
   :noindex:

.. autoclass:: OneToOne
   :noindex:
.. autoclass:: ManyToMany
   :noindex:
.. autoclass:: Grouper
   :noindex:
.. autoclass:: Slicer
   :noindex:


Node Configuration
++++++++++++++++++

.. autoclass:: Script
   :noindex:

.. autoclass:: DAGAbortCondition
   :noindex:


Writing a DAG to Disk
+++++++++++++++++++++

.. autofunction:: write_dag
   :noindex:

.. autoclass:: NodeNameFormatter
   :members:
   :noindex:

.. autoclass:: SimpleFormatter
   :noindex:


DAG Configuration
-----------------

.. autoclass:: DotConfig
   :noindex:

.. autoclass:: NodeStatusFile
   :noindex:


Rescue DAGs
-----------

:mod:`htcondor.dags` can read information from a DAGMan rescue file and apply
it to your DAG as it is being constructed.

See :ref:`Rescue DAG` for more information on Rescue DAGs.

.. autofunction:: rescue
   :noindex:

.. autofunction:: find_rescue_file
   :noindex:
