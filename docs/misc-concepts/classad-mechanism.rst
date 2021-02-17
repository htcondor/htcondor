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

All user interaction with tools (such as *condor_q*) as well as output
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

New ClassAds have no ``CurrentTime`` attribute. If needed, use the
time() function instead. In order to mimic Old ClassAd semantics in
current versions of HTCondor, all ClassAds have an implicit
``CurrentTime`` attribute, with a value of time().

In current versions of HTCondor, New ClassAds will mimic the evaluation
behavior of Old ClassAds. No configuration variables or submit
description file contents should need to be changed. To eliminate this
behavior and use only the semantics of New ClassAds, set the
configuration variable ``STRICT_CLASSAD_EVALUATION``
:index:`STRICT_CLASSAD_EVALUATION` to ``True``. This permits
testing expressions to see if any adjustment is required, before a
future version of HTCondor potentially makes New ClassAds evaluation
behavior the default or the only option.

.. _ClassAd Syntax:

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
    A double quote character, followed by an list of characters
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
discussed in :ref:`misc-concepts/classad-mechanism:classad evaluation
semantics`.

Expression Operators
'''''''''''''''''''''

:index:`expression operators<single: expression operators; ClassAd>`

The operators that may be used in ClassAd expressions are similar to
those available in C. The available operators and their relative
precedence is shown in the following example:

.. code-block:: text

      - (unary negation)   (high precedence)
      *   /
      +   - (addition, subtraction)
      <   <=   >=   >
      ==  !=  =?=  is  =!=  isnt
      &&
      ||                   (low precedence)

The operator with the highest precedence is the unary minus operator.
The only operators which are unfamiliar are the =?=, is, =!= and isnt
operators, which are discussed in
:ref:`misc-concepts/classad-mechanism:classad evaluation semantics`.

Predefined Functions
''''''''''''''''''''

:index:`expression functions<single: expression functions; ClassAd>`
:index:`ClassAd functions`

Any ClassAd expression may utilize predefined functions. Function names
are case insensitive. Parameters to functions and a return value from a
function may be typed (as given) or not. Nested or recursive function
calls are allowed.

Here are descriptions of each of these predefined functions. The
possible types are the same as itemized in
:ref:`misc-concepts/classad-mechanism:classad syntax`. Where the type may
be any of these literal types, it is called out as AnyType. Where the type is
Integer, but only returns the value 1 or 0 (implying ``True`` or
``False``), it is called out as Boolean. The format of each function is
given as

.. code-block:: text

    ReturnType FunctionName(ParameterType parameter1, ParameterType parameter2, ...)

Optional parameters are given within square brackets.
:index:`eval()<single: eval(); ClassAd functions>`

``AnyType eval(AnyType Expr)``
    Evaluates ``Expr`` as a string and then returns the result of
    evaluating the contents of the string as a ClassAd expression. This
    is useful when referring to an attribute such as ``slotX_State``
    where ``X``, the desired slot number is an expression, such as
    ``SlotID+10``. In such a case, if attribute ``SlotID`` is 5, the
    value of the attribute ``slot15_State`` can be referenced using the
    expression ``eval(strcat("slot", SlotID+10,"_State"))``. Function
    strcat() calls function string() on the second parameter, which
    evaluates the expression, and then converts the integer result 15 to
    the string ``"15"``. The concatenated string returned by strcat() is
    ``"slot15_State"``, and this string is then evaluated.

    Note that referring to attributes of a job from within the string
    passed to eval() in the ``Requirements`` or ``Rank`` expressions
    could cause inaccuracies in HTCondor's automatic auto-clustering of
    jobs into equivalent groups for matchmaking purposes. This is
    because HTCondor needs to determine which ClassAd attributes are
    significant for matchmaking purposes, and indirect references from
    within the string passed to eval() will not be counted.
    :index:`unparse()<single: unparse(); ClassAd functions>`

``String unparse(Attribute attr)``
    This function looks up the value of the provided attribute and
    returns the unparsed version as a string. The attribute's value is
    not evaluated. If the attribute's value is ``x + 3``, then the
    function would return the string ``"x + 3"``. If the provided
    attribute cannot be found, an empty string is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given or the argument is not an attribute reference.

``String unresolved(Attribute attr)``
    This function returns the external attribute references and unresolved
    attribute references of the expression that is the value of the provided attribute.
    If the provided attribute cannot be found, then ``undefined`` is returned.

    For example, in a typical job ClassAd if the ``Requirements`` expression has the value
    ``OpSys == "LINUX" && TARGET.Arch == "ARM" && Cpus >= RequestCpus``, then 
    ``unresolved(Requirements)`` will return ``"Arch,Cpus,OpSys"`` because those will not
    be attributes of the job ClassAd.

``Boolean unresolved(Attribute attr, String pattern)``
    This function returns ``True`` when at least one of the external or unresolved attribute
    references of the expression that is the value of the provided attribute matches the
    given Perl regular expression pattern.  If none of the references match the pattern, then
    ``False`` is returned. If the provided attribute cannot be found, then ``undefined`` is returned.

    For example, in a typical job ClassAd if the ``Requirements`` expression has the value
    ``OpSys == "LINUX" && Arch == "ARM"``, then ``unresolved(Requirements, "^OpSys")`` will
    return ``True``, and ``unresolved(Requirements, "OpSys.+")`` will return ``False``.

    The intended use of this function is to make it easier to apply a submit transform to
    a job only when the job does not already reference a certain attribute. For instance

..  code-block:: text

    JOB_TRANSFORM_DefPlatform @=end
       # Apply this transform only when the job requirements does not reference OpSysAndver or OpSysName
       REQUIREMENTS ! unresolved(Requirements, "OpSys.+")
       # Add a clause to the job requirements to match only CentOs7 machines
       SET Requirements $(Requirements) && OpSysAndVer == "CentOS7"
    @end

:index:`ifThenElse()<single: ifThenElse(); ClassAd functions>`


``AnyType ifThenElse(AnyType IfExpr,AnyType ThenExpr, AnyType ElseExpr)``
    A conditional expression is described by ``IfExpr``. The following
    defines return values, when ``IfExpr`` evaluates to

    -  ``True``. Evaluate and return the value as given by ``ThenExpr``.
    -  ``False``. Evaluate and return the value as given by
       ``ElseExpr``.
    -  ``UNDEFINED``. Return the value ``UNDEFINED``.
    -  ``ERROR``. Return the value ``ERROR``.
    -  ``0.0``. Evaluate, and return the value as given by ``ElseExpr``.
    -  non-``0.0`` Real values. Evaluate, and return the value as given
       by ``ThenExpr``.

    Where ``IfExpr`` evaluates to give a value of type ``String``, the
    function returns the value ``ERROR``. The implementation uses lazy
    evaluation, so expressions are only evaluated as defined.

    This function returns ``ERROR`` if other than exactly 3 arguments
    are given. 
    :index:`isUndefined()<single: isUndefined(); ClassAd functions>`

``Boolean isUndefined(AnyType Expr)``
    Returns ``True``, if ``Expr`` evaluates to ``UNDEFINED``. Returns
    ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isError()<single: isError(); ClassAd functions>`

``Boolean isError(AnyType Expr)``
    Returns ``True``, if ``Expr`` evaluates to ``ERROR``. Returns
    ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isString()<single: isString(); ClassAd functions>`

``Boolean isString(AnyType Expr)``
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``String``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isInteger()<single: isInteger(); ClassAd functions>`

``Boolean isInteger(AnyType Expr)``
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``Integer``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isReal()<single: isReal(); ClassAd functions>`

``Boolean isReal(AnyType Expr)``
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``Real``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isList()<single: isList(); ClassAd functions>`

``Boolean isList(AnyType Expr)``
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``List``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isClassad()<single: isClassad(); ClassAd functions>`

``Boolean isClassAd(AnyType Expr)``
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``ClassAd``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isBoolean()<single: isBoolean(); ClassAd functions>`

``Boolean isBoolean(AnyType Expr)``
    Returns ``True``, if the evaluation of ``Expr`` gives the integer
    value 0 or 1. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isAbstime()<single: isAbstime(); ClassAd functions>`

``Boolean isAbstime(AnyType Expr)``
    Returns ``True``, if the evaluation of ``Expr`` returns an abstime
    type. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isRelTime()<single: isRelTime(); ClassAd functions>`

``Boolean isReltime(AnyType Expr)``
    Returns ``True``, if the evaluation of ``Expr`` returns an relative time
    type. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

    :index:`member()<single: member(); ClassAd functions>`

``Boolean member(AnyType m, ListType l)``
    Returns error if m does not evalute to a scalar, or l does not
    evaluate to a list.  Otherwise the elements of l are evaluted
    in order, and if an element is equal to m in the sense of ``==``
    the result of the function is ``True``.  Otherwise the function
    returns false.

    :index:`anyCompare()<single: anyCompare(); ClassAd functions>`

``Boolean anyCompare(string op, list l, AnyType t)``
    Returns error if op does not evalute to one of ``<``, ``<=``,
    ``==``, ``>``, ``>=``, ``!-``, ``is`` or ``isnt``. Returns error
    if l isn't a list, or t isn't a scalar
    Otherwise the elements of l are evaluted and compared to t
    using the corresponding operator defined by op. If any of
    the members of l evaluate to true, the result is ``True``.
    Otherwise the function returns ``False``.

    :index:`allCompare()<single: allCompare(); ClassAd functions>`

``Boolean allCompare(string op, list l, AnyType t)``
    Returns error if op does not evalute to one of ``<``, ``<=``,
    ``==``, ``>``, ``>=``, ``!-``, ``is`` or ``isnt``. Returns error
    if l isn't a list, or t isn't a scalar
    Otherwise the elements of l are evaluted and compared to t
    using the corresponding operator defined by op. If all of
    the members of l evaluate to true, the result is ``True``.
    Otherwise the function returns ``False``.

    :index:`IdenticalMember()<single: member(); ClassAd functions>`

``Boolean IdenticalMember(AnyType m, ListType l)``
    Returns error if m does not evalute to a scalar, or l does not
    evaluate to a list.  Otherwise the elements of l are evaluted
    in order, and if an element is equal to m in the sense of ``=?=``
    the result of the function is ``True``.  Otherwise the function
    returns false.

    :index:`int()<single: int(); ClassAd functions>`

``Integer int(AnyType Expr)``
    Returns the integer value as defined by ``Expr``. Where the type of
    the evaluated ``Expr`` is ``Real``, the value is truncated (round
    towards zero) to an integer. Where the type of the evaluated
    ``Expr`` is ``String``, the string is converted to an integer using
    a C-like atoi() function. When this result is not an integer,
    ``ERROR`` is returned. Where the evaluated ``Expr`` is ``ERROR`` or
    ``UNDEFINED``, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`real()<single: real(); ClassAd functions>`

``Real real(AnyType Expr)``
    Returns the real value as defined by ``Expr``. Where the type of the
    evaluated ``Expr`` is ``Integer``, the return value is the converted
    integer. Where the type of the evaluated ``Expr`` is ``String``, the
    string is converted to a real value using a C-like atof() function.
    When this result is not a real, ``ERROR`` is returned. Where the
    evaluated ``Expr`` is ``ERROR`` or ``UNDEFINED``, ``ERROR`` is
    returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`string()<single: string(); ClassAd functions>`

``String string(AnyType Expr)``
    Returns the string that results from the evaluation of ``Expr``.
    Converts a non-string value to a string. Where the evaluated
    ``Expr`` is ``ERROR`` or ``UNDEFINED``, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`bool()<single: bool(); ClassAd functions>`

``Bool bool(AnyType Expr)``
    Returns the boolean that results from the evaluation of ``Expr``.
    Converts a non-boolean value to a bool. A string expression
    that evaluates to the string "true" yields true, and "false" returns

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`absTime()<single: absTime(); ClassAd functions>`

``AbsTime absTime(AnyType t [, int z])``
    Creates an AbsTime value corresponding to time t an time-zone offset z. 
    If t is a String, then z must be omitted, and t is parsed as a specification as follows.

    The operand t is parsed as a specification of an instant in time (date and time). 
    This function accepts the canonical native representation of AbsTime values, but
    minor variations in format are allowed.
    The default format is yyyy-mm-ddThh:mm:sszzzzz where zzzzz is a time zone in the 
    format +hh:mm or -hh:mm

    If t and z are both omitted, the result is an AbsTime value representing the time and
    place where the function call is evaluated. Otherwise, t is converted to a Real by the function “real”,
    and treated as a number of seconds from the epoch, Midnight January 1, 1970 UTC. If z is
    specified, it is treated as a number of seconds east of Greenwich. Otherwise, the offset is calculated
    from t according to the local rules for the place where the function is evaluated.

    :index:`relTime()<single: relTime(); ClassAd functions>`

``RelTime relTime(AnyType t)``
     If the operand t is a String, it is parsed as a specification of a
     time interval. This function accepts the canonical native representation of RelTime values, but minor
     variations in format are allowed.

     Otherwise, t is converted to a Real by the function real, and treated as a number of seconds.
     The default string format is [-]days+hh:mm:ss.fff, where leading components and the fraction .fff
     are omitted if they are zero. In the default syntax, days is a sequence of digits starting with a non-zero
     digit, hh, mm, and ss are strings of exactly two digits (padded on the left with zeros if necessary) with
     values less than 24, 60, and 60, respectively and fff is a string of exactly three digits.

    :index:`floor()<single: floor(); ClassAd functions>`

``Integer floor(AnyType Expr)``
    Returns the integer that results from the evaluation of ``Expr``,
    where the type of the evaluated ``Expr`` is ``Integer``. Where the
    type of the evaluated ``Expr`` is not ``Integer``, function
    ``real(Expr)`` is called. Its return value is then used to return
    the largest magnitude integer that is not larger than the returned
    value. Where ``real(Expr)`` returns ``ERROR`` or ``UNDEFINED``,
    ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`ceiling()<single: ceiling(); ClassAd functions>`

``Integer ceiling(AnyType Expr)``
    Returns the integer that results from the evaluation of ``Expr``,
    where the type of the evaluated ``Expr`` is ``Integer``. Where the
    type of the evaluated ``Expr`` is not ``Integer``, function
    ``real(Expr)`` is called. Its return value is then used to return
    the smallest magnitude integer that is not less than the returned
    value. Where ``real(Expr)`` returns ``ERROR`` or ``UNDEFINED``,
    ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`pow()<single: pow(); ClassAd functions>`

``Integer pow(Integer base, Integer exponent)`` OR ``Real pow(Integer base, Integer exponent)`` OR ``Real pow(Real base, Real exponent)``
    Calculates ``base`` raised to the power of ``exponent``. If
    ``exponent`` is an integer value greater than or equal to 0, and
    ``base`` is an integer, then an integer value is returned. If
    ``exponent`` is an integer value less than 0, or if either ``base``
    or ``exponent`` is a real, then a real value is returned. An
    invocation with ``exponent=0`` or ``exponent=0.0``, for any value of
    ``base``, including 0 or 0.0, returns the value 1 or 1.0, type
    appropriate. 
    :index:`quantize()<single: quantize(); ClassAd functions>`

``Integer quantize(AnyType a, Integer b)`` OR ``Real quantize(AnyType a, Real b)`` OR ``AnyType quantize(AnyType a, AnyType list b)``
    ``quantize()`` computes the quotient of ``a/b``, in order to further
    compute ``  ceiling(quotient) * b``. This computes and returns an
    integral multiple of ``b`` that is at least as large as ``a``. So,
    when ``b >= a``, the return value will be ``b``. The return type is
    the same as that of ``b``, where ``b`` is an Integer or Real.

    When ``b`` is a list, ``quantize()`` returns the first value in the
    list that is greater than or equal to ``a``. When no value in the
    list is greater than or equal to ``a``, this computes and returns an
    integral multiple of the last member in the list that is at least as
    large as ``a``.

    This function returns ``ERROR`` if ``a`` or ``b``, or a member of
    the list that must be considered is not an Integer or Real.

    Here are examples:

    .. code-block:: text

             8     = quantize(3, 8)
             4     = quantize(3, 2)
             0     = quantize(0, 4)
             6.8   = quantize(1.5, 6.8)
             7.2   = quantize(6.8, 1.2)
             10.2  = quantize(10, 5.1)

             4     = quantize(0, {4})
             2     = quantize(2, {1, 2, "A"})
             3.0   = quantize(3, {1, 2, 0.5})
             3.0   = quantize(2.7, {1, 2, 0.5})
             ERROR = quantize(3, {1, 2, "A"})

    :index:`round()<single: round(); ClassAd functions>`

``Integer round(AnyType Expr)``
    Returns the integer that results from the evaluation of ``Expr``,
    where the type of the evaluated ``Expr`` is ``Integer``. Where the
    type of the evaluated ``Expr`` is not ``Integer``, function
    ``real(Expr)`` is called. Its return value is then used to return
    the integer that results from a round-to-nearest rounding method.
    The nearest integer value to the return value is returned, except in
    the case of the value at the exact midpoint between two integer
    values. In this case, the even valued integer is returned. Where
    ``real(Expr)`` returns ``ERROR`` or ``UNDEFINED``, or the integer
    value does not fit into 32 bits, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`random()<single: random(); ClassAd functions>`

``Integer random([ AnyType Expr ])``
    Where the optional argument ``Expr`` evaluates to type ``Integer``
    or type ``Real`` (and called ``x``), the return value is the integer
    or real ``r`` randomly chosen from the interval ``0 <= r < x``. With
    no argument, the return value is chosen with ``random(1.0)``.
    Returns ``ERROR`` in all other cases.

    This function returns ``ERROR`` if greater than 1 argument is given.
    
    :index:`sum()<single: sum(); ClassAd functions>`

``Number sum([ List l ])``
    The elements of l are evaluated, producing a list l of values. If l
    is composed only of numbers, the result is the sum of the values, 
    as a Real if any value is Real, and as an Integer otherwise. If the 
    list is empty, the result is 0. In other cases, the result is ``ERROR``.

    This function returns ``ERROR`` if greater than 1 argument is given.
    
    :index:`avg()<single: avg(); ClassAd functions>`

``Number avg([ List l ])``
    The elements of l are evaluated, producing a list l of values. If l
    is composed only of numbers, the result is the average of the values, 
    as a Real. If the list is empty, the result is 0.  In other cases, 
    the result is ERROR.
    
    :index:`min()<single: min(); ClassAd functions>`

``Number min([ List l ])``
    The elements of l are evaluated, producing a list l of values. If l
    is composed only of numbers, the result is the minimum of the values, 
    as a Real if any value is Real, and as an Integer otherwise. If the list 
    is empty, the result is UNDEFINED. In other cases, the result is ERROR.
    
    :index:`max()<single: max(); ClassAd functions>`

``Number max([ List l ])``
    The elements of l are evaluated, producing a list l of values. If l
    is composed only of numbers, the result is the maximum of the values, 
    as a Real if any value is Real, and as an Integer otherwise. If the list 
    is empty, the result is UNDEFINED. In other cases, the result is ERROR.
    
    :index:`strcat()<single: strcat(); ClassAd functions>`

``String strcat(AnyType Expr1 [ , AnyType Expr2 ...])``
    Returns the string which is the concatenation of all arguments,
    where all arguments are converted to type ``String`` by function
    ``string(Expr)``. Returns ``ERROR`` if any argument evaluates to
    ``UNDEFINED`` or ``ERROR``.
    
    :index:`join()<single: join(); ClassAd functions>`

``String join(String sep, AnyType Expr1 [ , AnyType Expr2 ...])`` OR ``String join(String sep, List list`` OR ``String join(List list``
    Returns the string which is the concatenation of all arguments after
    the first one. The first argument is the separator, and it is
    inserted between each of the other arguments during concatenation.
    All arguments are converted to type ``String`` by function
    ``string(Expr)`` before concatenation. When there are exactly two
    arguments, If the second argument is a List, all members of the list
    are converted to strings and then joined using the separator. When
    there is only one argument, and the argument is a List, all members
    of the list are converted to strings and then concatenated.

    Returns ``ERROR`` if any argument evaluates to ``UNDEFINED`` or
    ``ERROR``.

    For example:

    .. code-block:: text

            "a, b, c" = join(", ", "a", "b", "c")
            "abc"   = join(split("a b c"))
            "a;b;c" = join(";", split("a b c"))

    
    :index:`substr()<single: substr(); ClassAd functions>`

``String substr(String s, Integer offset [ , Integer length ])``
    Returns the substring of ``s``, from the position indicated by
    ``offset``, with (optional) ``length`` characters. The first
    character within ``s`` is at offset 0. If the optional ``length``
    argument is not present, the substring extends to the end of the
    string. If ``offset`` is negative, the value ``(length - offset)``
    is used for the offset. If ``length`` is negative, an initial
    substring is computed, from the offset to the end of the string.
    Then, the absolute value of ``length`` characters are deleted from
    the right end of the initial substring. Further, where characters of
    this resulting substring lie outside the original string, the part
    that lies within the original string is returned. If the substring
    lies completely outside of the original string, the null string is
    returned.

    This function returns ``ERROR`` if greater than 3 or less than 2
    arguments are given. 
    :index:`strcmp()<single: strcmp(); ClassAd functions>`

``Integer strcmp(AnyType Expr1, AnyType Expr2)``
    Both arguments are converted to type ``String`` by function
    ``string(Expr)``. The return value is an integer that will be

    -  less than 0, if ``Expr1`` is lexicographically less than
       ``Expr2``
    -  equal to 0, if ``Expr1`` is lexicographically equal to ``Expr2``
    -  greater than 0, if ``Expr1`` is lexicographically greater than
       ``Expr2``

    Case is significant in the comparison. Where either argument
    evaluates to ``ERROR`` or ``UNDEFINED``, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than 2 arguments are given.

    :index:`stricmp()<single: stricmp(); ClassAd functions>`

``Integer stricmp(AnyType Expr1, AnyType Expr2)``
    This function is the same as ``strcmp``, except that letter case is
    not significant.

    :index:`versioncmp()<single: versioncmp(); ClassAd functions>`

``Integer versioncmp(String left, String right)``
    This function version-compares two strings.  It returns an integer

    - less than zero if ``left`` is an earlier version than ``right``
    - zero if the strings are identical
    - more than zero if ``left`` is a later version than ``right``.

    A version comparison is a lexicographic comparison unless the first
    difference between the two strings occurs in a string of digits, in
    which case, sort by the value of that number (assuming that more
    leading zeroes mean smaller numbers).  Thus ``7.x`` is earlier than
    ``7.y``, ``7.9`` is earlier than ``7.10``, and the following sequence
    is in order: ``000, 00, 01, 010, 09, 0, 1, 9, 10``.

    :index:`versionGT()<single: versionGT(); ClassAd functions>`
    :index:`versionLT()<single: versionLT(); ClassAd functions>`
    :index:`versionGE()<single: versionGE(); ClassAd functions>`
    :index:`versionLE()<single: versionLE(); ClassAd functions>`
    :index:`versionEQ()<single: versionEQ(); ClassAd functions>`

``Boolean versionGT(String left, String right)``
``Boolean versionLT(String left, String right)``
``Boolean versionGE(String left, String right)``
``Boolean versionLE(String left, String right)``
``Boolean versionEQ(String left, String right)``

    As ``versioncmp()`` (above), but for a specific comparison and returning
    a boolean.  The two letter codes stand for "Greater Than", "Less Than",
    "Greater than or Equal", "Less than or Equal", and "EQual", respectively.

    :index:`version_in_range()<single: version_in_range; ClassAd functions>`

``Boolean version_in_range(String version, String min, String max)``

    Equivalent to ``versionLE(min, version) && versionLE(version, max)``.

    :index:`toUpper()<single: toUpper(); ClassAd functions>`

``String toUpper(AnyType Expr)``
    The single argument is converted to type ``String`` by function
    ``string(Expr)``. The return value is this string, with all lower
    case letters converted to upper case. If the argument evaluates to
    ``ERROR`` or ``UNDEFINED``, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`toLower()<single: toLower(); ClassAd functions>`

``String toLower(AnyType Expr)``
    The single argument is converted to type ``String`` by function
    ``string(Expr)``. The return value is this string, with all upper
    case letters converted to lower case. If the argument evaluates to
    ``ERROR`` or ``UNDEFINED``, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`size()<single: size(); ClassAd functions>`

``Integer size(AnyType Expr)``
    If Expr evaluates to a string, return the number of characters in the string.
    If Expr evaluate to a list, return the number of elements in the list.
    If Expr evaluate to a classad, return the number of entries in the ad.
    Otherwise, ``ERROR`` is returned.

    :index:`split()<single: split(); ClassAd functions>`

``List split(String s [ , String tokens ] )``
    Returns a list of the substrings of ``s`` that have been split up by
    using any of the characters within string ``tokens``. If ``tokens``
    is not specified, then all white space characters are used to
    delimit the string.
    
    :index:`splitUserName()<single: splitUserName(); ClassAd functions>`
``List splitUserName(String Name)``
    Returns a list of two strings. Where ``Name`` includes an ``@``
    character, the first string in the list will be the substring that
    comes before the ``@`` character, and the second string in the list
    will be the substring that comes after. Thus, if ``Name`` is
    ``"user@domain"``, then the returned list will be
    {"user", "domain"}. If there is no ``@`` character in ``Name``, then
    the first string in the list will be ``Name``, and the second string
    in the list will be the empty string. Thus, if ``Name`` is
    ``"username"``, then the returned list will be {"username", ""}.
    
    :index:`splitSlotName()<single: splitSlotName(); ClassAd functions>`
``List splitSlotName(String Name)``
    Returns a list of two strings. Where ``Name`` includes an ``@``
    character, the first string in the list will be the substring that
    comes before the ``@`` character, and the second string in the list
    will be the substring that comes after. Thus, if ``Name`` is
    ``"slot1@machine"``, then the returned list will be
    {"slot1", "machine"}. If there is no ``@`` character in ``Name``,
    then the first string in the list will be the empty string, and the
    second string in the list will be ``Name``, Thus, if ``Name`` is
    ``"machinename"``, then the returned list will be
    {"", "machinename"}. 
    :index:`time()<single: time(); ClassAd functions>`
``Integer time()``
    Returns the current coordinated universal time. This is the time, in
    seconds, since midnight of January 1, 1970.
    
    :index:`formatTime()<single: formatTime(); ClassAd functions>`
``String formatTime([ Integer time ] [ , String format ])``
    Returns a formatted string that is a representation of ``time``. The
    argument ``time`` is interpreted as coordinated universal time in
    seconds, since midnight of January 1, 1970. If not specified,
    ``time`` will default to the current time.

    The argument ``format`` is interpreted similarly to the format
    argument of the ANSI C strftime function. It consists of arbitrary
    text plus placeholders for elements of the time. These placeholders
    are percent signs (%) followed by a single letter. To have a percent
    sign in the output, use a double percent sign (%%). If ``format`` is
    not specified, it defaults to ``%c``.

    Because the implementation uses strftime() to implement this, and
    some versions implement extra, non-ANSI C options, the exact options
    available to an implementation may vary. An implementation is only
    required to implement the ANSI C options, which are:

     ``%a``
        abbreviated weekday name
     ``%A``
        full weekday name
     ``%b``
        abbreviated month name
     ``%B``
        full month name
     ``%c``
        local date and time representation
     ``%d``
        day of the month (01-31)
     ``%H``
        hour in the 24-hour clock (0-23)
     ``%I``
        hour in the 12-hour clock (01-12)
     ``%j``
        day of the year (001-366)
     ``%m``
        month (01-12)
     ``%M``
        minute (00-59)
     ``%p``
        local equivalent of AM or PM
     ``%S``
        second (00-59)
     ``%U``
        week number of the year (Sunday as first day of week) (00-53)
     ``%w``
        weekday (0-6, Sunday is 0)
     ``%W``
        week number of the year (Monday as first day of week) (00-53)
     ``%x``
        local date representation
     ``%X``
        local time representation
     ``%y``
        year without century (00-99)
     ``%Y``
        year with century
     ``%Z``
        time zone name, if any

    
    :index:`interval()<single: interval(); ClassAd functions>`

``String interval(Integer seconds)``
    Uses ``seconds`` to return a string of the form ``days+hh:mm:ss``.
    This represents an interval of time. Leading values that are zero
    are omitted from the string. For example, ``seconds`` of 67 becomes
    "1:07". A second example, ``seconds`` of 1472523 = 17\*24\*60\*60 +
    1\*60\*60 + 2\*60 + 3, results in the string "17+1:02:03".
    
    :index:`debug()<single: debug(); ClassAd functions>`
``AnyType debug(AnyType expression)``
    This function evaluates its argument, and it returns the result.
    Thus, it is a no-operation. However, a side-effect of the function
    is that information about the evaluation is logged to the evaluating
    program's log file, at the ``D_FULLDEBUG`` debug level. This is
    useful for determining why a given ClassAd expression is evaluating
    the way it does. For example, if a *condor_startd* ``START``
    expression is unexpectedly evaluating to ``UNDEFINED``, then
    wrapping the expression in this debug() function will log
    information about each component of the expression to the log file,
    making it easier to understand the expression.
    
    :index:`envV1ToV2()<single: envV1ToV2(); ClassAd functions>`
``String envV1ToV2(String old_env)``
    This function converts a set of environment variables from the old
    HTCondor syntax to the new syntax. The single argument should
    evaluate to a string that represents a set of environment variables
    using the old HTCondor syntax (usually stored in the job ClassAd
    attribute ``Env``). The result is the same set of environment
    variables using the new HTCondor syntax (usually stored in the job
    ClassAd attribute ``Environment``). If the argument evaluates to
    ``UNDEFINED``, then the result is also ``UNDEFINED``.
    
    :index:`mergeEnvironment()<single: mergeEnvironment(); ClassAd functions>`
``String mergeEnvironment(String env1 [ , String env2, ... ])``
    This function merges multiple sets of environment variables into a
    single set. If multiple arguments include the same variable, the one
    that appears last in the argument list is used. Each argument should
    evaluate to a string which represents a set of environment variables
    using the new HTCondor syntax or ``UNDEFINED``, which is treated
    like an empty string. The result is a string that represents the
    merged set of environment variables using the new HTCondor syntax
    (suitable for use as the value of the job ClassAd attribute
    ``Environment``).

For the following functions, a delimiter is represented by a string.
Each character within the delimiter string delimits individual strings
within a list of strings that is given by a single string. The default
delimiter contains the comma and space characters. A string within the
list is ended (delimited) by one or more characters within the delimiter
string. 

    :index:`stringListSize()<single: stringListSize(); ClassAd functions>`

``Integer stringListSize(String list [ , String delimiter ])``
    Returns the number of elements in the string ``list``, as delimited
    by the optional ``delimiter`` string. Returns ``ERROR`` if either
    argument is not a string.

    This function returns ``ERROR`` if other than 1 or 2 arguments are
    given. 
    :index:`stringListSum()<single: stringListSum(); ClassAd functions>`

``Integer stringListSum(String list [ , String delimiter ])`` OR ``Real stringListSum(String list [ , String delimiter ])``
    Sums and returns the sum of all items in the string ``list``, as
    delimited by the optional ``delimiter`` string. If all items in the
    list are integers, the return value is also an integer. If any item
    in the list is a real value (noninteger), the return value is a
    real. If any item does not represent an integer or real value, the
    return value is ``ERROR``.
    
    :index:`stringListAvg()<single: stringListAvg(); ClassAd functions>`

``Real stringListAvg(String list [ , String delimiter ])``
    Sums and returns the real-valued average of all items in the string
    ``list``, as delimited by the optional ``delimiter`` string. If any
    item does not represent an integer or real value, the return value
    is ``ERROR``. A list with 0 items (the empty list) returns the value
    0.0. 
    :index:`stringListMin()<single: stringListMin(); ClassAd functions>`

``Integer stringListMin(String list [ , String delimiter ])`` OR ``Real stringListMin(String list [ , String delimiter ])``
    Finds and returns the minimum value from all items in the string
    ``list``, as delimited by the optional ``delimiter`` string. If all
    items in the list are integers, the return value is also an integer.
    If any item in the list is a real value (noninteger), the return
    value is a real. If any item does not represent an integer or real
    value, the return value is ``ERROR``. A list with 0 items (the empty
    list) returns the value ``UNDEFINED``.
    
    :index:`stringListMax()<single: stringListMax(); ClassAd functions>`

``Integer stringListMax(String list [ , String delimiter ])`` OR ``Real stringListMax(String list [ , String delimiter ])``
    Finds and returns the maximum value from all items in the string
    ``list``, as delimited by the optional ``delimiter`` string. If all
    items in the list are integers, the return value is also an integer.
    If any item in the list is a real value (noninteger), the return
    value is a real. If any item does not represent an integer or real
    value, the return value is ``ERROR``. A list with 0 items (the empty
    list) returns the value ``UNDEFINED``.
    
    :index:`stringListMember()<single: stringListMember(); ClassAd functions>`

``Boolean stringListMember(String x, String list [ , String delimiter ])``
    Returns ``TRUE`` if item ``x`` is in the string ``list``, as
    delimited by the optional ``delimiter`` string. Returns ``FALSE`` if
    item ``x`` is not in the string ``list``. Comparison is done with
    ``strcmp()``. The return value is ``ERROR``, if any of the arguments
    are not strings.
    
    :index:`stringListIMember()<single: stringListIMember(); ClassAd functions>`

``Boolean stringListIMember(String x, String list [ , String delimiter ])``
    Same as ``stringListMember()``, but comparison is done with
    ``stricmp()``, so letter case is not relevant.
    
    :index:`stringListsIntersect()<single: stringListsIntersect(); ClassAd functions>`

``Integer stringListsIntersect(String list1, String list2 [ , String delimiter ])``
    Returns ``TRUE`` if the lists contain any matching elements, and
    returns ``FALSE`` if the lists do not contain any matching elements.
    Returns ``ERROR`` if either argument is not a string or if an
    incorrect number of arguments are given.

The following three functions utilize regular expressions as defined and
supported by the PCRE library. See
`http://www.pcre.org <http://www.pcre.org>`_ for complete documentation
of regular expressions.

The ``options`` argument to these functions is a string of special
characters that modify the use of the regular expressions. Inclusion of
characters other than these as options are ignored.

``I`` or ``i``
    Ignore letter case.

``M`` or ``m``
    Modifies the interpretation of the caret (^) and dollar sign ($)
    characters. The caret character matches the start of a string, as
    well as after each newline character. The dollar sign character
    matches before a newline character.

``S`` or ``s``
    The period matches any character, including the newline character.

``F`` or ``f``
    When doing substitution, return the full target string with
    substitutions applied. Normally, only the substitute text is
    returned.

``G`` or ``g``
    When doing substitution, apply the substitution for every matching
    portion of the target string (that doesn't overlap a previous
    match).

    :index:`regexp()<single: regexp(); ClassAd functions>`

``Boolean regexp(String pattern, String target [ , String options ])``
    Uses the regular expression given by string ``pattern`` to scan
    through the string ``target``. Returns ``TRUE`` when ``target``
    matches the regular expression given by ``pattern``. Returns
    ``FALSE`` otherwise. If any argument is not a string, or if
    ``pattern`` does not describe a valid regular expression, returns
    ``ERROR``. 
    :index:`regexpMember()<single: regexpMember(); ClassAd functions>`

``Boolean regexpMember(String pattern, List targetStrings [ , String options ])``
    Uses the description of a regular expression given by string ``pattern``
    to scan through a List of string n ``targetStrings``. Returns ``TRUE`` when ``target``
    matches a regular expression given by ``pattern``. If no strings match,
    and at least one item in targetString evaluated to undefined, returns
    undefined.  If any item in targetString before a match evaluated to neither
    a string nor undefined, returns ``ERROR``. 

    :index:`regexps()<single: regexps(); ClassAd functions>`
``String regexps``
    ``(String pattern, String target, String substitute [ , String options ])``
    Uses the regular expression given by string ``pattern`` to scan
    through the string ``target``. When ``target`` matches the regular
    expression given by ``pattern``, the string ``substitute`` is
    returned, with backslash expansion performed. If any argument is not
    a string, returns ``ERROR``.
    :index:`replace()<single: replace(); ClassAd functions>`
``String replace``
    ``(String pattern, String target, String substitute [ , String options ])``
    Uses the regular expression given by string ``pattern`` to scan
    through the string ``target``. Returns a modified version of
    ``target``, where the first substring that matches ``pattern`` is
    replaced by the string ``substitute``, with backslash expansion
    performed. Equivalent to ``regexps()`` with the ``f`` option. If any
    argument is not a string, returns ``ERROR``.
    :index:`replaceall()<single: replaceall(); ClassAd functions>`
``String replaceall``
    ``(String pattern, String target, String substitute [ , String options ])``
    Uses the regular expression given by string ``pattern`` to scan
    through the string ``target``. Returns a modified version of
    ``target``, where every substring that matches ``pattern`` is
    replaced by the string ``substitute``, with backslash expansion
    performed. Equivalent to ``regexps()`` with the ``fg`` options. If
    any argument is not a string, returns ``ERROR``.
    :index:`stringList_regexpMember()<single: stringList_regexpMember(); ClassAd functions>`
``Boolean stringList_regexpMember``
    ``(String pattern, String list [ , String delimiter ] [ , String options ])``
    Uses the description of a regular expression given by string
    ``pattern`` to scan through the list of strings in ``list``. Returns
    ``TRUE`` when one of the strings in ``list`` is a regular expression
    as described by ``pattern``. The optional ``delimiter`` describes
    how the list is delimited, and string ``options`` modifies how the
    match is performed. Returns ``FALSE`` if ``pattern`` does not match
    any entries in ``list``. The return value is ``ERROR``, if any of
    the arguments are not strings, or if ``pattern`` is not a valid
    regular expression. 
    :index:`userHome()<single: userHome(); ClassAd functions>`
``String userHome(String userName [ , String default ])``
    Returns the home directory of the given user as configured on the
    current system (determined using the getpwdnam() call). (Returns
    ``default`` if the ``default`` argument is passed and the home
    directory of the user is not defined.)
    
    :index:`userMap()<single: userMap(); ClassAd functions>`
``List userMap(String mapSetName, String userName)``
    Map an input string using the given mapping set. Returns a string containing
    the list of groups to which the user belongs separated by commas or undefined
    if the user was not found in the map file.
    
    :index:`userMap()<single: userMap(); ClassAd functions>`

``String userMap(String mapSetName, String userName, String preferredGroup)``
    Map an input string using the given mapping set. Returns a string,
    which is the preferred group if the user is in that group; otherwise
    it is the first group to which the user belongs, or undefined if the
    user belongs to no groups.
    
    :index:`userMap()<single: userMap(); ClassAd functions>`

``String userMap(String mapSetName, String userName, String preferredGroup, String defaultGroup)``
    Map an input string using the given mapping set. Returns a string,
    which is the preferred group if the user is in that group; the first
    group to which the user belongs, if any; and the default group if
    the user belongs to no groups.

    The maps for the ``userMap()`` function are defined by the following
    configuration macros: ``<SUBSYS>_CLASSAD_USER_MAP_NAMES``,
    ``CLASSAD_USER_MAPFILE_<name>`` and ``CLASSAD_USER_MAPDATA_<name>``
    (see the :ref:`admin-manual/configuration-macros:htcondor-wide
    configuration file entries` section).

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
      ``CurrentTime``, which evaluates to the integer value returned by
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
left hand side operand is identical in both type and value to the the
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
pool such as checkpoint servers, submitters of jobs and master daemons.
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
through the *condor_status* and *condor_q* tools which allow users to
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
contains ClassAd information. *condor_advertise* is used to inject this
information, and *condor_status* constrains the search with an
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
   by setting the ``CLASSAD_USER_LIBS`` :index:`CLASSAD_USER_LIBS`
   configuration variable to the full name of the shared library. In
   this case,

   .. code-block:: condor-config

       CLASSAD_USER_LIBS = $(LIBEXEC)/shared.so

-  Restart HTCondor.
-  Test the new functions by running

   .. code-block:: console

       $ condor_status -format "%s\n" todays_date()

:index:`ClassAd`


