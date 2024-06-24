HTCondor's ClassAd Mechanism
============================

:index:`ClassAd`

ClassAds are a flexible mechanism for representing the characteristics
and constraints of machines and jobs in the HTCondor system. ClassAds
are used extensively in the HTCondor system to represent jobs,
resources, submitters and other HTCondor daemons. An understanding of
this mechanism is required to harness the full flexibility of the
HTCondor system.

A ClassAd is a set of uniquely named expressions. Each named
expression is called an attribute. The following shows
ten attributes, a portion of an example ClassAd.

.. code-block:: condor-classad

    MyType       = "Machine"
    TargetType   = "Job"
    Machine      = "froth.cs.wisc.edu"
    Arch         = "INTEL"
    OpSys        = "LINUX"
    Disk         = 35882
    Memory       = 128
    KeyboardIdle = 173
    LoadAvg      = 0.1000
    Requirements = TARGET.Owner=="smith" || LoadAvg<=0.3 && KeyboardIdle>15*60

ClassAd expressions look very much like expressions in C, and are
composed of literals and attribute references composed with operators
and functions. The difference between ClassAd expressions and C
expressions arise from the fact that ClassAd expressions operate in a
much more dynamic environment. For example, an expression from a
machine's ClassAd may refer to an attribute in a job's ClassAd, such as
TARGET.Owner in the above example. The value and type of the attribute
is not known until the expression is evaluated in an environment which
pairs a specific job ClassAd with the machine ClassAd.

ClassAd expressions handle these uncertainties by defining all operators
to be total operators, which means that they have well defined behavior
regardless of supplied operands. This functionality is provided through
two distinguished values, ``UNDEFINED`` and ``ERROR``, and defining all
operators so that they can operate on all possible values in the ClassAd
system. For example, the multiplication operator which usually only
operates on numbers, has a well defined behavior if supplied with values
which are not meaningful to multiply. Thus, the expression
10 \* "A string" evaluates to the value ``ERROR``. Most operators are
strict with respect to ``ERROR``, which means that they evaluate to
``ERROR`` if any of their operands are ``ERROR``. Similarly, most
operators are strict with respect to ``UNDEFINED``.

ClassAds: Old and New
---------------------

ClassAds have existed for quite some time in two forms: Old and New. Old
ClassAds were the original form and were used in HTCondor until HTCondor
version 7.5.0. They were heavily tied to the HTCondor development
libraries. New ClassAds added new features and were designed as a
stand-alone library that could be used apart from HTCondor.

In HTCondor version 7.5.1, HTCondor switched to using the New ClassAd
library for all use of ClassAds within HTCondor. The library is placed
into a compatibility mode so that HTCondor 7.5.1 is still able to
exchange ClassAds with older versions of HTCondor.

All user interaction with tools (such as :tool:`condor_q`) as well as output
of tools is still compatible with Old ClassAds. Before HTCondor version
7.5.1, New ClassAds were used only in the Job Router. There are some
syntax and behavior differences between Old and New ClassAds, all of
which should remain invisible to users of HTCondor.

A complete description of New ClassAds can be found at
`http://htcondor.org/classad/classad.html <http://htcondor.org/classad/classad.html>`_,
and in the ClassAd Language Reference Manual found on that web page.

Some of the features of New ClassAds that are not in Old ClassAds are
lists, nested ClassAds, time values, and matching groups of ClassAds.
HTCondor has avoided using these features, as using them makes it
difficult to interact with older versions of HTCondor. But, users can
start using them if they do not need to interact with versions of
HTCondor older than 7.5.1.

The syntax varies slightly between Old and New ClassAds. Here is an
example ClassAd presented in both forms. The Old form:

.. code-block:: condor-classad

    Foo = 3
    Bar = "ab\"cd\ef"
    Moo = Foo =!= Undefined

The New form:

.. code-block:: condor-classad

    [
    Foo = 3;
    Bar = "ab\"cd\\ef";
    Moo = Foo isnt Undefined;
    ]

HTCondor will convert to and from Old ClassAd syntax as needed.

New ClassAd Attribute References
''''''''''''''''''''''''''''''''

Expressions often refer to ClassAd attributes. These attribute
references work differently in Old ClassAds as compared with New
ClassAds. In New ClassAds, an unscoped reference is looked for only in
the local ClassAd. An unscoped reference is an attribute that does not
have a ``MY.`` or ``TARGET.`` prefix. The local ClassAd may be described
by an example. Matchmaking uses two ClassAds: the job ClassAd and the
machine ClassAd. The job ClassAd is evaluated to see if it is a match
for the machine ClassAd. The job ClassAd is the local ClassAd.
Therefore, in the ``Requirements`` attribute of the job ClassAd, any
attribute without the prefix ``TARGET.`` is looked up only in the job
ClassAd. With New ClassAd evaluation, the use of the prefix ``MY.`` is
eliminated, as an unscoped reference can only refer to the local
ClassAd.

The ``MY.`` and ``TARGET.`` scoping prefixes only apply when evaluating
an expression within the context of two ClassAds. Two examples that
exemplify this are matchmaking and machine policy evaluation. When
evaluating an expression within the context of a single ClassAd, ``MY.``
and ``TARGET.`` are not defined. Using them within the context of a
single ClassAd will result in a value of ``Undefined``. Two examples
that exemplify evaluating an expression within the context of a single
ClassAd are during user job policy evaluation, and with the
**-constraint** option to command-line tools.

New ClassAds have no :ad-attr:`CurrentTime` attribute. If needed, use the
time() function instead. In order to mimic Old ClassAd semantics in
current versions of HTCondor, all ClassAds have an implicit
:ad-attr:`CurrentTime` attribute, with a value of time().

In current versions of HTCondor, New ClassAds will mimic the evaluation
behavior of Old ClassAds. No configuration variables or submit
description file contents should need to be changed. To eliminate this
behavior and use only the semantics of New ClassAds, set the
configuration variable :macro:`STRICT_CLASSAD_EVALUATION` to ``True``. This permits
testing expressions to see if any adjustment is required, before a
future version of HTCondor potentially makes New ClassAds evaluation
behavior the default or the only option.

ClassAd Syntax
--------------

:index:`expression syntax of Old ClassAds<single: expression syntax of Old ClassAds; ClassAd>`

ClassAd expressions are formed by composing literals, attribute
references and other sub-expressions with operators and functions.

Composing Literals
''''''''''''''''''

Literals in the ClassAd language may be of integer, real, string,
undefined or error types. The syntax of these literals is as follows:

 Integer
    A sequence of continuous digits (i.e., [0-9]). Additionally, the
    keywords TRUE and FALSE (case insensitive) are syntactic
    representations of the integers 1 and 0 respectively.
 Real
    Two sequences of continuous digits separated by a period (i.e.,
    [0-9]+.[0-9]+).
 String
    A double quote character, followed by a list of characters
    terminated by a double quote character. A backslash character inside
    the string causes the following character to be considered as part
    of the string, irrespective of what that character is.
 Undefined
    The keyword ``UNDEFINED`` (case insensitive) represents the
    ``UNDEFINED`` value.
 Error
    The keyword ``ERROR`` (case insensitive) represents the ``ERROR``
    value.

Attributes
''''''''''

:index:`attributes<single: attributes; ClassAd>`

Every expression in a ClassAd is named by an attribute name. Together,
the (name,expression) pair is called an attribute. An attribute may be
referred to in other expressions through its attribute name.

Attribute names are sequences of alphabetic characters, digits and
underscores, and may not begin with a digit. All characters in the name
are significant, but case is not significant. Thus, Memory, memory and
MeMoRy all refer to the same attribute.

An attribute reference consists of the name of the attribute being
referenced, and an optional scope resolution prefix. The prefixes that
may be used are ``MY.`` and ``TARGET.``. The case used for these
prefixes is not significant. The semantics of supplying a prefix are
discussed in :ref:`classads/classad-mechanism:classad evaluation
semantics`.

Expression Operators
'''''''''''''''''''''

:index:`expression operators<single: expression operators; ClassAd>`

The operators that may be used in ClassAd expressions are similar to
those available in C. The available operators and their relative
precedence is shown in the following example:

.. code-block:: text

      - (unary negation)   (high precedence)
      *  /   %
      +   - (addition, subtraction)
      <   <=   >=   >
      ==  !=  =?=  is  =!=  isnt
      &&
      ||                   (low precedence)

The operator with the highest precedence is the unary minus operator.
The only operators which are unfamiliar are the =?=, is, =!= and isnt
operators, which are discussed in
:ref:`classads/classad-mechanism:classad evaluation semantics`.

ClassAd Evaluation Semantics
--------------------------------

The ClassAd mechanism's primary purpose is for matching entities that
supply constraints on candidate matches. The mechanism is therefore
defined to carry out expression evaluations in the context of two
ClassAds that are testing each other for a potential match. For example,
the *condor_negotiator* evaluates the ``Requirements`` expressions of
machine and job ClassAds to test if they can be matched. The semantics
of evaluating such constraints is defined below.

Evaluating Literals
'''''''''''''''''''

Literals are self-evaluating, Thus, integer, string, real, undefined and
error values evaluate to themselves.

Attribute References
''''''''''''''''''''

:index:`scope of evaluation, MY.<single: scope of evaluation, MY.; ClassAd>`
:index:`scope of evaluation, TARGET.<single: scope of evaluation, TARGET.; ClassAd>`
:index:`TARGET., ClassAd scope resolution prefix`
:index:`MY., ClassAd scope resolution prefix`

Since the expression evaluation is being carried out in the context of
two ClassAds, there is a potential for name space ambiguities. The
following rules define the semantics of attribute references made by
ClassAd A that is being evaluated in a context with another ClassAd B:

#. If the reference is prefixed by a scope resolution prefix,

   -  If the prefix is ``MY.``, the attribute is looked up in ClassAd A.
      If the named attribute does not exist in A, the value of the
      reference is ``UNDEFINED``. Otherwise, the value of the reference
      is the value of the expression bound to the attribute name.
   -  Similarly, if the prefix is ``TARGET.``, the attribute is looked
      up in ClassAd B. If the named attribute does not exist in B, the
      value of the reference is ``UNDEFINED``. Otherwise, the value of
      the reference is the value of the expression bound to the
      attribute name.

#. If the reference is not prefixed by a scope resolution prefix,

   -  If the attribute is defined in A, the value of the reference is
      the value of the expression bound to the attribute name in A.
   -  Otherwise, if the attribute is defined in B, the value of the
      reference is the value of the expression bound to the attribute
      name in B.
   -  Otherwise, if the attribute is defined in the ClassAd environment,
      the value from the environment is returned. This is a special
      environment, to be distinguished from the Unix environment.
      Currently, the only attribute of the environment is
      :ad-attr:`CurrentTime`, which evaluates to the integer value returned by
      the system call ``time(2)``.
   -  Otherwise, the value of the reference is ``UNDEFINED``.

#. Finally, if the reference refers to an expression that is itself in
   the process of being evaluated, there is a circular dependency in the
   evaluation. The value of the reference is ``ERROR``.

ClassAd Operators
'''''''''''''''''''''

:index:`expression operators<single: expression operators; ClassAd>`

All operators in the ClassAd language are total, and thus have well
defined behavior regardless of the supplied operands. Furthermore, most
operators are strict with respect to ``ERROR`` and ``UNDEFINED``, and
thus evaluate to ``ERROR`` or ``UNDEFINED`` if either of their operands
have these exceptional values.

-  **Arithmetic operators:**

   #. The operators ``\*``, ``/``, ``+`` and ``-`` operate arithmetically only on
      integers and reals.
   #. Arithmetic is carried out in the same type as both operands, and
      type promotions from integers to reals are performed if one
      operand is an integer and the other real.
   #. The operators are strict with respect to both ``UNDEFINED`` and
      ``ERROR``.
   #. If either operand is not a numerical type, the value of the
      operation is ``ERROR``.

-  **Comparison operators:**

   #. The comparison operators ``==``, ``!=``, ``<=``, ``<``, ``>=`` and ``>`` operate on
      integers, reals and strings.
   #. String comparisons are case insensitive for most operators. The
      only exceptions are the operators ``=?=`` and ``=!=``, which do case
      sensitive comparisons assuming both sides are strings.
   #. Comparisons are carried out in the same type as both operands, and
      type promotions from integers to reals are performed if one
      operand is a real, and the other an integer. Strings may not be
      converted to any other type, so comparing a string and an integer
      or a string and a real results in ``ERROR``.
   #. The operators ``==``, ``!=``, ``<=``, ``<``, ``>=``, and ``>`` are strict with respect to
      both ``UNDEFINED`` and ``ERROR``.
   #. In addition, the operators ``=?=``, ``is``, ``=!=``, and ``isnt`` behave similar to
      ``==`` and !=, but are not strict. Semantically, the ``=?=`` and is test
      if their operands are "identical," i.e., have the same type and
      the same value. For example, ``10 == UNDEFINED`` and
      ``UNDEFINED == UNDEFINED`` both evaluate to ``UNDEFINED``, but
      ``10 =?= UNDEFINED`` and ``UNDEFINED`` is ``UNDEFINED`` evaluate to ``FALSE``
      and ``TRUE`` respectively. The ``=!=`` and ``isnt`` operators test for the
      "is not identical to" condition.

      ``=?=`` and ``is`` have the same behavior as each other. And ``isnt`` and ``=!=``
      behave the same as each other. The ClassAd unparser will always
      use ``=?=`` in preference to ``is`` and ``=!=`` in preference to ``isnt`` when
      printing out ClassAds.

-  **Logical operators:**

   #. The logical operators ``&&`` and ``||`` operate on integers and reals.
      The zero value of these types are considered ``FALSE`` and
      non-zero values ``TRUE``.
   #. The operators are not strict, and exploit the "don't care"
      properties of the operators to squash ``UNDEFINED`` and ``ERROR``
      values when possible. For example, UNDEFINED && FALSE evaluates to
      ``FALSE``, but ``UNDEFINED || FALSE`` evaluates to ``UNDEFINED``.
   #. Any string operand is equivalent to an ``ERROR`` operand for a
      logical operator. In other words, ``TRUE && "foobar"`` evaluates to
      ``ERROR``.

-  **The Ternary operator:**

   #. The Ternary operator (``expr1 ? expr2 : expr3``) operate
      with expressions. If all three expressions are given, the
      operation is strict.
   #. However, if the middle expression is missing, eg. ``expr1 ?:
      expr3``, then, when expr1 is defined, that defined value is
      returned. Otherwise, when expr1 evaluated to ``UNDEFINED``, the
      value of expr3 is evaluated and returned. This can be a convenient
      shortcut for writing what would otherwise be a much longer classad
      expression.

Expression Examples
'''''''''''''''''''

:index:`expression examples<single: expression examples; ClassAd>`

The ``=?=`` operator is similar to the ``==`` operator. It checks if the
left hand side operand is identical in both type and value to the
right hand side operand, returning ``TRUE`` when they are identical.

.. caution::

    For strings, the comparison is case-insensitive with the ``==`` operator and
    case-sensitive with the ``=?=`` operator. A key point in understanding is that
    the ``=?=`` operator only produces evaluation results of ``TRUE`` and
    ``FALSE``, where the ``==`` operator may produce evaluation results ``TRUE``,
    ``FALSE``, ``UNDEFINED``, or ``ERROR``.

Table 4.1 presents examples that define the outcome of the ``==`` operator.
Table 4.2 presents examples that define the outcome of the ``=?=`` operator.

+-------------------------------+---------------------------+
| **expression**                | **evaluated result**      |
+===============================+===========================+
| ``(10 == 10)``                | ``TRUE``                  |
+-------------------------------+---------------------------+
| ``(10 == 5)``                 | ``FALSE``                 |
+-------------------------------+---------------------------+
| ``(10 == "ABC")``             | ``ERROR``                 |
+-------------------------------+---------------------------+
| ``"ABC" == "abc"``            | ``TRUE``                  |
+-------------------------------+---------------------------+
| ``(10 == UNDEFINED)``         | ``UNDEFINED``             |
+-------------------------------+---------------------------+
| ``(UNDEFINED == UNDEFINED)``  | ``UNDEFINED``             |
+-------------------------------+---------------------------+

Table 4.1: Evaluation examples for the ``==`` operator


+-------------------------------+----------------------+
| **expression**                | **evaluated result** |
+===============================+======================+
| ``(10 =?= 10)``               | ``TRUE``             |
+-------------------------------+----------------------+
| ``(10 =?= 5)``                | ``FALSE``            |
+-------------------------------+----------------------+
| ``(10 =?= "ABC")``            | ``FALSE``            |
+-------------------------------+----------------------+
| ``"ABC" =?= "abc"``           | ``FALSE``            |
+-------------------------------+----------------------+
| ``(10 =?= UNDEFINED)``        | ``FALSE``            |
+-------------------------------+----------------------+
| ``(UNDEFINED =?= UNDEFINED)`` | ``TRUE``             |
+-------------------------------+----------------------+

Table 4.2: Evaluation examples for the ``=?=`` operator


The ``=!=`` operator is similar to the ``!=`` operator. It checks if the
left hand side operand is not identical in both type and value to the
the right hand side operand, returning ``FALSE`` when they are
identical.

.. caution::

    For strings, the comparison is case-insensitive with the !=
    operator and case-sensitive with the =!= operator. A key point in
    understanding is that the ``=!=`` operator only produces evaluation results
    of ``TRUE`` and ``FALSE``, where the ``!=`` operator may produce evaluation
    results ``TRUE``, ``FALSE``, ``UNDEFINED``, or ``ERROR``.

Table 4.3 presents examples that define the outcome of the ``!=`` operator.
Table 4.4 presents examples that define the outcome of the ``=!=`` operator.

+-------------------------------+----------------------------+
| **expression**                | **evaluated result**       |
+===============================+============================+
| ``(10 != 10)``                | ``FALSE``                  |
+-------------------------------+----------------------------+
| ``(10 != 5)``                 | ``TRUE``                   |
+-------------------------------+----------------------------+
| ``(10 != "ABC")``             | ``ERROR``                  |
+-------------------------------+----------------------------+
| ``"ABC" != "abc"``            | ``FALSE``                  |
+-------------------------------+----------------------------+
| ``(10 != UNDEFINED)``         | ``UNDEFINED``              |
+-------------------------------+----------------------------+
| ``(UNDEFINED != UNDEFINED)``  | ``UNDEFINED``              |
+-------------------------------+----------------------------+

Table 4.3: Evaluation examples for the ``!=`` operator


+-------------------------------+-----------------------+
| **expression**                | **evaluated result**  |
+===============================+=======================+
| ``(10 =!= 10)``               | ``FALSE``             |
+-------------------------------+-----------------------+
| ``(10 =!= 5)``                | ``TRUE``              |
+-------------------------------+-----------------------+
| ``(10 =!= "ABC")``            | ``TRUE``              |
+-------------------------------+-----------------------+
| ``"ABC" =!= "abc"``           | ``TRUE``              |
+-------------------------------+-----------------------+
| ``(10 =!= UNDEFINED)``        | ``TRUE``              |
+-------------------------------+-----------------------+
| ``(UNDEFINED =!= UNDEFINED)`` | ``FALSE``             |
+-------------------------------+-----------------------+

Table 4.4: Evaluation examples for the ``=!=`` operator


Old ClassAds in the HTCondor System
-----------------------------------

The simplicity and flexibility of ClassAds is heavily exploited in the
HTCondor system. ClassAds are not only used to represent machines and
jobs in the HTCondor pool, but also other entities that exist in the
pool such as submitters of jobs and master daemons.
Since arbitrary expressions may be supplied and evaluated over these
ClassAds, users have a uniform and powerful mechanism to specify
constraints over these ClassAds. These constraints can take the form of
``Requirements`` expressions in resource and job ClassAds, or queries
over other ClassAds.

Constraints and Preferences
'''''''''''''''''''''''''''

:index:`requirements<single: requirements; ClassAd attribute>`
:index:`rank<single: rank; ClassAd attribute>`

The ``requirements`` and ``rank`` expressions within the submit
description file are the mechanism by which users specify the
constraints and preferences of jobs. For machines, the configuration
determines both constraints and preferences of the machines.
:index:`examples<single: examples; rank attribute>`
:index:`requirements attribute`

For both machine and job, the ``rank`` expression specifies the
desirability of the match (where higher numbers mean better matches).
For example, a job ClassAd may contain the following expressions:

.. code-block:: condor-classad

    Requirements = (Arch == "INTEL") && (OpSys == "LINUX")
    Rank         = TARGET.Memory + TARGET.Mips

In this case, the job requires a 32-bit Intel processor running a Linux
operating system. Among all such computers, the customer prefers those
with large physical memories and high MIPS ratings. Since the ``Rank``
is a user-specified metric, any expression may be used to specify the
perceived desirability of the match. The *condor_negotiator* daemon
runs algorithms to deliver the best resource (as defined by the ``rank``
expression), while satisfying other required criteria.

Similarly, the machine may place constraints and preferences on the jobs
that it will run by setting the machine's configuration. For example,

.. code-block:: condor-classad

        Friend        = Owner == "tannenba" || Owner == "wright"
        ResearchGroup = Owner == "jbasney" || Owner == "raman"
        Trusted       = Owner != "rival" && Owner != "riffraff"
        START         = Trusted && ( ResearchGroup || LoadAvg < 0.3 && KeyboardIdle > 15*60 )
        RANK          = Friend + ResearchGroup*10

The above policy states that the computer will never run jobs owned by
users rival and riffraff, while the computer will always run a job
submitted by members of the research group. Furthermore, jobs submitted
by friends are preferred to other foreign jobs, and jobs submitted by
the research group are preferred to jobs submitted by friends.

**Note:** Because of the dynamic nature of ClassAd expressions, there is
no a priori notion of an integer-valued expression, a real-valued
expression, etc. However, it is intuitive to think of the
``Requirements`` and ``Rank`` expressions as integer-valued and
real-valued expressions, respectively. If the actual type of the
expression is not of the expected type, the value is assumed to be zero.

Querying with ClassAd Expressions
'''''''''''''''''''''''''''''''''

The flexibility of this system may also be used when querying ClassAds
through the :tool:`condor_status` and :tool:`condor_q` tools which allow users to
supply ClassAd constraint expressions from the command line.

Needed syntax is different on Unix and Windows platforms, due to the
interpretation of characters in forming command-line arguments. The
expression must be a single command-line argument, and the resulting
examples differ for the platforms. For Unix shells, single quote marks
are used to delimit a single argument. For a Windows command window,
double quote marks are used to delimit a single argument. Within the
argument, Unix escapes the double quote mark by prepending a backslash
to the double quote mark. Windows escapes the double quote mark by
prepending another double quote mark. There may not be spaces in
between.

Here are several examples. To find all computers which have had their
keyboards idle for more than 60 minutes and have more than 4000 MB of
memory, the desired ClassAd expression is

.. code-block:: condor-classad-expr

    KeyboardIdle > 60*60 && Memory > 4000

On a Unix platform, the command appears as

.. code-block:: console

    $ condor_status -const 'KeyboardIdle > 60*60 && Memory > 4000'

    Name               OpSys   Arch   State     Activity LoadAv Mem  ActvtyTime
    100
    slot1@altair.cs.wi LINUX   X86_64 Owner     Idle     0.000 8018 13+00:31:46
    slot2@altair.cs.wi LINUX   X86_64 Owner     Idle     0.000 8018 13+00:31:47
    ...
    ...
    slot1@athena.stat. LINUX   X86_64 Unclaimed Idle     0.000 7946  0+00:25:04
    slot2@athena.stat. LINUX   X86_64 Unclaimed Idle     0.000 7946  0+00:25:05
    ...
    ...

The Windows equivalent command is

.. code-block:: doscon

    > condor_status -const "KeyboardIdle > 60*60 && Memory > 4000"

Here is an example for a Unix platform that utilizes a regular
expression ClassAd function to list specific information. A file
contains ClassAd information. :tool:`condor_advertise` is used to inject this
information, and :tool:`condor_status` constrains the search with an
expression that contains a ClassAd function.

.. code-block:: console

    $ cat ad
    MyType = "Generic"
    FauxType = "DBMS"
    Name = "random-test"
    Machine = "f05.cs.wisc.edu"
    MyAddress = "<128.105.149.105:34000>"
    DaemonStartTime = 1153192799
    UpdateSequenceNumber = 1

    $ condor_advertise UPDATE_AD_GENERIC ad

    $ condor_status -any -constraint 'FauxType=="DBMS" && regexp("random.*", Name, "i")'

    MyType               TargetType           Name

    Generic              None                 random-test

The ClassAd expression describing a machine that advertises a Windows
operating system:

.. code-block:: condor-classad-expr

    OpSys == "WINDOWS"

Here are three equivalent ways on a Unix platform to list all machines
advertising a Windows operating system. Spaces appear in these examples
to show where they are permitted.

.. code-block:: console

    $ condor_status -constraint ' OpSys == "WINDOWS"  '

.. code-block:: console

    $ condor_status -constraint OpSys==\"WINDOWS\"

.. code-block:: console

    $ condor_status -constraint "OpSys==\"WINDOWS\""

The equivalent command on a Windows platform to list all machines
advertising a Windows operating system must delimit the single argument
with double quote marks, and then escape the needed double quote marks
that identify the string within the expression. Spaces appear in this
example where they are permitted.

.. code-block:: doscon

    > condor_status -constraint " OpSys == ""WINDOWS"" "

Extending ClassAds with User-written Functions
----------------------------------------------

The ClassAd language provides a rich set of functions. It is possible to
add new functions to the ClassAd language without recompiling the
HTCondor system or the ClassAd library. This requires implementing the
new function in the C++ programming language, compiling the code into a
shared library, and telling HTCondor where in the file system the shared
library lives.

While the details of the ClassAd implementation are beyond the scope of
this document, the ClassAd source distribution ships with an example
source file that extends ClassAds by adding two new functions, named
todays_date() and double(). This can be used as a model for users to
implement their own functions. To deploy this example extension, follow
the following steps on Linux:

-  Download the ClassAd source distribution from
   `http://www.cs.wisc.edu/condor/classad <http://www.cs.wisc.edu/condor/classad>`_.
-  Unpack the tarball.
-  Inspect the source file ``shared.cpp``. This one file contains the
   whole extension.
-  Build ``shared.cpp`` into a shared library. On Linux, the command
   line to do so is

   .. code-block:: console

       $ g++ -DWANT_CLASSAD_NAMESPACE -I. -shared -o shared.so \
         -Wl,-soname,shared.so -o shared.so -fPIC shared.cpp

-  Copy the file ``shared.so`` to a location that all of the HTCondor
   tools and daemons can read.

   .. code-block:: console

       $ cp shared.so `condor_config_val LIBEXEC`

-  Tell HTCondor to load the shared library into all tools and daemons,
   by setting the :macro:`CLASSAD_USER_LIBS` configuration variable to
   the full name of the shared library. In this case,

   .. code-block:: condor-config

       CLASSAD_USER_LIBS = $(LIBEXEC)/shared.so

-  Restart HTCondor.
-  Test the new functions by running

   .. code-block:: console

       $ condor_status -format "%s\n" todays_date()

:index:`ClassAd`


