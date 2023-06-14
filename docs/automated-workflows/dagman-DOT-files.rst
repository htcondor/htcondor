.. _visualizing-dags-with-dot:

Visualizing DAGs
================

:index:`DOT command<single: DAG input file; DOT command>`
:index:`Visualizing DAGs<single: DAGMan; Visualizing DAGs>`

It can be helpful to see a picture of a DAG. DAGMan can assist you in
visualizing a DAG by creating the input files used by the AT&T Research
Labs *graphviz* package. *dot* is a program within this package,
available from `http://www.graphviz.org/ <http://www.graphviz.org/>`_,
and it is used to draw pictures of DAGs.

DAGMan produces one or more dot files as the result of an extra line in
a DAG input file. The line appears as

.. code-block:: condor-dagman

    DOT dag.dot

This creates a file called ``dag.dot`` which contains a specification
of the DAG before any jobs within the DAG are submitted to HTCondor. The
``dag.dot`` file is used to create a visualization of the DAG by using
this file as input to *dot*. This example creates a Postscript file,
with a visualization of the DAG:

.. code-block:: console

    $ dot -Tps dag.dot -o dag.ps

Within the DAG input file, the DOT command can take several optional
parameters:

-  **UPDATE** This will update the dot file every time a significant
   update happens.
-  **DONT-UPDATE** Creates a single dot file, when the DAGMan begins
   executing. This is the default if the parameter **UPDATE** is not
   used.
-  **OVERWRITE** Overwrites the dot file each time it is created. This
   is the default, unless **DONT-OVERWRITE** is specified.
-  **DONT-OVERWRITE** Used to create multiple dot files, instead of
   overwriting the single one specified. To create file names, DAGMan
   uses the name of the file concatenated with a period and an integer.
   For example, the DAG input file line

   .. code-block:: condor-dagman

        DOT dag.dot DONT-OVERWRITE

   causes files ``dag.dot.0``, ``dag.dot.1``, ``dag.dot.2``, etc. to be
   created. This option is most useful when combined with the **UPDATE**
   option to visualize the history of the DAG after it has finished
   executing.

-  **INCLUDE** *path-to-filename* Includes the contents of a file
   given by ``path-to-filename`` in the file produced by the **DOT**
   command. The include file contents are always placed after the line
   of the form label=. This may be useful if further editing of the
   created files would be necessary, perhaps because you are
   automatically visualizing the DAG as it progresses.

If conflicting parameters are used in a DOT command, the last one listed
is used.
