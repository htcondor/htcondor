#!/usr/bin/python

import os
import re
import sys
import types
import pickle
import classad
import datetime
import unittest
import warnings
import tempfile

_long_type = int if sys.version_info > (3,) else long

class TestClassad(unittest.TestCase):

    def test_classad_constructor(self):
        ad = classad.ClassAd('[foo = "1"; bar = 2]')
        self.assertEqual(ad['foo'], "1")
        self.assertEqual(ad['bar'], 2)
        self.assertRaises(KeyError, ad.__getitem__, 'baz')

    def test_pickle(self):
        ad = classad.ClassAd({"one": 1})
        expr = classad.ExprTree("2+2")
        pad = pickle.dumps(ad)
        pexpr = pickle.dumps(expr)
        ad2 = pickle.loads(pad)
        expr2 = pickle.loads(pexpr)
        self.assertEqual(ad2.__repr__(), "[ one = 1 ]")
        self.assertEqual(expr2.__repr__(), "2 + 2")

    def test_load_classad_from_file(self):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            ad = classad.parse(open("tests/test.ad"))
        self.assertEqual(ad["foo"], "bar")
        self.assertEqual(ad["baz"], classad.Value.Undefined)
        self.assertRaises(KeyError, ad.__getitem__, "bar")

    def test_load_classad_from_file_v2(self):
        ad = classad.parseOne(open("tests/test.ad"))
        self.assertEqual(ad["foo"], "bar")
        self.assertEqual(ad["baz"], classad.Value.Undefined)
        self.assertRaises(KeyError, ad.__getitem__, "bar")

    def one_ad_verify(self, ad):
        self.assertEqual(len(ad), 2)
        self.assertEqual(ad["foo"], 1)
        self.assertEqual(ad["bar"], 2)

    def test_parse_one(self):
        ad = classad.parseOne("foo = 1\nbar = 2")
        self.one_ad_verify(ad)
        ad = classad.parseOne("[foo = 1; bar = 2]")
        self.one_ad_verify(ad)
        ad = classad.parseOne("foo = 1", classad.Parser.New)
        self.assertEqual(len(ad), 0)
        self.one_ad_verify(classad.parseOne("foo = 1\nbar = 2\n"))
        self.one_ad_verify(classad.parseOne("foo = 1\nbar = 1\n\nbar = 2\n"))
        ad = classad.parseOne("[foo = 1]", classad.Parser.Old)
        self.assertEqual(len(ad), 0)
        self.one_ad_verify(classad.parseOne("[foo = 1; bar = 1;] [bar = 2]"))
        self.one_ad_verify(classad.parseOne("-------\nfoo = 1\nbar = 2\n\n"))

    def test_parse_iter(self):
        tf = tempfile.TemporaryFile()
        tf.write(b"[foo = 1] [bar = 2]")
        tf.seek(0)
        if sys.version_info > (3,):
            tf,tf_ = open(tf.fileno()), tf
        ad_iter = classad.parseAds(tf)
        ad = next(ad_iter)
        self.assertEqual(len(ad), 1)
        self.assertEqual(ad["foo"], 1)
        self.assertEqual(" [bar = 2]", tf.read())
        tf = tempfile.TemporaryFile()
        tf.write(b"-----\nfoo = 1\n\nbar = 2\n")
        tf.seek(0)
        if sys.version_info > (3,):
            tf,tf_ = open(tf.fileno()), tf
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            ad_iter = classad.parseOldAds(tf)
        ad = next(ad_iter)
        self.assertEqual(len(ad), 1)
        self.assertEqual(ad["foo"], 1)
        self.assertEqual("bar = 2\n", tf.read())

    def test_parse_next(self):
        tf = tempfile.TemporaryFile()
        tf.write(b"[foo = 1] [bar = 2]")
        tf.seek(0)
        if sys.version_info > (3,):
            tf,tf_ = open(tf.fileno()), tf
        ad = classad.parseNext(tf)
        self.assertEqual(len(ad), 1)
        self.assertEqual(ad["foo"], 1)
        self.assertEqual(" [bar = 2]", tf.read())
        tf = tempfile.TemporaryFile()
        tf.write(b"-----\nfoo = 1\n\nbar = 2\n")
        tf.seek(0)
        if sys.version_info > (3,):
            tf,tf_ = open(tf.fileno()), tf
        ad = classad.parseNext(tf)
        self.assertEqual(len(ad), 1)
        self.assertEqual(ad["foo"], 1)
        self.assertEqual("bar = 2\n", tf.read())

    def new_ads_verify(self, ads):
        ads = list(ads)
        self.assertEqual(len(ads), 2)
        ad1, ad2 = ads
        self.assertEqual(ad1["foo"], "bar")
        self.assertEqual(ad1["baz"], classad.Value.Undefined)
        self.assertEqual(ad2["bar"], 1)
        self.assertEqual(len(ad1), 2)
        self.assertEqual(len(ad2), 1)
        self.assertRaises(KeyError, ad1.__getitem__, "bar")

    def old_ads_verify(self, ads):
        ads = list(ads)
        self.assertEqual(len(ads), 2)
        ad1, ad2 = ads
        self.assertEqual(ad1["MaxHosts"], 1)
        self.assertEqual(ad1["Managed"], "Schedd")
        self.assertEqual(ad2["User"], "bbockelm@users.opensciencegrid.org")
        self.assertEqual(ad2["SUBMIT_x509userproxy"], "/tmp/x509up_u1221")
        self.assertEqual(len(ad1), 2)
        self.assertEqual(len(ad2), 2)

    def test_load_classads(self):
        self.new_ads_verify(classad.parseAds(open("tests/test_multiple.ad")))
        self.new_ads_verify(classad.parseAds(open("tests/test_multiple.ad").read()))
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            self.old_ads_verify(classad.parseOldAds(open("tests/test_multiple.old.ad")))
            self.old_ads_verify(classad.parseOldAds(open("tests/test_multiple.old.ad").read()))
        self.old_ads_verify(classad.parseAds(open("tests/test_multiple.old.ad")))
        self.old_ads_verify(classad.parseAds(open("tests/test_multiple.old.ad").read()))

    def test_warnings(self):
        with warnings.catch_warnings(record=True) as w:
            warnings.simplefilter("always")
            classad.parseOld("foo = 1\nbar = 2")
            self.assertEqual(len(w), 1)
            self.assertTrue(issubclass(w[-1].category, DeprecationWarning))
            self.assertTrue("deprecated" in str(w[-1].message))

    def test_old_classad(self):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            ad = classad.parseOld(open("tests/test.old.ad"))
        contents = open("tests/test.old.ad").read()
        keys = []
        for line in contents.splitlines():
            info = line.split(" = ")
            if len(info) != 2:
                continue
            self.assertTrue(info[0] in ad)
            self.assertEqual(ad.lookup(info[0]).__repr__(), info[1])
            keys.append(info[0])
        for key in ad:
            self.assertTrue(key in keys)

    def test_old_classad_v2(self):
        ad = classad.parseNext(open("tests/test.old.ad"))
        contents = open("tests/test.old.ad").read()
        keys = []
        for line in contents.splitlines():
            info = line.split(" = ")
            if len(info) != 2:
                continue
            self.assertTrue(info[0] in ad)
            self.assertEqual(ad.lookup(info[0]).__repr__(), info[1])
            keys.append(info[0])
        for key in ad:
            self.assertTrue(key in keys)

    def test_exprtree(self):
        ad = classad.ClassAd()
        ad["foo"] = classad.ExprTree("2+2")
        expr = ad["foo"]
        self.assertEqual(expr.__repr__(), "2 + 2")
        self.assertEqual(expr.eval(), 4)

    def test_exprtree_func(self):
        ad = classad.ClassAd()
        ad["foo"] = classad.ExprTree('regexps("foo (bar)", "foo bar", "\\\\1")')
        self.assertEqual(ad.eval("foo"), "bar")

    def test_list_conversion(self):
        ad = dict(classad.ClassAd("[a = {1,2,3}]"))
        self.assertTrue(isinstance(ad["a"], list))
        self.assertTrue(isinstance(ad["a"][0], _long_type))
        def listAdd(a, b): return a+b
        classad.register(listAdd)
        self.assertEqual(classad.ExprTree("listAdd({1,2}, {3,4})")[0], 1)

    def test_dict_conversion(self):
        ad = classad.ClassAd({'a': [1,2, {}]})
        dict_ad = dict(ad)
        self.assertTrue(isinstance(dict_ad["a"][2], dict))
        self.assertEqual(classad.ClassAd(dict_ad).__repr__(), "[ a = { 1,2,[  ] } ]")
        ad = classad.ClassAd("[a = [b = {1,2,3}]]")
        inner_list = dict(ad)['a']['b']
        self.assertTrue(isinstance(inner_list, list))
        self.assertTrue(isinstance(inner_list[0], _long_type))
        self.assertTrue(isinstance(ad['a'], dict))

    def test_ad_assignment(self):
        ad = classad.ClassAd()
        ad["foo"] = 2.1
        self.assertEqual(ad["foo"], 2.1)
        ad["foo"] = 2
        self.assertEqual(ad["foo"], 2)
        ad["foo"] = "bar"
        self.assertEqual(ad["foo"], "bar")
        self.assertRaises(TypeError, ad.__setitem__, {})

    def test_ad_refs(self):
        ad = classad.ClassAd()
        ad["foo"] = classad.ExprTree("bar + baz")
        ad["bar"] = 2.1
        ad["baz"] = 4
        self.assertEqual(ad["foo"].__repr__(), "bar + baz")
        self.assertEqual(ad.eval("foo"), 6.1)

    def test_ad_special_values(self):
        ad = classad.ClassAd()
        ad["foo"] = classad.ExprTree('regexp(12, 34)')
        ad["bar"] = classad.Value.Undefined
        self.assertEqual(ad["foo"].eval(), classad.Value.Error)
        self.assertNotEqual(ad["foo"].eval(), ad["bar"])
        self.assertEqual(classad.Value.Undefined, ad["bar"])

    def test_ad_iterator(self):
        ad = classad.ClassAd()
        ad["foo"] = 1
        ad["bar"] = 2
        self.assertEqual(len(ad), 2)
        self.assertEqual(len(list(ad)), 2)
        self.assertEqual(list(ad)[1], "foo")
        self.assertEqual(list(ad)[0], "bar")
        self.assertEqual(list(ad.items())[1][1], 1)
        self.assertEqual(list(ad.items())[0][1], 2)
        self.assertEqual(list(ad.values())[1], 1)
        self.assertEqual(list(ad.values())[0], 2)

    def test_ad_lookup(self):
        ad = classad.ClassAd()
        ad["foo"] = classad.Value.Error
        self.assertTrue(isinstance(ad.lookup("foo"), classad.ExprTree))
        self.assertEqual(ad.lookup("foo").eval(), classad.Value.Error)

    def test_get(self):
        ad = classad.ClassAd()
        self.assertEqual(ad.get("foo"), None)
        self.assertEqual(ad.get("foo", "bar"), "bar")
        ad["foo"] = "baz"
        self.assertEqual(ad.get("foo"), "baz")
        self.assertEqual(ad.get("foo", "bar"), "baz")

    def test_setdefault(self):
        ad = classad.ClassAd()
        self.assertEqual(ad.setdefault("foo", "bar"), "bar")
        self.assertEqual(ad.get("foo"), "bar")
        ad["bar"] = "baz"
        self.assertEqual(ad.setdefault("bar", "foo"), "baz")

    def test_update(self):
        ad = classad.ClassAd()
        ad.update({"1": 2})
        self.assertTrue("1" in ad)
        self.assertEqual(ad["1"], 2)
        ad.update([("1",3)])
        self.assertEqual(ad["1"], 3)
        other = classad.ClassAd({"3": "5"})
        ad.update(other)
        del other
        self.assertTrue("3" in ad)
        self.assertEqual(ad["3"], "5")

    def test_invalid_ref(self):
        expr = classad.ExprTree("foo")
        self.assertEqual(classad.Value.Undefined, expr.eval())

    def test_temp_scope(self):
        expr = classad.ExprTree("foo")
        self.assertEqual("bar", expr.eval({"foo": "bar"}))
        ad = classad.ClassAd({"foo": "baz", "test": classad.ExprTree("foo")})
        expr = ad["test"]
        self.assertEqual("baz", expr.eval())
        self.assertEqual("bar", expr.eval({"foo": "bar"}))
        self.assertEqual("bar", expr.eval({"foo": "bar"}))
        self.assertEqual("baz", expr.eval())

    def test_abstime(self):
        expr = classad.ExprTree('absTime("2013-11-12T07:50:23")')
        dt = expr.eval()
        self.assertTrue(isinstance(dt, datetime.datetime))
        self.assertEqual(dt.year, 2013)
        self.assertEqual(dt.month, 11)
        self.assertEqual(dt.day, 12)
        self.assertEqual(dt.hour, 7)
        self.assertEqual(dt.minute, 50)
        self.assertEqual(dt.second, 23)

        ad = classad.ClassAd({"foo": dt})
        dt2 = ad["foo"]
        self.assertTrue(isinstance(dt2, datetime.datetime))
        self.assertEqual(dt, dt2)

        ad = classad.ClassAd({"foo": datetime.datetime.now()});
        td = (datetime.datetime.now()-ad["foo"])
        self.assertEqual(td.days, 0)
        self.assertTrue(td.seconds < 300)

    def test_reltime(self):
        expr = classad.ExprTree('relTime(5)')
        self.assertEqual(expr.eval(), 5)

    def test_quote(self):
        self.assertEqual(classad.quote("foo"), '"foo"')
        self.assertEqual(classad.quote('"foo'), '"\\"foo"')
        for i in ["foo", '"foo', '"\\"foo']:
            self.assertEqual(i, classad.unquote(classad.quote(i)))

    def test_literal(self):
        self.assertEqual(classad.ExprTree('"foo"'), classad.Literal("foo"))
        self.assertEqual(classad.Literal(1).eval(), 1)

    def test_operator(self):
        expr = classad.Literal(1) + 2
        self.assertTrue(isinstance(expr, classad.ExprTree))
        self.assertTrue(expr)
        self.assertTrue(expr.sameAs(classad.ExprTree('1 + 2')))
        expr = classad.Literal(1) & 2
        self.assertTrue(isinstance(expr, classad.ExprTree))
        self.assertEqual(expr.eval(), 0)
        self.assertTrue(expr.sameAs(classad.ExprTree('1 & 2')))
        expr = classad.Attribute("foo").is_(classad.Value.Undefined)
        self.assertTrue(expr.eval())
        ad = classad.ClassAd("[foo = 1]")
        expr = classad.Attribute("foo").isnt_(classad.Value.Undefined)
        self.assertTrue(expr.eval(ad))
        expr = classad.Literal(1).and_( classad.Literal(2) )
        self.assertRaises(RuntimeError, bool, expr)

    def test_subscript(self):
        ad = classad.ClassAd({'foo': [0,1,2,3]})
        expr = classad.Attribute("foo")._get(2)
        self.assertTrue(isinstance(expr, classad.ExprTree))
        self.assertEqual(expr.eval(), classad.Value.Undefined)
        self.assertEqual(expr.eval(ad), 2)

    def test_function(self):
        expr = classad.Function("strcat", "hello", " ", "world")
        self.assertTrue(isinstance(expr, classad.ExprTree))
        self.assertEqual(expr.eval(), "hello world")
        expr = classad.Function("regexp", ".*")
        self.assertEqual(expr.eval(), classad.Value.Error)

    def test_flatten(self):
        expr = classad.Attribute("foo") == classad.Attribute("bar")
        ad = classad.ClassAd({"bar": 1})
        self.assertTrue(ad.flatten(expr).sameAs( classad.ExprTree('foo == 1') ))

    def test_matches(self):
        left = classad.ClassAd('[requirements = other.foo == 3; bar=1]')
        right = classad.ClassAd('[foo = 3]')
        right2 = classad.ClassAd('[foo = 3; requirements = other.bar == 1;]')
        self.assertFalse(left.matches(right))
        self.assertTrue(right.matches(left))
        self.assertFalse(right.symmetricMatch(left))
        self.assertTrue(left.matches(right2))
        self.assertTrue(right2.symmetricMatch(left))

    def test_bool(self):
        self.assertTrue(bool( classad.ExprTree('true || false') ))
        self.assertTrue(bool( classad.Literal(True).or_(False) )) 
        self.assertFalse(bool( classad.ExprTree('true && false') ))
        self.assertFalse(bool( classad.Literal(True).and_(False) ))

    def test_register(self):
        class BadException(Exception): pass
        def myAdd(a, b): return a+b
        def myBad(a, b): raise BadException("bad")
        def myComplex(a): return 1j # ClassAds have no complex numbers, not able to convert from python to an expression
        def myExpr(**kw): return classad.ExprTree("foo") # Functions must return values; this becomes "undefined".
        def myFoo(foo): return foo['foo']
        def myIntersect(a, b): return set(a).intersection(set(b))
        classad.register(myAdd)
        classad.register(myAdd, name='myAdd2')
        classad.register(myBad)
        classad.register(myComplex)
        classad.register(myExpr)
        classad.register(myFoo)
        classad.register(myIntersect)
        self.assertEqual(3, classad.ExprTree('myAdd(1, 2)').eval())
        self.assertEqual(3, classad.ExprTree('myAdd2(1, 2)').eval())
        self.assertRaises(BadException, classad.ExprTree('myBad(1, 2)').eval)
        self.assertRaises(TypeError, classad.ExprTree('myComplex(1)').eval)
        self.assertEqual(classad.Value.Undefined, classad.ExprTree('myExpr()').eval())
        self.assertEqual(classad.ExprTree('myExpr()').eval({"foo": 2}), 2)
        self.assertRaises(TypeError, classad.ExprTree('myAdd(1)').eval) # myAdd requires 2 arguments; only one is given.
        self.assertEqual(classad.ExprTree('myFoo([foo = 1])').eval(), 1)
        self.assertEqual(classad.ExprTree('size(myIntersect({1, 2}, {2, 3}))').eval(), 1)
        self.assertEqual(classad.ExprTree('myIntersect({1, 2}, {2, 3})[0]').eval(), 2)

    def test_state(self):
        def myFunc(state): return 1 if state else 0
        classad.register(myFunc)
        self.assertEqual(0, classad.ExprTree('myFunc(false)').eval())
        self.assertEqual(1, classad.ExprTree('myFunc("foo")').eval())
        ad = classad.ClassAd("""[foo = myFunc(); bar = 2]""")
        self.assertEqual(1, ad.eval('foo'))
        ad['foo'] = classad.ExprTree('myFunc(1)')
        self.assertRaises(TypeError, ad.eval, ('foo',))
        def myFunc(arg1, **kw): return kw['state']['bar']
        classad.register(myFunc)
        self.assertEqual(2, ad.eval('foo'))

    def test_refs(self):
        ad = classad.ClassAd({"bar": 2})
        expr = classad.ExprTree("foo =?= bar")
        self.assertEqual(ad.externalRefs(expr), ["foo"])
        self.assertEqual(ad.internalRefs(expr), ["bar"])

    def test_cast(self):
        self.assertEqual(4, int(classad.ExprTree('1+3')))
        self.assertEqual(4.5, float(classad.ExprTree('1.0+3.5')))
        self.assertEqual(34, int(classad.ExprTree('strcat("3", "4")')))
        self.assertEqual(34.5, float(classad.ExprTree('"34.5"')))
        self.assertRaises(ValueError, float, classad.ExprTree('"34.foo"'))
        self.assertRaises(ValueError, int, classad.ExprTree('"12 "'))
        ad = classad.ClassAd("""[foo = 2+5; bar = foo]""")
        expr = ad['bar']
        self.assertEqual(7, int(expr))
        self.assertEqual(7, int(ad.lookup('bar')))
        self.assertEqual(0, int(classad.ExprTree('false')))
        self.assertEqual(0.0, float(classad.ExprTree('false')))
        self.assertEqual(1, int(classad.ExprTree('true')))
        self.assertEqual(1.0, float(classad.ExprTree('true')))
        self.assertEqual(3, int(classad.ExprTree('3.99')))
        self.assertEqual(3.0, float(classad.ExprTree('1+2')))
        self.assertRaises(ValueError, int, classad.ExprTree('undefined'))
        self.assertRaises(ValueError, float, classad.ExprTree('error'))
        self.assertRaises(ValueError, float, classad.ExprTree('foo'))

    def test_pipes(self):
        # One regression we saw in the ClassAd library is the new
        # parsing routines would fail if tell/seek was non-functional.
        r, w = os.pipe()
        rfd = os.fdopen(r, 'r')
        wfd = os.fdopen(w, 'w')
        wfd.write("[foo = 1]")
        wfd.close()
        ad = classad.parseNext(rfd ,parser=classad.Parser.New)
        self.assertEqual(tuple(dict(ad).items()), (('foo', 1),))
        self.assertRaises(StopIteration, classad.parseNext, rfd, classad.Parser.New)
        rfd.close()
        r, w = os.pipe()
        rfd = os.fdopen(r, 'r')
        wfd = os.fdopen(w, 'w')
        wfd.write("[foo = 1]")
        wfd.close()
        self.assertRaises(ValueError, classad.parseNext, rfd)
        rfd.close()

if __name__ == '__main__':
    unittest.main()

