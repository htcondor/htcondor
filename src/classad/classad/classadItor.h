/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef __CLASSAD_CLASSAD_ITOR_H__
#define __CLASSAD_CLASSAD_ITOR_H__

namespace classad {

/// An object for iterating over the attributes of a ClassAd--deprecated. 
/// We recommend that you now use the STL-like iterators defined with
/// ClassAd.  Several iterators may be active over the same ClassAd at
/// any time, and the same iterator object may be used to iterate over
/// other ClassAds as well.  Note that attributes will not be provided
/// in any specific order.  Also, ClassAdIterator is a ``forward
/// iterator'' only; i.e., there is no PreviousAttribute() method.
class ClassAdIterator
{
    public:
        /// Constructor
        ClassAdIterator() { ad = NULL; }

        /** Constructor which initializes the iterator to the given ClassAd.
            @param ca The ClassAd to iterate over.
            @see initialize
        */
        ClassAdIterator(const ClassAd &ca) { ad=&ca; ToFirst( ); }

        /// Destructor
        ~ClassAdIterator(){ }

        /** Initializes the object to iterate over a ClassAd; the iterator
                begins at the "before first" position.  This method must be
                called before the iterator is usable.  (The iteration methods
                return false if the iterator has not been initialized.)  This
                method may be called any number of times, with different
                ClassAds as arguments.
            @param ca The ClassAd to iterate over (i.e., the iteratee).
        */
        inline void Initialize(const ClassAd &ca){ ad=&ca; ToFirst( ); }

        /// Positions the iterator to the "before first" position.
        inline void ToFirst () { if(ad) itr = ad->attrList.begin( ); }

        /// Positions the iterator to the "after last" position
        inline void ToAfterLast ()  { if(ad) itr = ad->attrList.end( ); }

        /** Gets the next attribute in the ClassAd.
            @param attr The name of the next attribute in the ClassAd.
            @param expr The expression of the next attribute in the ClassAd.
            @return false if the iterator has crossed the last attribute in the
                ClassAd, or true otherwise.
        */
        bool NextAttribute( std::string& attr, const ExprTree*& expr );

        /** Gets the attribute currently referred to by the iterator.
            @param attr The name of the next attribute in the ClassAd.
            @param expr The expression of the next attribute in the ClassAd.
            @return false if the operation failed, true otherwise.
        */
        bool CurrentAttribute( std::string& attr, const ExprTree*& expr ) const;

        /** Predicate to check the position of the iterator.
            @return true iff the iterator is before the first element.
        */
        inline bool IsAtFirst() const {
			return(ad?(itr==ad->attrList.begin()):false);
		}

        /** Predicate to check the position of the iterator.
            @return true iff the iterator is after the last element.
        */
        inline bool IsAfterLast() const {
			return(ad?(itr==ad->attrList.end()):false); 
		}

    private:
		AttrList::const_iterator	itr;
        const ClassAd   			*ad;
};

} // classad

#endif//__CLASSAD_CLASSAD_ITOR_H__
