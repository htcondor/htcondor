#include "condor_common.h"

#include <algorithm>
#include "ranger.h"


///// ranger insert/erase/find implementation /////


ranger::iterator ranger::insert(ranger::range r)
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

    if (it_start->contains(r))
        return it_start;

    iterator it_back = --it;
    range rr_new = { std::min(it_start->_start, r._start),
                     std::max(it_back->_end, r._end) };

    // avoid erase+insert if only expanding a range
    if (it_start == it_back) {
        it_start->_start = rr_new._start;
        it_start->_end = rr_new._end;
        return it_start;
    }

    iterator hint = forest.erase(it_start, it_end);

    return forest.insert(hint, rr_new);
}

ranger::iterator ranger::erase(ranger::range r)
{
    iterator it_start = upper_bound(r._start);
    iterator it = it_start;
    while (it != end() && it->_start < r._end)
        ++it;

    iterator it_end = it;
    if (it_start == it_end)
        return it_start;

    iterator it_back = --it;
    range rr_back = *it_back;

    if (it_start->_start < r._start && r._end < it_start->_end) {
        // split a single range
        it_start->_end = r._start;
        return forest.insert(it_end, range(r._end, rr_back._end));
    }

    if (it_start->_start < r._start) {
        it_start->_end = r._start;
        ++it_start;
    }

    if (r._end < rr_back._end) {
        it_back->_start = r._end;
        --it_end;
    }

    return it_start == it_end ? it_end : forest.erase(it_start, it_end);
}

std::pair<ranger::iterator, bool>
ranger::find(element_type x) const
{
    iterator it = upper_bound(x);
    return {it, it != end() && it->_start <= x};
}

bool ranger::contains(element_type x) const
{
    return find(x).second;
}

ranger::ranger(const std::initializer_list<ranger::range> &il)
{
    for (const range &rr : il)
        insert(rr);
}


// specialize lower_bound / upper_bound based on forest type
static inline std::set<ranger::range>::const_iterator
lower_bounder(const std::set<ranger::range> &f, ranger::range rr)
{
    return f.lower_bound(rr);
}

static inline std::set<ranger::range>::const_iterator
upper_bounder(const std::set<ranger::range> &f, ranger::range rr)
{
    return f.upper_bound(rr);
}

static inline std::vector<ranger::range>::const_iterator
lower_bounder(const std::vector<ranger::range> &f, ranger::range rr)
{
    return std::lower_bound(f.begin(), f.end(), rr);
}

static inline std::vector<ranger::range>::const_iterator
upper_bounder(const std::vector<ranger::range> &f, ranger::range rr)
{
    return std::upper_bound(f.begin(), f.end(), rr);
}


ranger::iterator ranger::lower_bound(element_type x) const
{
    return lower_bounder(forest, x);
}

ranger::iterator ranger::upper_bound(element_type x) const
{
    return upper_bounder(forest, x);
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

void ranger::elements::iterator::mk_valid()
{
    if (!rit_valid) {
        rit = sit->begin();
        rit_valid = true;
    }
}

ranger::element_type ranger::elements::iterator::operator*()
{
    mk_valid();
    return *rit;
}

ranger::elements::iterator &ranger::elements::iterator::operator++()
{
    mk_valid();
    if (++rit == sit->end()) {
        ++sit;
        rit_valid = false;
    }
    return *this;
}

ranger::elements::iterator &ranger::elements::iterator::operator--()
{
    mk_valid();
    if (rit == sit->begin()) {
        --sit;
        rit = sit->end();
        --rit;
    }
    return *this;
}

bool ranger::elements::iterator::operator==(iterator &it)
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

bool ranger::elements::iterator::operator!=(iterator &it)
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
 *  integer.  Eg, "2", "5-10", "4;7;10-20;44;50-60"
 */

static
void persist_range_single(std::string &s, const ranger::range &rr)
{
    char buf[64];
    int n;
    if (rr._start == rr._end - 1)
        n = sprintf(buf, "%d;", rr._start);
    else
        n = sprintf(buf, "%d-%d;", rr._start, rr._end - 1);
    s.append(buf, n);
}

void persist(std::string &s, const ranger &r)
{
    s.clear();
    if (r.empty())
        return;

    for (auto &rr : r)
        persist_range_single(s, rr);

    s.erase(s.size() - 1);
}

void persist_range(std::string &s, const ranger &r, const ranger::range &rr)
{
    s.clear();
    if (r.empty())
        return;

    auto rit = r.find(rr._start).first;
    for (; rit != r.end() && rit->_start < rr._end; ++rit) {
        ranger::range rr_new = { std::max(rit->_start, rr._start),
                                 std::min(rit->_end,   rr._end) };
        persist_range_single(s, rr_new);
    }

    s.erase(s.size() - 1);
}

void persist_slice(std::string &s, const ranger &r, int start, int back)
{
    persist_range(s, r, {start, back+1});
}

// return 0 on success, (-1 - (position in string)) on parse failure
int load(ranger &r, const char *s)
{
    const char *sstart = s;
    while (*s) {
        char *sp;
        int start = strtol(s, &sp, 10);
        int back;
        if (s == sp)
            // no int parsed is ok as long as we're at the end
            return *s ? -1 - int(s - sstart) : 0;
        s = sp;
        if (*sp == '-') {
            s++;
            back = strtol(s, &sp, 10);
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
        r.insert({start, back + 1});
    }
    return 0;
}

