ClassAds Introduction
=====================

In this module, you will learn the basics of the `ClassAd
language <https://research.cs.wisc.edu/htcondor/classad/classad.html>`__,
the policy and data exchange language that underpins all of HTCondor.
Before we start to interact with the HTCondor daemons, it is best to
start here.

As always, we start off by importing the relevant module:

.. code:: ipython3

    import classad

Great!

The Python bindings - and HTCondor itself - are strongly centered on the
`ClassAd
language <https://research.cs.wisc.edu/htcondor/classad/classad.html>`__.

The ClassAd language is built around *values* and *expressions*. If you
know Python, both concepts are familiar. Examples of familiar values
include: - Integers (``1``, ``2``, ``3``), - Floating point numbers
(``3.145``, ``-1e-6``) - Booleans (``true`` and ``false``).

Examples of expressions are: - Attribute references: ``foo`` - Boolean
expressions: ``a && b`` - Arithmetic expressions: ``123 + c`` - Function
calls: ``ifThenElse(foo == 123, 3.14, 5.2)``

Expressions can be evaluated to values. Unlike many programming
languages, expressions are lazily-evaluated: they are kept in memory
until a value is explicitly requested.

The Python bindings interact with the ClassAd language through the
``ExprTree`` and ``ClassAd`` objects. Let's first examine simple
expressions:

.. code:: ipython3

    arith_expr = classad.ExprTree("1 + 4")
    print("ClassAd arithemetic expression: {} (of type {})".format(arith_expr, type(arith_expr)))
    print(arith_expr.eval())


.. parsed-literal::

    ClassAd arithemetic expression: 1 + 4 (of type <class 'classad._classad.ExprTree'>)
    5


.. code:: ipython3

    function_expr = classad.ExprTree("ifThenElse(4 > 6, 123, 456)")
    print("Function expression: {}".format(function_expr))
    value = function_expr.eval()
    print("Corresponding value: {} (of type {})".format(value, type(value)))


.. parsed-literal::

    Function expression: ifThenElse(4 > 6,123,456)
    Corresponding value: 456 (of type <class 'int'>)


Notice that, when possible, we convert ClassAd values to Python values.
Hence, the result of evaluating the expression above is the Python
number ``456``.

There are two important values in the ClassAd language that have no
direct equivalent in Python: ``Undefined`` and ``Error``.

``Undefined`` occurs when a reference occurs to an attribute that is not
defined; it is analogous to a ``NameError`` exception in Python (but
there is no concept of an exception in ClassAds). ``Error`` occurs
primarily when an expression combines two different types or when a
function call occurs with the incorrect arguments.

.. code:: ipython3

    print(classad.ExprTree("foo").eval())
    print(classad.ExprTree('5 + "bar"').eval())
    print(classad.ExprTree('ifThenElse(1, 2, 3, 4, 5)').eval())


.. parsed-literal::

    Undefined
    Error
    Error


**So what?**

Expressions, values, and various error conditions are not particularly
inspiring or new.

The concept that makes the ClassAd language special is, of course, the
*ClassAd*!

The ClassAd is analogous to a Python or JSON dictionary. *Unlike* a
dictionary, which is a set of unique key-value pairs, the ClassAd object
is a set of key-\ *expression* pairs. The expressions in the ad can
contain attribute references to other keys in the ad.

There are two common ways to represent ClassAds. The "new ClassAd"
format:

::

    [a = 1;
     b = "foo";
     c = b
    ]

And the "old ClassAd" format:

::

    a = 1
    b = "foo"
    c = b

Despite the "new" and "old" monikers, "new" is over a decade old and
HTCondor command line tools utilize the "old" representation; the Python
bindings default to "new".

A ``ClassAd`` object may be initialized via a string using one of the
above representation. As a ClassAd is so similar to a Python dictionary,
they may also be constructed from a dictionary.

Let's construct some ClassAds!

.. code:: ipython3

    ad1 = classad.ClassAd("""[
    a = 1;
    b = "foo";
    c = b;
    d = a + 4;
    ]""")
    print(ad1)


.. parsed-literal::

    
        [
            a = 1; 
            b = "foo"; 
            c = b; 
            d = a + 4
        ]


ClassAds are quite similar to dictionaries; in Python, the ``ClassAd``
object behaves similarly to a dictionary and has similar convenience
methods:

.. code:: ipython3

    print(ad1["a"])
    print(ad1["not_here"])


.. parsed-literal::

    1


::


    ---------------------------------------------------------------------------

    KeyError                                  Traceback (most recent call last)

    <ipython-input-6-dc6144e1855f> in <module>
          1 print(ad1["a"])
    ----> 2 print(ad1["not_here"])
    

    KeyError: 'not_here'


.. code:: ipython3

    print(ad1.get("not_here", 5))


.. parsed-literal::

    5


.. code:: ipython3

    ad1.update({"e": 8, "f": True})
    for key in ad1:
        print(key, ad1[key])


.. parsed-literal::

    f True
    e 8
    a 1
    b foo
    c b
    d a + 4


Remember our example of an ``Undefined`` attribute above? We now can
evaluate references within the ad:

.. code:: ipython3

    print(ad1.eval("d"))


.. parsed-literal::

    5


Note that an expression is still not evaluated until requested, even if
it is invalid:

.. code:: ipython3

    ad1["g"] = classad.ExprTree("b + 5")
    print(ad1["g"], type(ad1["g"]))
    print(ad1.eval("g"))


.. parsed-literal::

    b + 5 <class 'classad._classad.ExprTree'>
    Error


Onto HTCondor!
--------------

ClassAds and expressions are core concepts in interacting with HTCondor.
Internally, machines and jobs are represented as ClassAds; expressions
are used to filter objects and to define policy.

There's much more to learn in ClassAds! For now, you have enough
background to continue to the next tutorial - `HTCondor
Introduction <HTCondor-Introduction.ipynb>`__.

