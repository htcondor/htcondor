DAG Save Point Files
====================

:index:`DAG save point file<single: DAGMan; DAG save point file>`

A DAG can be set up to write the current progress of the DAG at specified
nodes to a save point file. These files are written the first time the
designated node starts running. Meaning any retries won't save the DAG
progress again. The save point file is written in the exact same format
as a partial Rescue DAG except that all node retry values will be reset
to their max value. The DAG save point file can then be specified when
re-running a DAG to start the DAG at a certain point of progress.

To specify a save point file use the DAG submit description keyword
``SAVE_POINT_FILE`` followed by the name of the node designated as the save
point to write a save file, and optionally a filename. If a filename is not
specified the file will be written as ``[Node Name]-[DAG filename].save``
where the DAG filename is the DAG file that the save file declaration was
read from.

If the specified save point filename includes a path then DAGMan will attempt
to write the file to that location. If the *condor_submit_dag* ``useDagDir``
flag is used and a path is specified for a save point then the file will be
written to that path relative to a DAG's working directory. Any save point
files without a specified path will be written to a sub-directory called
``save_files`` created near all other DAGMan procuded files (i.e. ``.condor.sub``,
``.dagman.out``, etc.).

.. code-block:: condor-dagman

    # File: savepointEx.dag
    JOB A node.sub
    JOB B node.sub
    JOB C node.sub
    JOB D node.sub

    PARENT A B C CHILD D

    #SAVE_POINT_FILE NodeName Filename
    SAVE_POINT_FILE A
    SAVE_POINT_FILE B Node-B_custom.save
    SAVE_POINT_FILE C ../example/subdir/Node-C_custom.save
    SAVE_POINT_FILE D ./Node-D_custom.save

Given the above example DAG file, if ``condor_submit_dag savepointEx.dag`` was ran
from the below directory ``my_work`` then the produced files appear in the
directory tree as follows:

.. code-block:: text

    Directory Tree Visualized
    └─Home
        ├─example
        │   └─subdir
        │       └─Node-C_custom.save
        └─my_work
            ├─savepointEx.dag
            ├─savepointEx.dag.condor.sub
            ├─savepointEx.dag.dagman.out
            ├─...
            ├─Node-D_custom.save
            └─save_files
                  ├─ A-savepointEx.dag.save
                  └─ Node-B_custom.save

Once a DAG has ran and produce save point files, the DAG can then be re-run from
a save file by passing a filename via the ``-load_save`` flag for *condor_submit_dag*.
If the save point file is passed with a specified path then DAGMan will attempt to
read the file from that path. If just a save point filename is given then DAGMan will
assume the file is located in the``save_files`` directory. The path to save point
files will be checked relative to the current working directory that *condor_submit_dag*
was ran from.

When DAGMan writes save point files, if a save file with the same name already exists
then DAGMan will rotate the file to ``[filename].old`` before writing the new save.
Any already existing "old" save files will be removed prior to rotation and saving.
So, if the above example DAG was re-run with ``condor_submit_dag -load_save
./Node-D_custom.save savepointEx.dag`` from the same directory then once node D starts
the previous save would become ``Node-D_custom.save.old``. This behavior does not just
effect save point files when re-running a DAG. If a DAG was set up as follows:

.. code-block:: condor-dagman

    # File: progressSavefile.dag
    JOB A node.sub
    JOB B node.sub
    JOB C node.sub
    ...
    SAVE_POINT_FILE A dag-progress.save
    SAVE_POINT_FILE B dag-progress.save
    SAVE_POINT_FILE C dag-progress.save

Then assuming the parent/child relationships is ``A->B->C``, the first save written at
the start of node A will be written to ``dag-progress.save``. Then when node B starts
the present ``dag-progress.save`` will become ``dag-progress.save.old`` and a new
``dag-progress.save`` will be written. Finally, once node C starts ``dag-progress.save.old``
will be deleted, the present ``dag-progress.save`` will become ``dag-progress.save.old``
and a new ``dag-progress.save`` will be written. Allowing a single save file that progresses
with the DAG to be created.
