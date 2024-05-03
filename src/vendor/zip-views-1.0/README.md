# C++20 Zip Views

This is a fork from the [ZipIterator](https://github.com/CommitThis/zip-iterator) repository, modified in order to:

- build with c++20
- implement zip and make it available on std::ranges and std::views.

List of changes and old readme can be found below.

## Disclaimer

If you have a [c++23 compiler](https://en.cppreference.com/w/cpp/compiler_support/23) that implements [std::views::zip](https://en.cppreference.com/w/cpp/ranges/zip_view), you should not use this library.

If you don't have a c++23 compiler, but you need an implementation of [std::views::zip](https://en.cppreference.com/w/cpp/ranges/zip_view) that works in your c++20 compiler, this is what you're looking for! 

## Usage

### Include

Just set the root of this repo as an include directory and you're good to go, or, using CMake:

```cmake

add_subdirectory(zip-views)

target_link_libraries(your_target PRIVATE zip-view)

```

Then, in your code:

```cpp
#include <zip_view.hpp>
```

If you want to inject the zip view into the std namespace to be compatible with c++23 zip view, you can do so by defining the macro `ZIP_VIEW_INJECT_STD_VIEWS_NAMESPACE` before including the header.

```cpp
#define ZIP_VIEW_INJECT_STD_VIEWS_NAMESPACE
#include <zip_view.hpp>
```

### Example

Given two views `a` and `b`, and a container `c`, you can zip `a` and `b` into `c` like this:

```cpp
#include <zip_view.hpp>

for (auto && [x, y] : c9::zip(a, b)) {
    c.push_back(x + z);
}
```

# List of Changes

[ZipIterator](https://github.com/CommitThis/zip-iterator) was coded in order to work with containers that defined `iterator` or `const_iterator` type that was a random access iterator.

I modified the old `zip_tuple.hpp` in order to work with any view, using the functions provided by the [c++20 ranges library](https://en.cppreference.com/w/cpp/ranges).

I also:
- added the possibility to inject the zip view into the std namespace to be compatible with c++23 zip view;
- added the possibility to include the repo as a subdirectory in a cmake project, and added an option to not build the tests;
- removed the `zip_two.hpp` file and its test;

Detailed edits can be found in the commit history of the repo.

# === OLD README ===

C++: Zip Iteration
==================
This document is licensed CC-BY-NC-SA 3.0, everything else is licensed under 
the MIT terms.

Python is one of my favourite languages. One of the features it has is the `zip` 
function, which allows for parallel iteration of arrays and I wondered if
something like that was possible in C++. While I haven't seen anything like this
before and I didn't really go to the trouble of researching this; I thought it 
would be an interesting exercise I could try and do myself.

In any case, using some modern C++ techniques we can do this reasonably easily.
Being C++, I am not terribly confident I've covered all of the various details
properly, and any feedback on this would be appreciated.

> While the value categories in C++ aren't too complicated, I have simplified
> the categories to an __lvalue__ (that which is named, and has a predictable
> address, e.g. `int x = 1`), and an __rvalue__ (that which does not, is a 
> temporary; is a new instance returned by a function e.g. `f(){ return 1; }`.

The Loop
--------
This was the motivating example, use structured bindings to unpack values from
two iterables in parallel.

    for (auto && [x, y] : zip(a, b)) {
        c.push_back(x + z);
    }

I wanted both `x` and `y` to be references and therefore modifiable, but only if
the respective zipped containers was passed in as non-const, and values
otherwise.

A range-based for loop executes functions over a range, using syntactic sugar to
do so. It requires that the range expression represents a suitable sequence, 
e.g. an array or object with `begin` and `end` functions available.

https://en.cppreference.com/w/cpp/language/range-for

Therefore the `zip` function ought to return such an object. It will be a 
temporary (it is an __rvalue__), but that is ok as it's lifetime is extended until
the end of the loop. The `begin` and `end` functions of this object must then
return suitable iterators. 

The `zip` call also needs to be able to handle arguments of different types,
and not require the type be supplied at use.

Finally, the `value_type` of the iterated sequence must be de-structurable,
which can be an array, tuple-like, or an object with non-static data members.

https://en.cppreference.com/w/cpp/language/structured_binding

So, three components will be needed:
* A utility function for users to call that handles type deduction;
* An object that meets the requirements for range-for that returns
* A suitable iterator that proxies the iterators for the passed in types.


The Utility Function
--------------------

    template <typename T, typename U>
    auto zip(T && t, U && u) {
        return zipper<T, U>{std::forward<T>(t), std::forward<U>(u)};
    }

Universal references are used to accept arguments of any type. This is not 
because we want to accept temporaries (which does appear to work), but because
we want to accept both const and non-const types. We could be more clever at
this point to be more strict with some more template trickery, but I think this
is good enough for this example.

In any case, we can see that the types of the arguments are deduced and are then
used as template arguments to the object that returns the iterators.


The Zipper
----------
I couldn't think of a great name for this, but basically is a really simple 
object that contains references (or values, largely depends on what the type
arguments are) to the objects to be iterated over, and returns our special
zip iterators.

    template <typename T, typename U>
    class zipper {
    public:
        using Iter1 = select_iterator_for<T>;
        using Iter2 = select_iterator_for<U>;

        using zip_type = zip_iterator<Iter1, Iter2>;

        template <typename V, typename W>
        zipper(V && a, W && b)
            : m_a{a}
            , m_b{b}
        {}

        auto begin() -> zip_type {
            return zip_type{std::begin(m_a), std::begin(m_b)};
        }
        auto end() -> zip_type {
            return zip_type{std::end(m_a), std::end(m_b)};
        }

    private:
        T m_a;
        U m_b;
    };

The only trickery here is making sure we select the create iterator type to be
used by the proxy. If we pass in a `std::vector<>&`, we want to use the 
`std::vector<>::iterator` type, however if it's a `std::vector<> const &` then
we need to select `std::vector<>::const_iterator`. A relatively simple use of
`std::conditional_t` solves this:

    template <typename T>
    using select_iterator_for = std::conditional_t<
        std::is_const_v<std::remove_reference_t<T>>, 
        typename std::decay_t<T>::const_iterator,
        typename std::decay_t<T>::iterator>;

This selects the correct iterator type. Basically, if what's passed in is const,
the `const_iterator` is selected and otherwise `iterator`. The reason why we 
`std::decay_t` is used is because the types are references, and you can't access
their type definitions as it's considered an incomplete type. `std::decay_t`
returns it's fundamental type and solves this issue.

> As per https://en.cppreference.com/w/cpp/types/is_const, the reference needs 
> to be removed from the original type, although I'm not sure why!


The Zip Iterator
----------------
The zip iterator holds two iterators for the actual type being iterated over,
and proxies their usual functions in tandem. 

    template <typename Iter1, typename Iter2>
    class zip_iterator
    {
    public:
        using value_type = std::pair<
            select_access_type_for<Iter1>,
            select_access_type_for<Iter2>
        >;

        zip_iterator() = delete;

        zip_iterator(Iter1 iter_1_begin, Iter2 iter_2_begin)
            : m_iter_1_begin {iter_1_begin}
            , m_iter_2_begin {iter_2_begin}
        {}

        auto operator++() -> zip_iterator& {
            ++m_iter_1_begin;
            ++m_iter_2_begin;
            return *this;
        }

        auto operator++(int) -> zip_iterator {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        auto operator!=(zip_iterator const & other) {
            return !(*this == other);
        }

        auto operator==(zip_iterator const & other) {
            return 
                m_iter_1_begin == other.m_iter_1_begin ||
                m_iter_2_begin == other.m_iter_2_begin;
        }

        auto operator*() -> value_type {
            return value_type{*m_iter_1_begin, *m_iter_2_begin};
        }

    private:
        Iter1 m_iter_1_begin;
        Iter2 m_iter_2_begin;
    };

Again, fairly straight forward. The iterator is constructed from two iterators,
which are incremented whenever the iterator itself is incremented. It's not 
really a proper iterator in the sense that it doesn't meet the minimum
requirements for such (e.g. doesn't have a category), but it is a minimal 
example that works.


The comparison we need to make in order to detect when iteration should end is
when any of the iterators equal their end position. This isn't strictly correct
from a semantic point of view -- it returns true as soon as any of the contained
iterators equal their respective pair. This is necessary as the distance between
the starting points of either may be different, and then they would never be
equal at the same time leading to dereferencing past the end, and the loop never
terminating.

De-referencing the iterator returns a `std::pair` containing either references
or values corresponding to the internal de-referenced iterators. 

The consideration here is what the access type is when the internal iterators 
are is de-referenced. In most cases this should be a reference, but this is not
always possible. For example, `std::vector<bool>` is a special case in that it
doesn't actually store `bool`s as you might think. It does some special bit
twiddling to save on space (although arguably this is implementation defined).
A normal iterator of such is incapable of returning a reference, and
de-referencing returns a value.

    template <typename Iter>
    using select_access_type_for = std::conditional_t<
        std::is_same_v<Iter, std::vector<bool>::iterator> ||
        std::is_same_v<Iter, std::vector<bool>::const_iterator>,
        typename Iter::value_type,
        typename Iter::reference
    >;

The access type selection is there because I was originally zipping a vector of 
`int`s and a vector of `bool`s. `std::vector` has a specialisation for `bool`
that means that the value can't be accessed by reference, but instead by value.
This is obviously very specific for a `std::vector`, but this could either be
extended or replaced and I think works quite well to demonstrate how the access
type could be changed. This could, however,  be surprising for users.


What Is The Loop Actually Doing?
--------------------------------
* First, the `zip` function is called that deduces the passed in typed which are
  then used to create:
* The `zipper` object. This contains either a reference or value to what's been
  passed in and has `begin` and `end` functions which the range-for uses to
  generate a loop;
* On each pass of the loop, the iterator from `begin` is dereferenced and 
  assigned to the value on the left hand side (the "range declaration") and 
  finally incremented;
* The range declaration is a structured binding, and the temporary `std::pair`
  from the dereferenced iterator is unpacked into it.


There is also some strangeness going on with the structured bindings. If one of
the destructured variables is a reference, then a const qualification will have
effect. If it is a value (e.g. in the case of `std::vector<bool>`), then the
const qualifier is applied.

Further, the binding will fail if it is qualified as a non-const __lvalue__
reference.

The Story So Far...
-------------------
In any case, at this point, we should be able to iterate in parallel over two
containers:

    auto a = std::vector<int>{1, 2, 3, 4, 5, 6};
    auto const & b = a;
    auto c = std::vector<int>{};

    for (auto && [x, y] : zip(a, b)) {
        c.push_back(x + y);
    }

This works as you might expect, `x` and `y` are either const or non-const
references. What's curious here is how the structured binding is used.
Ideally, if you wanted to iteration and wanted to enforce const on any of the 
values you might write something like:

    for (auto const & [x, y] : zip(a, b)) ...

The `const` in this case refers to the type being unpacked, which is a temporary
`std::pair`. This will have no affect whatsoever on the actual types it 
contains. If you want to enforce this, you would need to make sure that the
passed in type (`a` or `b`) are actually const references.

Additionally, because the returned type is a temporary, you will never be able 
to bind it to an __lvalue__ reference.

But what about.....


Iterating Over More Than Two Types
==================================
This is slightly more complicated than the previous example. I won't go to the
trouble of repeating what i've said earlier, however the full source can be
found on ########

The utility function is modified to take a variable amount of arguments:

    template <typename ... T>
    auto zip(T && ... t) {
        return zipper<T ...>{std::forward<T>(t)...};
    }

In the zipper class, instead of storing the container/references in named 
members, we can use a tuple instead.

    template <typename ... Args>
    class zipper {
    public:
        using zip_type = zip_iterator<select_iterator_for<T> ...>;

    // ... snip ...

    private:
        std::tuple<T...> m_args;
    };

As for creating the iterator through `begin` and `end`, we have to find some way
of unpacking the tuple in order to call these functions on the underlying types.

C++17 has a utility function that can do this: `std::apply`. It takes a lambda
expression as the first argument (which in itself takes as arguments the values
of the tuple) and the tuple as the second:

    std::apply([](auto && ... args){ /* more magic here */ }, my_tuple);

The arguments can then be expanded as an ordinary parameter pack's using
`std::begin(args)...`:

    auto begin() -> zip_type {
        return std::apply([](auto && ... args){ 
                return zip_type(std::begin(args)...); 
            }, m_args);
    }

So, basically what's going on is that the tuple is being turned into a 
parameter pack that's expanded in the constructor for our custom iterator.

With respect to the zip iterator we use a tuple member to store the iterators,
and use the same technique to provide `operator*` and `operator++`:

    template <typename ... Iters>
    class zip_iterator
    {
    public:

        // ... snip ...

        auto operator++() -> zip_iterator& {
            std::apply([](auto && ... args){ ((args += 1), ...); }, m_iters);
            return *this;
        }

        auto operator*() -> value_type {
            return std::apply([](auto && ... args){ 
                    return value_type(*args...); 
                }, m_iters);
        }
    };

The use of this technique with `operator*` is exactly the same, only we're 
de-referencing the internal iterators and unpacking the results into yet another
tuple. With `operator++`, we're not returning anything, and just applying the
same operator to the contained iterators.

Finally, the equivalence operator is a special case. In a twist of fate, we also
need to do parallel iteration on all of the iterators! We can't use `std::apply`
as, as far as I can tell, only works on one tuple at a time. With a somewhat 
terrifying set of template functions:

    template <typename ... Args, std::size_t ... Index>
    auto any_match_impl(
        std::tuple<Args...> const & lhs,
        std::tuple<Args...> const & rhs,
        std::index_sequence<Index...>) -> bool
    {
        auto result = false;
        result = (... | (std::get<Index>(lhs) == std::get<Index>(rhs)));
        return result;
    }

    template <typename ... Args>
    auto any_match(
        std::tuple<Args...> const & lhs, 
        std::tuple<Args...> const & rhs) -> bool
    {
        return any_match_impl(lhs, rhs, std::index_sequence_for<Args...>{});
    }

The second function is a helper to call the actual implementation, so that a
sequence of integers is generated with respect to the size of the template
arguments.

> At this point, both the tuples are the same type and therefore have exactly
> the same size.

In the implementation function, a fold expression is used to expand the index
sequence; comparing the type instances of each tuple at the same index, which is
aggregated into the result. This was probably the most difficult thing I had to
deal with throughout; it wasn't an expansion I was at all familiar with, and I 
apologise in advance if I haven't explained it properly!


Conclusion
==========
Hopefully, I have demonstrated how parallel iteration over multiple types is
possible. We can now quite happily do something like this (observe the varying
sizes!):

    auto a = std::vector<int>{1, 2, 3, 4, 5, 6};
    auto b = std::vector<int>{1, 2, 3, 4, 5, 6, 7};
    auto c = std::vector<int>{0, 0, 0, 0, 0};
    auto const & d = b;

    for (auto && [x, y, z] : c9::zip(a, d, c)) {
        z = x + y;
    }

    auto expected = std::vector<int>{2, 4, 6, 8, 10};
    assert(c == expected);

There are two ways I can currently see that could be improved:
* Making the iterators have knowledge of their end position. This could be used
  to make the equality operation semantically correct in cases (which I can't 
  see as being useful) and also help with runtime safety;
* One of the concepts of C++ is flexible operations over data; you may want to
  "zip" across ranges within containers, that aren't either at the begin or end
  positions. In which case you might want a `zip` signature that looks like 
  `zip(begin_a, end_a, begin_b, end_b, ...)`. This would be easy for two sets of
  data, but might be more complicated for the variadic case.