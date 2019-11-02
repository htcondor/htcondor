#ifndef __RANGER_H__
#define __RANGER_H__

#include <set>

typedef struct ranger range_mask;

struct ranger {
    struct range;
    struct elements;
    typedef int                         element_type;
    typedef std::set<range>             forest_type;
    typedef forest_type::const_iterator iterator;

    ranger() {};
    ranger(const std::initializer_list<range> &il);

    iterator insert(range r);
    iterator erase(range r);

    iterator lower_bound(element_type x) const;
    iterator upper_bound(element_type x) const;

    std::pair<iterator, bool> find(element_type x) const;

    bool contains(element_type x) const;
    bool empty()                const { return forest.empty(); }
    size_t size()               const { return forest.size(); }
    void clear()                      { forest.clear(); }

    inline iterator begin() const;
    inline iterator end()   const;

    inline elements get_elements() const;

    // first/final range/element; do not call if empty()
    inline const range &front()         const;
    inline const range &back()          const;
    inline element_type front_element() const;
    inline element_type back_element()  const;

    private:
    // the state of our ranger
    forest_type forest;
};

struct ranger::range {
    struct iterator;
    typedef ranger::element_type value_type;

    range(value_type e) : _start(0), _end(e) {}
    range(value_type s, value_type e) : _start(s), _end(e) {}

    value_type back()             const { return _end - 1; }
    value_type size()             const { return _end - _start; }
    bool contains(value_type x)   const { return _start <= x && x < _end; }
    bool contains(const range &r) const
    { return _start <= r._start && r._end < _end; }

    // only for use in our disjoint ranger forest context
    bool operator< (const range &r2) const { return _end < r2._end; }

    inline iterator begin() const;
    inline iterator end()   const;

    // data members; a valid range in ranger forest context has _start < _end
    mutable value_type _start;
    mutable value_type _end;
};

struct ranger::range::iterator {
    typedef ranger::element_type value_type;

    iterator() : i(0) {}
    iterator(value_type n) : i(n) {}

    value_type  operator*()             const {      return i;     }
    iterator    operator+(value_type n) const {      return i+n;   }
    iterator    operator-(value_type n) const {      return i-n;   }
    iterator   &operator++()                  { ++i; return *this; }
    iterator   &operator--()                  { --i; return *this; }
    iterator    operator++(int)               {      return i++;   }
    iterator    operator--(int)               {      return i--;   }

    // takes care of rel ops :D
    operator value_type()               const {      return i;     }

    // this is both the iterator "position" and the value
    value_type i;
};

struct ranger::elements {
    struct iterator;
    typedef ranger::element_type value_type;

    elements(const ranger &r) : r(r) {}

    inline iterator begin() const;
    inline iterator end()   const;

    const ranger &r;
};

struct ranger::elements::iterator {
    iterator(ranger::iterator si) : sit(si), rit_valid(0) {}
    iterator() : rit_valid(0) {}

    value_type operator*();
    iterator &operator++();
    iterator &operator--();
    bool operator==(iterator &it);
    bool operator!=(iterator &it);

    private:
    void mk_valid();

    ranger::iterator sit;
    range::iterator rit;
    bool rit_valid;
};


// these are inline but must appear after their return type definitions

ranger::elements           ranger::get_elements()    const { return *this; }

ranger::elements::iterator ranger::elements::begin() const { return r.begin(); }
ranger::elements::iterator ranger::elements::end()   const { return r.end();   }

ranger::range::iterator    ranger::range::begin()    const { return _start; }
ranger::range::iterator    ranger::range::end()      const { return _end;   }

ranger::iterator           ranger::begin()   const { return forest.begin(); }
ranger::iterator           ranger::end()     const { return forest.end();   }

const ranger::range &ranger::front()         const { return *begin();       }
ranger::element_type ranger::front_element() const { return front()._start; }

const ranger::range &ranger::back()          const { return *--end();       }
ranger::element_type ranger::back_element()  const { return back().back();  }


/*  persist / load ranger objects
 *
 *  The serialized format is one or more sub-ranges, separated by semicolons,
 *  where each sub-range is either N-M (for inclusive N..M) or N for a single
 *  integer.  Eg, "2", "5-10", "4;7;10-20;44;50-60"
 */

#include <string>

void persist(std::string &s, const ranger &r);
void persist_slice(std::string &s, const ranger &r, int start, int back);
void persist_range(std::string &s, const ranger &r, const ranger::range &rr);

// return 0 on success, (-1 - (position in string)) on parse failure
int load(ranger &r, const char *s);


#endif
