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

**:config-template:** is used in the configuration introduction page to define an anchor for a designated
configuration template, and to automatically create an index entry. This is used for both templated categories
and full templates. When specifying a full template (not a category), this role needs to specify the associated
category in **\<\>**. Defined templates and template categories can then be referenced by the **:macro:** role
(see below).

Example: **:config-template:\`POLICY\`** -- Defines anchor and indexes the POLICY category.

Example: **:config-template:\`Desktop\<POLICY\>\`** -- Defines anchor and indexes the Desktop policy configuration template.

Example: **:config-template:\`Monitor( Args )\<FEATURE\>\`** -- Defines anchor and indexes the Monitor feature
configuration template, but displays **Monitor( Args )** in documentation.

---

**:macro-def:** is used in the configuration macros reference page to define an anchor for the above to target,
and an index entry. If given an argument, the index pairing will be the name of the macro and
**[Arg] Configuration Options**. Any macros without an additional argument will just be paired with
**Configuration Options**.

Example: **:macro-def:\`START\`**

Example: **:macro-def:\`START[Startd]\`**

If the **:macro-def:** is being added to a file other than one listed below, be sure to update **macro.py**
extension to know where to find the newly added macro definition (i.e. add the file path).

|          **Macro Definition Files**         |
|:-------------------------------------------:|
|    admin-manual/configuration-macros        |
|    cloud-computing/annex-configuration      |

---

**:macro:** used to reference a condor config macro knob declared with **macro-def** as above. **macro** creates
a reference (link) from the existing position to the entry in the configuration macros page. If given an argument,
it will also create and index entry with a subheading under the macro name.

Example: **:macro:\`START\`**

Example: **:macro:\`START[with Preemption]\`**

The **:macro:** role can also be used to reference configuration templates and template categories when provided
**use Category:Template** or **use Category**.

Example: **:macro:\`use POLICY\`** -- Creates reference to the configuration template POLICY category.

Example: **:macro:\`use Feature:Monitor\`** -- Creates a reference to the Monitor feature configuration template.

Example: **:macro:\`use ROLE:Personal[setting up a mini-condor]\`** -- Creates a reference to the personal role
configuration template and creates and index entry labeled **setting up a mini-condor**.

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

**:ad-attr:** references ClassAd attributes to the that have been defined by **:classad-attribute-def:**. **:ad-attr:**
creates a reference (link) from the existing position to the appropriate ClassAd Type documentation page. This extension
can be given addition details via *key=value* information separated by semi-colons in brackets.

Extra **:ad-attr:** detail keywords:
- **Index** -- Creates an index entry with the value name under the attribute
- **Type** -- Specifies file to link to based on explicit ClassAd type. See Option to file mapping below

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

---

**:dag-cmd-def:** is used in the DAGMan Quick Reference section to automatically create a page anchor and
index entry for the provided DAG command. All DAG commands defined this way will be indexed under the
**DAG Commands** section.

Example: **:dag-cmd-def:\`JOB\`** -- Defines anchor and index for the **JOB** command.

---

**:dag-cmd:** is used inline to create a reference link to the specified DAG commands definition. If provided extra
information via **[Text]** then this role will automatically produce an index entry with the provided text.

Example: **:dag-cmd:\`SUBDAG\`** -- Creates a reference to the **SUBDAG** command definition.

Example: **:dag-cmd:\`SUBDAG[DAG-ception]\`** -- Does as above while also creating the index entry **DAG-ception**.

---

**:classad-function-def:** is used in the ClassAd Builtin Functions section to automatically create a page anchor and
index entry for ClassAd Functions. All ClassAd Functions defined this way will have an index entry (**FunctionName()**)
produced under **ClassAd Functions**. **Note:** All defined functions expect a **ReturnType** and parenthesis' **()**.
Arguments are optional and will not break the extension.

Example: **:classad-function-def:\`Boolean foo()\`** -- Creates an anchor and index for **foo()**

Example: **:classad-function-def:\`String listMembers(list [, filter])\`** -- Creates an anchor and index for **listMembers()**

---

**:classad-function:** is used inline to create a reference link to the specified ClassAd Function definition. If
provided extra information via **[Text]** then this role will automatically produce an index entry with the provided
text. **Note:** The function name may or may not contain parenthesis' **()**.

Example: **:classad-function:\`foo\`** -- Creates a reference to the **foo()** function definition.

Example: **:classad-function:\`foo()\`** -- Creates a reference to the **foo()** function definition.

Example: **:classad-function:\`foo()[Custom Index]\`** -- Does as above while also creating the index entry **Custom Index**.

---

**.. multi-tab::** is a custom directive that allows multiple **.. code-block::** directives to be
used in single space with selection tabs above (looks like a single code block example).

The selection tabs are named **Option N** where **N** is the example number from left to right. This
default tab name can be overridden by setting \`**:class: tab:{Name}**\` where **{Name}** is hyphen delimited
sentence (not spaces) that will be turned into a title case sentence. i.e. \`**:class: tab:my-custom-tab-name**\`
becomes **[My Custom Tab Name]** in the tab.

Note: When building for Man Pages all code blocks will be written in order top to bottom.

For live example of displayed output see [LVM setup section](https://htcondor.readthedocs.io/en/latest/admin-manual/ep-policy-configuration.html#per-job-ephemeral-scratch-filesystems)

Example: Code below will produce a code block with two option tabs **\`[Option 1] [Custom Name]\`**

1. **Option 1**: Displays the JSON code block (displayed by default).
2. **Custom Name**: Displays the HTCondor ClassAd code block.

```
.. multi-tab::
    .. code-block:: json
        {
            "JSON" : "Example",
        }

    .. code-block:: condor-classad
        :class: tab:custom-name
        ClassAd = "Example"
```
