#include "condor_common.h"

#include <algorithm>
#include <utility>
#include "ranger.h"
#include "proc.h"

using namespace std::rel_ops;


///// ranger insert/erase/find implementation /////


template <class T>
auto ranger<T>::insert(range r) -> iterator
{
    // lower_bound here will coalesce an adjacent disjoint range;
    // can use upper_bound instead to avoid this and leave them fractured
    iterator it_start = lower_bound(r._start);
    iterator it = it_start;
    while (it != end() && it->_start <= r._end)  // '<' for no coalesce
        ++it;

    iterator it_end = it;
    if (it_start == it_end)
        return forest.insert(it_end, r);

    iterator it_back = --it;
    element_type min_start = std::min(it_start->_start, r._start);

    // update back of affected range in-place
    if (min_start < it_back->_start)
        it_back->_start = min_start;

    if (it_back->_end < r._end)
        it_back->_end = r._end;

    return it_start == it_back ? it_back : forest.erase(it_start, it_back);
}

template <class T>
auto ranger<T>::erase(range r) -> iterator
{
    iterator it_start = upper_bound(r._start);
    iterator it = it_start;
    while (it != end() && it->_start < r._end)
        ++it;

    iterator it_end = it;
    if (it_start == it_end)
        return it_start;

    iterator it_back = --it;
    range orig_back = *it_back;

    if (it_start->_start < r._start && r._end < it_start->_end) {
        // split a single range
        it_start->_end = r._start;
        return forest.insert(it_end, range(r._end, orig_back._end));
    }

    if (it_start->_start < r._start) {
        it_start->_end = r._start;
        ++it_start;
    }

    if (r._end < orig_back._end) {
        it_back->_start = r._end;
        --it_end;
    }

    return it_start == it_end ? it_end : forest.erase(it_start, it_end);
}

template <class T>
auto ranger<T>::find(element_type x) const -> std::pair<iterator, bool>
{
    iterator it = upper_bound(x);
    return {it, it != end() && it->_start <= x};
}

template <class T>
ranger<T>::ranger(const std::initializer_list<range> &il)
{
    for (const range &rr : il)
        insert(rr);
}

template <class T>
ranger<T>::ranger(const std::initializer_list<element_type> &il)
{
    for (const element_type &e : il)
        insert(e);
}


// use std::lower_bound for generic containers (other than std::set)
template <class Forest>
struct bounder {
    typedef typename Forest::value_type Range;
    typedef typename Forest::const_iterator iterator;

    static iterator lower_bound(const Forest &f, const Range &r) {
        return std::lower_bound(f.begin(), f.end(), r);
    }

    static iterator upper_bound(const Forest &f, const Range &r) {
        return std::upper_bound(f.begin(), f.end(), r);
    }
};


// specialize for std::set containers to use std::set::lower_bound
template <class Range>
struct bounder<std::set<Range> > {
    typedef std::set<Range> Forest;
    typedef typename Forest::const_iterator iterator;

    static iterator lower_bound(const Forest &f, const Range &r) {
        return f.lower_bound(r);
    }

    static iterator upper_bound(const Forest &f, const Range &r) {
        return f.upper_bound(r);
    }
};


template <class T>
auto ranger<T>::lower_bound(element_type x) const -> iterator
{
    return bounder<forest_type>::lower_bound(forest, x);
}

template <class T>
auto ranger<T>::upper_bound(element_type x) const -> iterator
{
    return bounder<forest_type>::upper_bound(forest, x);
}



///// ranger::elements::iterator implementation /////


/* This allows:
 *
 *      for (int i : myranger.get_elements())
 *          process_int(i);
 *
 *
 * Instead of the still-straightforward:
 *
 *      for (auto &rr : myranger)
 *          for (int i : rr)
 *              process_int(i);
 */

template <class T>
void ranger<T>::elements::iterator::mk_valid()
{
    if (!rit_valid) {
        rit = sit->begin();
        rit_valid = true;
    }
}

template <class T>
auto ranger<T>::elements::iterator::operator*() -> value_type
{
    mk_valid();
    return *rit;
}

template <class T>
auto ranger<T>::elements::iterator::operator++() -> iterator &
{
    mk_valid();
    if (++rit == sit->end()) {
        ++sit;
        rit_valid = false;
    }
    return *this;
}

template <class T>
auto ranger<T>::elements::iterator::operator--() -> iterator &
{
    mk_valid();
    if (rit == sit->begin()) {
        --sit;
        rit = sit->end();
        --rit;
    }
    return *this;
}

template <class T>
bool ranger<T>::elements::iterator::operator==(iterator &it)
{
    if (sit != it.sit)
        return false;
    if (!rit_valid && !it.rit_valid)
        return true;
    // at this point neither sit nor it.sit points to the end() of its set,
    // thus it's OK to call mk_valid() on each, which may dereference *sit
    mk_valid();
    it.mk_valid();
    return rit == it.rit;
}

template <class T>
bool ranger<T>::elements::iterator::operator!=(iterator &it)
{
    return !(*this == it);
}



///// ranger persist/load implementation /////


#include <stdlib.h> // strtol
#include <stdio.h>  // sprintf
#include <string>

/*  persist / load ranger objects
 *
 *  The serialized format is one or more sub-ranges, separated by semicolons,
 *  where each sub-range is either N-M (for inclusive N..M) or N for a single
 *  element.  Eg, "2", "5-10", "4;7;10-20;44;50-60"
 */


// define read_element & write_element for each desired element type

// extract element from string s, output to out, return pos after element
static const char *read_element(const char *s, int *out)
{
    char *ret;
    *out = strtol(s, &ret, 10);
    return ret;
}

// write 'in' to buf, return number of bytes written
static int write_element(int in, char *buf)
{
    return sprintf(buf, "%d", in);
}


static const char *read_element(const char *s, JOB_ID_KEY *out)
{
    int len, ret = sscanf(s, "%d.%d%n", &out->cluster, &out->proc, &len);
    return ret != 2 ? s : s + len;
}

static int write_element(JOB_ID_KEY in, char *buf)
{
    return sprintf(buf, "%d.%d", in.cluster, in.proc);
}


template <class T>
static
void persist_range_single(std::string &s, const typename ranger<T>::range &rr)
{
    char buf[64];
    int n;
    n = write_element(rr._start, buf);
    if (rr._start != rr._end - 1) {
        buf[n++] = '-';
        n += write_element(rr._end - 1, buf+n);
    }
    buf[n++] = ';';
    s.append(buf, n);
}

template <class T>
void ranger<T>::persist(std::string &s) const
{
    s.clear();
    if (empty())
        return;

    for (auto &rr : *this)
        persist_range_single<T>(s, rr);

    s.erase(s.size() - 1);
}

template <class T>
void ranger<T>::persist_range(std::string &s, const range &rr) const
{
    s.clear();
    if (empty())
        return;

    auto rit = find(rr._start).first;
    for (; rit != end() && rit->_start < rr._end; ++rit) {
        range rr_new = { std::max(rit->_start, rr._start),
                         std::min(rit->_end,   rr._end) };
        persist_range_single<T>(s, rr_new);
    }

    if (!s.empty())
        s.erase(s.size() - 1);
}

template <class T>
void ranger<T>::persist_slice(std::string &s, element_type start,
                                              element_type back) const
{
    persist_range(s, {start, back+1});
}

// return 0 on success, (-1 - (position in string)) on parse failure
template <class T>
int ranger<T>::load(const char *s)
{
    const char *sstart = s;
    while (*s) {
        element_type start, back;
        const char *sp = read_element(s, &start);
        if (s == sp)
            // no int parsed is ok as long as we're at the end
            return *s ? -1 - int(s - sstart) : 0;
        s = sp;
        if (*sp == '-') {
            s++;
            sp = read_element(s, &back);
            if (s == sp)
                // a number should have followed '-'
                return -1 - int(s - sstart);
            s = sp;
        } else {
            back = start;
        }
        if (*s == ';')
            s++;
        else if (*s)
            // expected either ';' or end of string
            return -1 - int(s - sstart);
        insert({start, back + 1});
    }
    return 0;
}



// need exactly one explicit template instantiation per desired element type:

template struct ranger<int>;
template struct ranger<JOB_ID_KEY>;

