
#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

//
// Code for a generic hash table. The hash table provides a mapping from
// values in the Key class to values in the Value class. The constructor
// takes the following arguments:
//
// - numBuckets: number of buckets in the hash table. Should be
//   a prime number.
// - hashFn: function to map a Key value to a bucket number
// - compFn: function to compare two Keys
// - keyCopyFn: function to copy Keys. This is an optional argument.
//   If it is not specified, then the C assignment operator is used
//   to copy Keys.
// - deleteKeys: if true, then Key values will be deleted as entries
//   are removed.
//
//

#include <stdlib.h>
#include <stdio.h>
#include <iostream.h>

template <class Key, class Value>
class HashBucket
{
  public:
    HashBucket();
    ~HashBucket();
    Key         _key;
    Value       _value;
    HashBucket *_next;
};

template <class Key, class Value>
class HashTable
{
  public:
    
    HashTable(int numBuckets, 
	      int (*hashFn)(Key key, int numBuckets),
	      int (*compFn)(Key key1, Key key2),
	      void (*keyCopyFn)(Key &dst, Key src) = 0,
	      bool deleteKeys = false);
    ~HashTable();
    bool Insert(Key key, Value value);
    bool Lookup(Key key, Value &value);
    bool Delete(Key key);
    int NumEntries();
    void Clear();
    void Display(ostream &os);
    
  protected:
    
    int (*_hashFn)(Key key, int numBuckets);
    int (*_compFn)(Key key1, Key key2);
    void (*_keyCopyFn)(Key &dst, Key src);

    int _numEntries;
    int _numBuckets;
    bool _deleteKeys;
    HashBucket<Key,Value> **_buckets;
    
};


template <class Key, class Value>
HashBucket<Key,Value>::HashBucket()
{
    _next = NULL;
}
	
template <class Key, class Value>
HashBucket<Key,Value>::~HashBucket()
{
}

template <class Key, class Value>
HashTable<Key,Value>::HashTable(int numBuckets, 
				int (*hashFn)(Key index, int numBuckets),
				int (*compFn)(Key key1, Key key2),
				void (*keyCopyFn)(Key &dst, Key src) = 0,
				bool deleteKeys = false)
{
    int i;
    _hashFn = hashFn;
    _compFn = compFn;
    _keyCopyFn = keyCopyFn;
    _deleteKeys = deleteKeys;
    _numBuckets = numBuckets;
    _numEntries = 0;
    _buckets = new HashBucket<Key,Value> *[_numBuckets];
    for (i = 0; i < _numBuckets; i++)
    {
	_buckets[i] = NULL;
    }
}

template <class Key, class Value>
HashTable<Key,Value>::~HashTable()
{
    Clear();
    delete [] _buckets;
}

template <class Key, class Value>
void HashTable<Key,Value>::Clear()
{
    int i;
    HashBucket<Key,Value> *temp;
    for (i = 0; i < _numBuckets; i++)
    {
	temp = _buckets[i];
	while (_buckets[i])
	{
	    temp = _buckets[i];
	    _buckets[i] = _buckets[i]->_next;
	    if (_deleteKeys)
		delete temp->_key;
	    delete temp;
	}
    }
}

template <class Key, class Value>
bool HashTable<Key,Value>::Insert(Key key, Value value)
{
    HashBucket<Key,Value> *b;
    int h;
    int status;
    h = _hashFn(key, _numBuckets);
    for (b = _buckets[h]; b != NULL; b = b->_next)
    {
	if (!_compFn(key, b->_key))
	{
	    b->_value = value;
	    return true;
	}
    }
    b = new HashBucket<Key,Value>;
    if (_keyCopyFn)
	_keyCopyFn(b->_key, key);
    else
	b->_key = key;
    b->_value = value;
    b->_next = _buckets[h];
    _buckets[h] = b;
    _numEntries++;
    return false;
}

template <class Key, class Value>    
bool HashTable<Key,Value>::Lookup(Key key, Value &value)
{
    HashBucket<Key,Value> *b;
    int h;
    h = _hashFn(key, _numBuckets);
    for (b = _buckets[h]; b != NULL; b = b->_next)
    {
	if (!_compFn(key, b->_key))
	{
	    value = b->_value;
	    return true;
	}
    }
    return false;
}

template <class Key, class Value>
bool HashTable<Key,Value>::Delete(Key  key)
{
    HashBucket<Key,Value> *b;
    HashBucket<Key,Value> *prev;
    int h;
    h = _hashFn(key, _numBuckets);
    for (b = _buckets[h], prev = NULL;
	 b != NULL;
	 prev = b, b = b->_next)
    {
	if (!_compFn(key, b->_key))
	{
	    if (prev == NULL)
	    {
		_buckets[h] = b->_next;
	    }
	    else
	    {
		prev->_next = b->_next;
	    }
	    if (_deleteKeys)
		delete b->key;
	    delete b;
	    _numEntries--;
	    return true;
	}
    }
    return false;
}

template <class Key, class Value>
int HashTable<Key,Value>::NumEntries()
{
    return _numEntries;
}

template <class Key, class Value>
void HashTable<Key,Value>::Display(ostream &os)
{
    HashBucket *b;
    int i;
    char *name;
    
    os << "BEGIN HashTable. " << _numBuckets << " buckets, "
	<< _numEntries << " entries" << endl;

    for (i = 0; i < _numBuckets; i++)
    {
	os << " [" << i << "]";
	for (b = _buckets[i];
	     b != NULL;
	     b = b->_next)
	{
	    os << " (" << b->_key << "," << b->_value << ")";
	}
	os << endl;
    }
    
    os << "END HashTable" << endl;
}

#endif

