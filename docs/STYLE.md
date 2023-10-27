# HTCONDOR Style guide

## Tone and voice

TBD (To be documented)

## Syntax and markup

The current manual, in Sphinx format, with some custom extensions, has been automatically converted from LaTeX sources,
and as such, there are some remaining inconsistencies in the formatting.  We are currently converting the manual to this
new consistent form as a long term background task, but please use this new syntax when adding new documentation and
cleaning up old.  In general, we are replacing all explicit formatting commands (like bold or italic) with semantically
meaningful commands, like **:subcom:** or **:macro:**

### HTCondor specific markups

**:index:** used to add entries to the index.  Our users do rely on the index, so take care to make good entries.  It
is encouraged to have multiple meaningful index entries for the same topic. In some cases it may make sense
to have both an index and inverted index entry for a topic.  For example, we have a primary index entry
for every classad function, and also an inverted entry, with one top-level entry for "classad functions", with
a subentry for each function under that. The form of the command is

Example:  **:index:\`executable name\<single; executable name; and checkpointing\>\`** -- This 
makes a main index entry named `executable`, with
an indented link under it named `and checkpointing`.  Multiple
descriptions/usage for a single index entry make it easy for readers to find
related items that may be located in different places in the manual.

**:macro:** used to reference a condor config macro knob.  It takes one argument, the name of the macro to reference.

Example: **:macro:\`START\`**

**:macro-def:** is used in the coonfiguration macros reference page to define an anchor for the above to target,
and an index entry.

Example: **:macro-def:\`START\`**

**:subcom-def:** is used in the condor_submit manual page to define a condor submit command.  **:subcom-def:** 
takes one argument, the name of the submit command. It creates an index entry
and an anchor for the following command to use.  It should only be used in the condor submit manual page.

Example:  **:subcom-def:\`environment\`** 

**:subcom:** references a condor submit command that has been **subcom-def**'d as above.  **subcom** creates
a reference (link) from the existing position to the entry in the submit manual.  If given an argument, it will
also create an index entry with a subheading under the main one.

Example:  **:subcom:\`environment\`** -- makes a reference from here to the condor_submit man page

Example:  **:subcom:\`environment\<container universe\>\`** -- does the above plus add index entry under *environment*
