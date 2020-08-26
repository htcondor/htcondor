      

Job Monitor/Log Viewer
======================

:index:`Job monitor` :index:`log files<single: log files; viewing>`

The HTCondor Job Monitor is a Java application designed to allow users
to view user log files. It is identified as the Contrib Module called
HTCondor Log Viewer.

To view a user log file, select it using the open file command in the
File menu. After the file is parsed, it will be visually represented.
Each horizontal line represents an individual job. The x-axis is time.
Whether a job is running at a particular time is represented by its
color at that time - white for running, black for idle. For example, a
job which appears predominantly white has made efficient progress,
whereas a job which appears predominantly black has received an
inordinately small proportion of computational time.

Transition States
-----------------

A transition state is the state of a job at any time. It is called a
transition, because it is defined by the two events which bookmark it.
There are two basic transition states: running and idle. An idle job
typically is a job which has just been submitted into the HTCondor pool
and is waiting to be matched with an appropriate machine or a job which
has vacated from a machine and has been returned to the pool. A running
job, by contrast, is a job which is making active progress.

Advanced users may want a visual distinction between two types of
running transitions: goodput or badput. Goodput is the transition state
preceding an eventual job completion or checkpoint. Badput is the
transition state preceding a non-checkpointed eviction event. Note that
badput is potentially a misleading nomenclature; a job which does not
produce a checkpoint by the HTCondor program may produce the checkpoint
itself or make progress in some other way. To view these two transition
as distinct transitions, select the appropriate option from the "View"
menu.

Events
------

There are two basic kinds of events: checkpoint events and error events.
Plus, advanced users can ask to see more events.

Selecting Jobs
--------------

To view any arbitrary selection of jobs in a job file, use the job
selector tool. Jobs appear visually by order of appearance within the
actual text log file. For example, the log file might contain jobs
775.1, 775.2, 775.3, 775.4, and 775.5, which appear in that order. A
user who wishes to see only jobs 775.2 and 775.5 can select only these
two jobs in the job selector tool and click the "Ok" or "Apply" button.
The job selector supports double clicking; double click on any single
job to see it drawn in isolation.

Zooming
-------

To view a small area of the log file, zoom in on the area which you
would like to see in greater detail. You can zoom in, out and do a full
zoom. A full zoom redraws the log file in its entirety. For example, if
you have zoomed in very close and would like to go all the way back out,
you could do so with a succession of zoom outs or with one full zoom.

There is a difference between using the menu driven zooming and the
mouse driven zooming. The menu driven zooming will recenter itself
around the current center, whereas mouse driven zooming will recenter
itself (as much as possible) around the mouse click. To help you re-find
the clicked area, a box will flash after the zoom. This is called the
"zoom finder" and it can be turned off in the zoom menu if you prefer.

Keyboard and Mouse Shortcuts
----------------------------

#. The Keyboard shortcuts:

   -  Arrows - an approximate ten percent scroll bar movement
   -  PageUp and PageDown - an approximate one hundred percent scroll
      bar movement
   -  Control + Left or Right - approximate one hundred percent scroll
      bar movement
   -  End and Home - scroll bar movement to the vertical extreme
   -  Others - as seen beside menu items

#. The mouse shortcuts:

   -  Control + Left click - zoom in
   -  Control + Right click - zoom out
   -  Shift + left click - re-center

      
