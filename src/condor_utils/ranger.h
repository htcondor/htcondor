#ifndef __RANGER_H__
#define __RANGER_H__

#include <set>
#include <string>

template <class T>
struct ranger {
    struct range;
    struct elements;
    typedef T                                    element_type;
    typedef std::set<range>                      forest_type;
    typedef typename forest_type::const_iterator iterator;

    ranger() {};
    ranger(const std::initializer_list<range> &il);
    ranger(const std::initializer_list<element_type> &il);

    iterator insert(range r);
    iterator erase(range r);

    iterator insert(element_type e) { return insert(range(e, e + 1)); }
    iterator erase(element_type e)  { return erase(range(e, e + 1));  }

    iterator insert_slice(element_type front, element_type back)
    { return insert(range(front, back + 1)); }

    iterator erase_slice(element_type front, element_type back)
    { return erase(range(front, back + 1)); }

    iterator lower_bound(element_type x) const;
    iterator upper_bound(element_type x) const;

    std::pair<iterator, bool> find(element_type x) const;

    bool contains(element_type x) const { return find(x).second; }
    bool empty()                  const { return forest.empty(); }
    size_t size()                 const { return forest.size(); }
    void clear()                        { forest.clear(); }

    iterator begin() const { return forest.begin(); }
    iterator end()   const { return forest.end(); }

    elements get_elements() const { return *this; }

    // first/final range/element; do not call if empty()
    const range &front()         const { return *begin(); }
    const range &back()          const { return *--end(); }
    element_type front_element() const { return front().front(); }
    element_type back_element()  const { return back().back(); }




/*  persist / load ranger objects
 *
 *  The serialized format is one or more sub-ranges, separated by semicolons,
 *  where each sub-range is either N-M (for inclusive N..M) or N for a single
 *  element.  Eg, "2", "5-10", "4;7;10-20;44;50-60"
 */

    void persist(std::string &s) const;
    void persist_range(std::string &s, const range &rr) const;
    void persist_slice(std::string &s, element_type start,
                                       element_type back) const;

    // insert sub-ranges serialized in s
    // return 0 on success, (-1 - (position in string)) on parse failure
    int load(const char *s);

    private:
    // the state of our ranger
    forest_type forest;
};

template <class T>
struct ranger<T>::range {
    struct iterator;
    typedef typename ranger::element_type value_type;

    range(value_type e) : _start(), _end(e) {}
    range(value_type s, value_type e) : _start(s), _end(e) {}

    value_type front()            const { return _start;   }
    value_type back()             const { return _end - 1; }

    bool contains(value_type x)   const { return !(x < _start) && x < _end; }
    bool contains(const range &r) const
    { return !(r._start < _start) && r._end < _end; }

    // only for use in our disjoint ranger forest context
    bool operator< (const range &r2) const { return _end < r2._end; }

    iterator begin() const { return _start; }
    iterator end()   const { return _end;   }

    // data members; a valid range in ranger forest context has _start < _end
    mutable value_type _start;
    mutable value_type _end;
};

template <class T>
struct ranger<T>::range::iterator {
    typedef typename ranger::element_type value_type;

    iterator() : i() {}
    iterator(value_type n) : i(n) {}

    value_type  operator*()             const {      return i;     }
    iterator    operator+(int n)        const {      return i+n;   }
    iterator    operator-(int n)        const {      return i-n;   }
    iterator   &operator++()                  { ++i; return *this; }
    iterator   &operator--()                  { --i; return *this; }

    bool operator==(const iterator &b)  const { return i == b.i;   }
    bool operator!=(const iterator &b)  const { return !(i==b.i);  }

    // this is both the iterator "position" and the value
    value_type i;
};

template <class T>
struct ranger<T>::elements {
    struct iterator;
    typedef typename ranger::element_type value_type;

    elements(const ranger &r) : r(r) {}

    iterator begin() const { return r.begin(); }
    iterator end()   const { return r.end();   }

    const ranger &r;
};

template <class T>
struct ranger<T>::elements::iterator {
    iterator(typename ranger::iterator si) : sit(si), rit_valid(0) {}
    iterator() : rit_valid(0) {}

    value_type operator*();
    iterator &operator++();
    iterator &operator--();
    bool operator==(iterator &it);
    bool operator!=(iterator &it);

    private:
    void mk_valid();

    typename ranger::iterator sit;
    typename range::iterator rit;
    bool rit_valid;
};


#endif
