
Predefined Functions
====================

:index:`expression functions<single: expression functions; ClassAd>`
:index:`ClassAd functions`

Any ClassAd expression may utilize predefined functions. Function names
are case insensitive. Parameters to functions and a return value from a
function may be typed (as given) or not. Nested or recursive function
calls are allowed.

Here are descriptions of each of these predefined functions. The
possible types are the same as itemized in
:ref:`classads/classad-mechanism:classad syntax`. Where the type may
be any of these literal types, it is called out as AnyType. Where the type is
Integer, but only returns the value 1 or 0 (implying ``True`` or
``False``), it is called out as Boolean. The format of each function is
given as

.. code-block:: text

    ReturnType FunctionName(ParameterType parameter1, ParameterType parameter2, ...)

Optional parameters are given within square brackets.

:classad-function-def:`AnyType eval(AnyType Expr)`
    Evaluates ``Expr`` as a string and then returns the result of
    evaluating the contents of the string as a ClassAd expression. This
    is useful when referring to an attribute such as ``slotX_State``
    where ``X``, the desired slot number is an expression, such as
    ``SlotID+10``. In such a case, if attribute :ad-attr:`SlotID` is 5, the
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

:classad-function-def:`String unparse(Attribute attr)`
    This function looks up the value of the provided attribute and
    returns the unparsed version as a string. The attribute's value is
    not evaluated. If the attribute's value is ``x + 3``, then the
    function would return the string ``"x + 3"``. If the provided
    attribute cannot be found, an empty string is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given or the argument is not an attribute reference.

:classad-function-def:`String unresolved(Attribute attr)`
    This function returns the external attribute references and unresolved
    attribute references of the expression that is the value of the provided attribute.
    If the provided attribute cannot be found, then ``undefined`` is returned.

    For example, in a typical job ClassAd if the ``Requirements`` expression has the value
    ``OpSys == "LINUX" && TARGET.Arch == "ARM" && Cpus >= RequestCpus``, then 
    ``unresolved(Requirements)`` will return ``"Arch,Cpus,OpSys"`` because those will not
    be attributes of the job ClassAd.

:classad-function-def:`Boolean unresolved(Attribute attr, String pattern)`
    This function returns ``True`` when at least one of the external or unresolved attribute
    references of the expression that is the value of the provided attribute matches the
    given Perl regular expression pattern.  If none of the references match the pattern, then
    ``False`` is returned. If the provided attribute cannot be found, then ``undefined`` is returned.

    For example, in a typical job ClassAd if the ``Requirements`` expression has the value
    ``OpSys == "LINUX" && Arch == "ARM"``, then ``unresolved(Requirements, "^OpSys")`` will
    return ``True``, and ``unresolved(Requirements, "OpSys.+")`` will return ``False``.

    The intended use of this function is to make it easier to apply a submit transform to
    a job only when the job does not already reference a certain attribute. For instance

    ..  code-block:: condor-config

        JOB_TRANSFORM_DefPlatform @=end
           # Apply this transform only when the job requirements does not reference OpSysAndver or OpSysName
           REQUIREMENTS ! unresolved(Requirements, "OpSys.+")
           # Add a clause to the job requirements to match only CentOs7 machines
           SET Requirements $(MY.Requirements) && OpSysAndVer == "CentOS7"
        @end

:classad-function-def:`AnyType ifThenElse(AnyType IfExpr,AnyType ThenExpr, AnyType ElseExpr)`
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

:classad-function-def:`Boolean isUndefined(AnyType Expr)`
    Returns ``True``, if ``Expr`` evaluates to ``UNDEFINED``. Returns
    ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Boolean isError(AnyType Expr)`
    Returns ``True``, if ``Expr`` evaluates to ``ERROR``. Returns
    ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Boolean isString(AnyType Expr)`
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``String``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Boolean isInteger(AnyType Expr)`
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``Integer``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Boolean isReal(AnyType Expr)`
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``Real``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Boolean isList(AnyType Expr)`
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``List``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Boolean isClassAd(AnyType Expr)`
    Returns ``True``, if the evaluation of ``Expr`` gives a value of
    type ``ClassAd``. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Boolean isBoolean(AnyType Expr)`
    Returns ``True``, if the evaluation of ``Expr`` gives the integer
    value 0 or 1. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Boolean isAbstime(AnyType Expr)`
    Returns ``True``, if the evaluation of ``Expr`` returns an abstime
    type. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 
    :index:`isRelTime()<single: isRelTime(); ClassAd functions>`

:classad-function-def:`Boolean isReltime(AnyType Expr)`
    Returns ``True``, if the evaluation of ``Expr`` returns an relative time
    type. Returns ``False`` in all other cases.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Boolean member(AnyType m, ListType l)`
    Returns error if m does not evalute to a scalar, or l does not
    evaluate to a list.  Otherwise the elements of l are evaluted
    in order, and if an element is equal to m in the sense of ``==``
    the result of the function is ``True``.  Otherwise the function
    returns false.

:classad-function-def:`Boolean anyCompare(string op, list l, AnyType t)`
    Returns error if op does not evalute to one of ``<``, ``<=``,
    ``==``, ``>``, ``>=``, ``!-``, ``is`` or ``isnt``. Returns error
    if l isn't a list, or t isn't a scalar
    Otherwise the elements of l are evaluted and compared to t
    using the corresponding operator defined by op. If any of
    the members of l evaluate to true, the result is ``True``.
    Otherwise the function returns ``False``.

:classad-function-def:`Boolean allCompare(string op, list l, AnyType t)`
    Returns error if op does not evalute to one of ``<``, ``<=``,
    ``==``, ``>``, ``>=``, ``!-``, ``is`` or ``isnt``. Returns error
    if l isn't a list, or t isn't a scalar
    Otherwise the elements of l are evaluted and compared to t
    using the corresponding operator defined by op. If all of
    the members of l evaluate to true, the result is ``True``.
    Otherwise the function returns ``False``.

:classad-function-def:`Boolean identicalMember(AnyType m, ListType l)`
    Returns error if m does not evalute to a scalar, or l does not
    evaluate to a list.  Otherwise the elements of l are evaluted
    in order, and if an element is equal to m in the sense of ``=?=``
    the result of the function is ``True``.  Otherwise the function
    returns false.

:classad-function-def:`Integer int(AnyType Expr)`
    Returns the integer value as defined by ``Expr``. Where the type of
    the evaluated ``Expr`` is ``Real``, the value is truncated (round
    towards zero) to an integer. Where the type of the evaluated
    ``Expr`` is ``String``, the string is converted to an integer using
    a C-like atoi() function. When this result is not an integer,
    ``ERROR`` is returned. Where the evaluated ``Expr`` is ``ERROR`` or
    ``UNDEFINED``, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Real real(AnyType Expr)`
    Returns the real value as defined by ``Expr``. Where the type of the
    evaluated ``Expr`` is ``Integer``, the return value is the converted
    integer. Where the type of the evaluated ``Expr`` is ``String``, the
    string is converted to a real value using a C-like atof() function.
    When this result is not a real, ``ERROR`` is returned. Where the
    evaluated ``Expr`` is ``ERROR`` or ``UNDEFINED``, ``ERROR`` is
    returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`String string(AnyType Expr)`
    Returns the string that results from the evaluation of ``Expr``.
    Converts a non-string value to a string. Where the evaluated
    ``Expr`` is ``ERROR`` or ``UNDEFINED``, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Bool bool(AnyType Expr)`
    Returns the boolean that results from the evaluation of ``Expr``.
    Converts a non-boolean value to a bool. A string expression
    that evaluates to the string "true" yields true, and "false" returns

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`AbsTime absTime(AnyType t [, int z])`
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

:classad-function-def:`RelTime relTime(AnyType t)`
     If the operand t is a String, it is parsed as a specification of a
     time interval. This function accepts the canonical native representation of RelTime values, but minor
     variations in format are allowed.

     Otherwise, t is converted to a Real by the function real, and treated as a number of seconds.
     The default string format is [-]days+hh:mm:ss.fff, where leading components and the fraction .fff
     are omitted if they are zero. In the default syntax, days is a sequence of digits starting with a non-zero
     digit, hh, mm, and ss are strings of exactly two digits (padded on the left with zeros if necessary) with
     values less than 24, 60, and 60, respectively and fff is a string of exactly three digits.

:classad-function-def:`Integer floor(AnyType Expr)`
    Returns the integer that results from the evaluation of ``Expr``,
    where the type of the evaluated ``Expr`` is ``Integer``. Where the
    type of the evaluated ``Expr`` is not ``Integer``, function
    ``real(Expr)`` is called. Its return value is then used to return
    the largest magnitude integer that is not larger than the returned
    value. Where ``real(Expr)`` returns ``ERROR`` or ``UNDEFINED``,
    ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Integer ceiling(AnyType Expr)`
    Returns the integer that results from the evaluation of ``Expr``,
    where the type of the evaluated ``Expr`` is ``Integer``. Where the
    type of the evaluated ``Expr`` is not ``Integer``, function
    ``real(Expr)`` is called. Its return value is then used to return
    the smallest magnitude integer that is not less than the returned
    value. Where ``real(Expr)`` returns ``ERROR`` or ``UNDEFINED``,
    ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Integer pow(Integer base, Integer exponent)` OR :classad-function-def:`Real pow(Integer base, Integer exponent)` OR :classad-function-def:`Real pow(Real base, Real exponent)`
    Calculates ``base`` raised to the power of ``exponent``. If
    ``exponent`` is an integer value greater than or equal to 0, and
    ``base`` is an integer, then an integer value is returned. If
    ``exponent`` is an integer value less than 0, or if either ``base``
    or ``exponent`` is a real, then a real value is returned. An
    invocation with ``exponent=0`` or ``exponent=0.0``, for any value of
    ``base``, including 0 or 0.0, returns the value 1 or 1.0, type
    appropriate. 

:classad-function-def:`Integer quantize(AnyType a, Integer b)` OR :classad-function-def:`Real quantize(AnyType a, Real b)` OR :classad-function-def:`AnyType quantize(AnyType a, AnyType list b)`
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

:classad-function-def:`Integer round(AnyType Expr)`
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

:classad-function-def:`Integer random([ AnyType Expr ])`
    Where the optional argument ``Expr`` evaluates to type ``Integer``
    or type ``Real`` (and called ``x``), the return value is the integer
    or real ``r`` randomly chosen from the interval ``0 <= r < x``. With
    no argument, the return value is chosen with ``random(1.0)``.
    Returns ``ERROR`` in all other cases.

    This function returns ``ERROR`` if greater than 1 argument is given.

:classad-function-def:`Number sum([ List l ])`
    The elements of l are evaluated, producing a list l of values. Undefined values
    are removed. If the resulting l is composed only of numbers, the result is the sum of the values,
    as a Real if any value is Real, and as an Integer otherwise. If the 
    list is empty, the result is 0. If the list has only Undefined values, the result
    is ``UNDEFINED``. In other cases, the result is ``ERROR``.

    This function returns ``ERROR`` if greater than 1 argument is given.

:classad-function-def:`Number avg([ List l ])`
    The elements of l are evaluated, producing a list l of values. Undefined values
    are removed. If the resulting l is composed only of numbers, the result is the average of the values,
    as a Real. If the list is empty, the result is 0.
    If the list has only Undefined values, the result is ``UNDEFINED``.
    In other cases,  the result is ERROR.

:classad-function-def:`Number min([ List l ])`
    The elements of l are evaluated, producing a list l of values.
    Undefined values are removed.
    If the resulting l is composed only of numbers, the result is the minimum of the values,
    as a Real if any value is Real, and as an Integer otherwise. If the list 
    is empty, the result is UNDEFINED. In other cases, the result is ERROR.


:classad-function-def:`Number max([ List l ])`
    The elements of l are evaluated, producing a list l of values.
    Undefined values are removed.
    If the resulting l is composed only of numbers, the result is the maximum of the values,
    as a Real if any value is Real, and as an Integer otherwise. If the list 
    is empty, the result is UNDEFINED. In other cases, the result is ERROR.

:classad-function-def:`String strcat(AnyType Expr1 [ , AnyType Expr2 ...])`
    Returns the string which is the concatenation of all arguments,
    where all arguments are converted to type ``String`` by function
    ``string(Expr)``. Returns ``ERROR`` if any argument evaluates to
    ``UNDEFINED`` or ``ERROR``.

:classad-function-def:`String join(String sep, AnyType Expr1 [ , AnyType Expr2 ...])` OR :classad-function-def:`String join(String sep, List list)` OR :classad-function-def:`String join(List list)`
    Returns the string which is the concatenation of all arguments after
    the first one. The first argument is the separator, and it is
    inserted between each of the other arguments during concatenation.
    All arguments which are not undefined are converted to type ``String`` by function
    ``string(Expr)`` before concatenation. Undefined arguments are skipped.
    When there are exactly two arguments, If the second argument is a List, all members of the list
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

:classad-function-def:`String substr(String s, Integer offset [ , Integer length ])`
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

:classad-function-def:`Integer strcmp(AnyType Expr1, AnyType Expr2)`
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

:classad-function-def:`Integer stricmp(AnyType Expr1, AnyType Expr2)`
    This function is the same as :classad-function:`strcmp`, except that letter case is
    not significant.

:classad-function-def:`Integer versioncmp(String left, String right)`
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

:classad-function-def:`Boolean versionGT(String left, String right)` OR :classad-function-def:`Boolean versionGE(String left, String right)`
    As :classad-function:`versioncmp()`, but for specific comparison of
    "Greater Than (GT)" and "Greater Than or Equal (GE)". Returns a boolean.

:classad-function-def:`Boolean versionLT(String left, String right)` OR :classad-function-def:`Boolean versionLE(String left, String right)`
    As :classad-function:`versioncmp()`, but for specific comparison of
    "Less Than (LT)" and "Less Than or Equal (LE)". Returns a boolean.

:classad-function-def:`Boolean versionEQ(String left, String right)`
    As :classad-function:`versioncmp()`, but for a specific comparison
    of equivalence. Returns a boolean.

:classad-function-def:`Boolean version_in_range(String version, String min, String max)`
    Equivalent to ``versionLE(min, version) && versionLE(version, max)``.

:classad-function-def:`String toUpper(AnyType Expr)`
    The single argument is converted to type ``String`` by function
    ``string(Expr)``. The return value is this string, with all lower
    case letters converted to upper case. If the argument evaluates to
    ``ERROR`` or ``UNDEFINED``, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`String toLower(AnyType Expr)`
    The single argument is converted to type ``String`` by function
    ``string(Expr)``. The return value is this string, with all upper
    case letters converted to lower case. If the argument evaluates to
    ``ERROR`` or ``UNDEFINED``, ``ERROR`` is returned.

    This function returns ``ERROR`` if other than exactly 1 argument is
    given. 

:classad-function-def:`Integer size(AnyType Expr)`
    If Expr evaluates to a string, return the number of characters in the string.
    If Expr evaluate to a list, return the number of elements in the list.
    If Expr evaluate to a classad, return the number of entries in the ad.
    Otherwise, ``ERROR`` is returned.

:classad-function-def:`List split(String s [ , String tokens ] )`
    Returns a list of the substrings of ``s`` that have been split up by
    using any of the characters within string ``tokens``. If ``tokens``
    is not specified, then all white space characters are used to
    delimit the string.

:classad-function-def:`List splitUserName(String Name)`
    Returns a list of two strings. Where ``Name`` includes an ``@``
    character, the first string in the list will be the substring that
    comes before the ``@`` character, and the second string in the list
    will be the substring that comes after. Thus, if ``Name`` is
    ``"user@domain"``, then the returned list will be
    {"user", "domain"}. If there is no ``@`` character in ``Name``, then
    the first string in the list will be ``Name``, and the second string
    in the list will be the empty string. Thus, if ``Name`` is
    ``"username"``, then the returned list will be {"username", ""}.

:classad-function-def:`List splitSlotName(String Name)`
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

:classad-function-def:`Integer time()`
    Returns the current coordinated universal time. This is the time, in
    seconds, since midnight of January 1, 1970.

:classad-function-def:`String formatTime([ Integer time ] [ , String format ])`
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

:classad-function-def:`String interval(Integer seconds)`
    Uses ``seconds`` to return a string of the form ``days+hh:mm:ss``.
    This represents an interval of time. Leading values that are zero
    are omitted from the string. For example, ``seconds`` of 67 becomes
    "1:07". A second example, ``seconds`` of 1472523 = 17\*24\*60\*60 +
    1\*60\*60 + 2\*60 + 3, results in the string "17+1:02:03".

:classad-function-def:`String evalInEachContext(Expression expr, List contexts)`
    This function evaluates its first argument as an expression in the context of
    each Classad in the second argument and returns a list that is the result of
    each evaluation. The first argument should be an expression. 
    If the second argument does not evaluate to a list of ClassAds, ``ERROR`` is returned.

    For example:

    .. code-block:: text

            {true, false} = evalInEachContext(Prio > 2, { [Prio=3;], [Prio=1;] })
            {3, 1} = evalInEachContext(Prio, { [Prio=3;], [Prio=1;] })
            ERROR = evalInEachContext(Prio > 2, { [Prio=3;], UNDEFINED })
            ERROR = evalInEachContext(Prio > 2, UNDEFINED)

:classad-function-def:`String countMatches(Expression expr, List contexts)`
    This function evaluates its first argument as an expression in the context of
    each Classad in the second argument and returns a count of the results that
    evaluated to ``True``. The first argument should be an expression. The second argument
    should be a list of ClassAds or a list of attribute references to classAds, or
    should evaluate to a list of ClassAds.  This function will always return a integer
    value when the first argument is defined and the second argument is not ``ERROR``.

    For example:

    .. code-block:: text

            1 = countMatches(Prio > 2, { [Prio=3;], [Prio=1;] })
            1 = countMatches(Prio > 2, { [Prio=3;], UNDEFINED })
            0 = countMatches(Prio > 2, UNDEFINED)

:classad-function-def:`AnyType debug(AnyType expression)`
    This function evaluates its argument, and it returns the result.
    Thus, it is a no-operation. However, a side-effect of the function
    is that information about the evaluation is logged to the evaluating
    program's log file, at the ``D_FULLDEBUG`` debug level. This is
    useful for determining why a given ClassAd expression is evaluating
    the way it does. For example, if a *condor_startd* :macro:`START`
    expression is unexpectedly evaluating to ``UNDEFINED``, then
    wrapping the expression in this debug() function will log
    information about each component of the expression to the log file,
    making it easier to understand the expression.

:classad-function-def:`String envV1ToV2(String old_env)`
    This function converts a set of environment variables from the old
    HTCondor syntax to the new syntax. The single argument should
    evaluate to a string that represents a set of environment variables
    using the old HTCondor syntax (usually stored in the job ClassAd
    attribute :ad-attr:`Env`). The result is the same set of environment
    variables using the new HTCondor syntax (usually stored in the job
    ClassAd attribute :ad-attr:`Environment`). If the argument evaluates to
    ``UNDEFINED``, then the result is also ``UNDEFINED``.

:classad-function-def:`String mergeEnvironment(String env1 [ , String env2, ... ])`
    This function merges multiple sets of environment variables into a
    single set. If multiple arguments include the same variable, the one
    that appears last in the argument list is used. Each argument should
    evaluate to a string which represents a set of environment variables
    using the new HTCondor syntax or ``UNDEFINED``, which is treated
    like an empty string. The result is a string that represents the
    merged set of environment variables using the new HTCondor syntax
    (suitable for use as the value of the job ClassAd attribute
    :ad-attr:`Environment`).

For the following functions, a delimiter is represented by a string.
Each character within the delimiter string delimits individual strings
within a list of strings that is given by a single string. The default
delimiter contains the comma and space characters. A string within the
list is ended (delimited) by one or more characters within the delimiter
string. 

:classad-function-def:`Integer stringListSize(String list [ , String delimiter ])`
    Returns the number of elements in the string ``list``, as delimited
    by the optional ``delimiter`` string. Returns ``ERROR`` if either
    argument is not a string.

    This function returns ``ERROR`` if other than 1 or 2 arguments are
    given. 

:classad-function-def:`Integer stringListSum(String list [ , String delimiter ])` OR :classad-function-def:`Real stringListSum(String list [ , String delimiter ])`
    Sums and returns the sum of all items in the string ``list``, as
    delimited by the optional ``delimiter`` string. If all items in the
    list are integers, the return value is also an integer. If any item
    in the list is a real value (noninteger), the return value is a
    real. If any item does not represent an integer or real value, the
    return value is ``ERROR``.

:classad-function-def:`Real stringListAvg(String list [ , String delimiter ])`
    Sums and returns the real-valued average of all items in the string
    ``list``, as delimited by the optional ``delimiter`` string. If any
    item does not represent an integer or real value, the return value
    is ``ERROR``. A list with 0 items (the empty list) returns the value
    0.0. 

:classad-function-def:`Integer stringListMin(String list [ , String delimiter ])` OR :classad-function-def:`Real stringListMin(String list [ , String delimiter ])`
    Finds and returns the minimum value from all items in the string
    ``list``, as delimited by the optional ``delimiter`` string. If all
    items in the list are integers, the return value is also an integer.
    If any item in the list is a real value (noninteger), the return
    value is a real. If any item does not represent an integer or real
    value, the return value is ``ERROR``. A list with 0 items (the empty
    list) returns the value ``UNDEFINED``.

:classad-function-def:`Integer stringListMax(String list [ , String delimiter ])` OR :classad-function-def:`Real stringListMax(String list [ , String delimiter ])`
    Finds and returns the maximum value from all items in the string
    ``list``, as delimited by the optional ``delimiter`` string. If all
    items in the list are integers, the return value is also an integer.
    If any item in the list is a real value (noninteger), the return
    value is a real. If any item does not represent an integer or real
    value, the return value is ``ERROR``. A list with 0 items (the empty
    list) returns the value ``UNDEFINED``.

:classad-function-def:`Boolean stringListMember(String x, String list [ , String delimiter ])`
    Returns ``TRUE`` if item ``x`` is in the string ``list``, as
    delimited by the optional ``delimiter`` string. Returns ``FALSE`` if
    item ``x`` is not in the string ``list``. Comparison is done with
    ``strcmp()``. The return value is ``ERROR``, if any of the arguments
    are not strings.

:classad-function-def:`Boolean stringListIMember(String x, String list [ , String delimiter ])`
    Same as ``stringListMember()``, but comparison is done with
    :classad-function:`stricmp()`, so letter case is not relevant.

:classad-function-def:`Integer stringListsIntersect(String list1, String list2 [ , String delimiter ])`
    Returns ``TRUE`` if the lists contain any matching elements, and
    returns ``FALSE`` if the lists do not contain any matching elements.
    Returns ``ERROR`` if either argument is not a string or if an
    incorrect number of arguments are given.

:classad-function-def:`Boolean stringListSubsetMatch(String list1, String list2 [ , String delimiter ])`
    Returns ``TRUE`` if all item in the string ``list1`` are also in the string ``list2``, as
    delimited by the optional ``delimiter`` string.  Returns ``FALSE`` if
    ``list1`` has any items that are not in ``list2``. Both lists are treated as sets. Empty items
    and duplicate items are ignored. The return value is ``TRUE`` if ``list1`` is ``UNDEFINED`` or empty
    and ``list2`` is any string value.  The return value is ``FALSE`` if ``list1`` is any string value and ``list2`` is
    ``UNDEFINED``.  The return value is ``UNDEFINED`` if both ``list1`` and ``list2`` are ``UNDEFINED``.
    The return value is ``ERROR``, if any of the arguments are not either strings or ``UNDEFINED``

:classad-function-def:`Boolean stringListISubsetMatch(String list1, String list2 [ , String delimiter ])`
    Same as ``stringListSubsetMatch()``, but the sets are case-insensitive.


The following six functions utilize regular expressions as defined and
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

:classad-function-def:`Boolean regexp(String pattern, String target [ , String options ])`
    Uses the regular expression given by string ``pattern`` to scan
    through the string ``target``. Returns ``TRUE`` when ``target``
    matches the regular expression given by ``pattern``. Returns
    ``FALSE`` otherwise. If any argument is not a string, or if
    ``pattern`` does not describe a valid regular expression, returns
    ``ERROR``. 

:classad-function-def:`Boolean regexpMember(String pattern, List targetStrings [ , String options ])`
    Uses the description of a regular expression given by string ``pattern``
    to scan through a List of string n ``targetStrings``. Returns ``TRUE`` when ``target``
    matches a regular expression given by ``pattern``. If no strings match,
    and at least one item in targetString evaluated to undefined, returns
    undefined.  If any item in targetString before a match evaluated to neither
    a string nor undefined, returns ``ERROR``. 

:classad-function-def:`String regexps(String pattern, String target, String substitute [ , String options ])`
    Uses the regular expression given by string ``pattern`` to scan
    through the string ``target``. When ``target`` matches the regular
    expression given by ``pattern``, the string ``substitute`` is
    returned, with backslash expansion performed. If any argument is not
    a string, returns ``ERROR``.

:classad-function-def:`String replace(String pattern, String target, String substitute [ , String options ])`
    Uses the regular expression given by string ``pattern`` to scan
    through the string ``target``. Returns a modified version of
    ``target``, where the first substring that matches ``pattern`` is
    replaced by the string ``substitute``, with backslash expansion
    performed. Equivalent to ``regexps()`` with the ``f`` option. If any
    argument is not a string, returns ``ERROR``.

:classad-function-def:`String replaceall(String pattern, String target, String substitute [ , String options ])`
    Uses the regular expression given by string ``pattern`` to scan
    through the string ``target``. Returns a modified version of
    ``target``, where every substring that matches ``pattern`` is
    replaced by the string ``substitute``, with backslash expansion
    performed. Equivalent to ``regexps()`` with the ``fg`` options. If
    any argument is not a string, returns ``ERROR``.

:classad-function-def:`Boolean stringList_regexpMember(String pattern, String list [ , String delimiter ] [ , String options ])`
    Uses the description of a regular expression given by string
    ``pattern`` to scan through the list of strings in ``list``. Returns
    ``TRUE`` when one of the strings in ``list`` is a regular expression
    as described by ``pattern``. The optional ``delimiter`` describes
    how the list is delimited, and string ``options`` modifies how the
    match is performed. Returns ``FALSE`` if ``pattern`` does not match
    any entries in ``list``. The return value is ``ERROR``, if any of
    the arguments are not strings, or if ``pattern`` is not a valid
    regular expression. 

:classad-function-def:`String userHome(String userName [ , String default ])`
    Returns the home directory of the given user as configured on the
    current system (determined using the getpwdnam() call). (Returns
    ``default`` if the ``default`` argument is passed and the home
    directory of the user is not defined.)

:classad-function-def:`List userMap(String mapSetName, String userName)`
    Map an input string using the given mapping set. Returns a string containing
    the list of groups to which the user belongs separated by commas or undefined
    if the user was not found in the map file.
    
:classad-function-def:`String userMap(String mapSetName, String userName, String preferredGroup)`
    Map an input string using the given mapping set. Returns a string,
    which is the preferred group if the user is in that group; otherwise
    it is the first group to which the user belongs, or undefined if the
    user belongs to no groups.

:classad-function-def:`String userMap(String mapSetName, String userName, String preferredGroup, String defaultGroup)`
    Map an input string using the given mapping set. Returns a string,
    which is the preferred group if the user is in that group; the first
    group to which the user belongs, if any; and the default group if
    the user belongs to no groups.

    The maps for the ``userMap()`` function are defined by the following
    configuration macros: :macro:`<SUBSYS>_CLASSAD_USER_MAP_NAMES`,
    :macro:`CLASSAD_USER_MAPFILE_<name>` and :macro:`CLASSAD_USER_MAPDATA_<name>`
    (see the :ref:`admin-manual/configuration-macros:htcondor-wide
    configuration file entries` section).


