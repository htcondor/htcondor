# HTCONDOR Style guide

## Tone and voice

TBD (To be documented)

## Syntax and markup

The current manual, in Sphinx format, with some custom extensions, has been automatically converted from LaTeX sources,
and as such, there are some remaining inconsistencies in the formatting.  We are currently converting the manual to this
new consistent form as a long term background task, but please use this new syntax when adding new documentation and
cleaning up old.  In general, we are replacing all explicit formatting commands (like bold or italic) with semantically
meaningful commands, like **:subcom:** or **:macro:**

### Built in sphinx markups

Sphinx comes equipped with some useful pre-built roles (extentions) and directives usable in our documentation to
automatically html formating and stylizing. A full list of [roles](https://www.sphinx-doc.org/en/master/usage/restructuredtext/roles.html)
and [directives](https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html) can be found in the sphinx
documentation. Some notable ones are listed below:

- Code Block -- **.. code-block::\<language\>**
- Warning -- **.. warning::**
- Note -- **.. note::**
- Sidebar -- **.. sidebar::**
- Mermaid graph -- **.. mermaid::**

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
---
**:macro-def:** is used in the configuration macros reference page to define an anchor for the above to target,
and an index entry. If given an argument, the index pairing will be the name of the macro and
**[Arg] Configuration Options**. Any macros without an additional argument will just be paired with
**Configuration Options**.

Example: **:macro-def:\`START\`**

Example: **:macro-def:\`START[Startd]\`**
---
**:macro:** used to reference a condor config macro knob declared with **macro-def** as above. **macro** creates
a reference (link) from the existing position to the entry in the configuration macros page. If given an argument,
it will also create and index entry with a subheading under the macro name.

Example: **:macro:\`START\`**

Example: **:macro:\`START[with Preemption]\`**
---
**:subcom-def:** is used in the condor_submit manual page to define a condor submit command.  **:subcom-def:** 
takes one argument, the name of the submit command. It creates an index entry
and an anchor for the following command to use.  It should only be used in the condor submit manual page.

Example:  **:subcom-def:\`environment\`** 
---
**:subcom:** references a condor submit command that has been **subcom-def**'d as above.  **subcom** creates
a reference (link) from the existing position to the entry in the submit manual.  If given an argument, it will
also create an index entry with a subheading under the main one.

Example:  **:subcom:\`environment\`** -- makes a reference from here to the condor_submit man page

Example:  **:subcom:\`environment[container universe]\`** -- does the above plus add index entry under *environment*
---
**:tool:** is used to make a reference link to an HTCondor command line tools manual page in the
documentation. If given optional information in brackets, it will also create an index option
under the tools name.

Example: **:tool:\`condor_q\`** -- makes a reference from here to the condor\_q manual page.

Example: **:tool:\`condor_q[using Autoformatting]\`** -- does the above plus makes an index entry under condor\_q.

Example: **:tool:\`htcondor job status\`** -- will make a reference link as *htcondor job status* that links to the htcondor tool man page.
---
**:ad-attr:** references Classad attributes to the that have been defined by **:classad-attribute-def:**. **:ad-attr:**
creates a refence (link) from the existing position to the appropriate Classad Type documentation page. This extension
can be given addition details via *key=value* information separated by semi-colons in brackets.

Extra **:ad-attr:** detail keywords:
- **Index** -- Creates an index entry with the value name under the attribute
- **Type** -- Specifies file to link to based on explicit Classad type. See Option to file mapping below

|    **Type**    |               **File**               |
|:---------------|:------------------------------------:|
| ACCOUNTING     | accounting-classad-attributes        |
| ADDED_COLLECTOR| classad-attributes-added-by-collector|
| COLLECTOR      | collector-classad-attributes         |
| DAEMONCORE     | daemon-core-statistics-attributes    |
| DEFRAG         | defrag-classad-attributes            |
| GRID           | grid-classad-attributes              |
| JOB            | job-classad-attributes               |
| MACHINE        | machine-classad-attributes           |
| MASTER         | daemon-master-classad-attributes     |
| NEGOTIATOR     | negotiator-classad-attributes        |
| SCHEDD         | scheduler-classad-attributes         |
| SUBMITTER      | submitter-classad-attributes         |

Example: **:ad-attr:**\`DAGManJobId\` -- Makes a reference link for *Job Classad Attributes*

Example: **:ad-attr**\`DAGManJobId[index="within Node Job"]` -- does as above plus makes an index entry under *DAGManJobId*

Example: **:ad-attr:**\`Machine[type=Master]\` -- Makes reference for *Machine* attribute to correct documentation page (daemon-master-classad-attributes)

Example: **:ad-attr:**\`Machine[type=Master;index="on EPs"]\` -- does as above plus makes an index entry under *Machine*

