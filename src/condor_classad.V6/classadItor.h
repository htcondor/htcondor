/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 * No use of the CONDOR Software Program Source Code is authorized
 * without the express consent of the CONDOR Team.  For more information
 * contact: CONDOR Team, Attention: Professor Miron Livny,
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685,
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure
 * by the U.S. Government is subject to restrictions as set forth in
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison,
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CLASSAD_ITOR
#define CLASSAD_ITOR

BEGIN_NAMESPACE( classad )

/** An object for iterating over the attributes of a ClassAd.  Several
    iterators may be active over the same ClassAd at any time, and the same
    iterator object may be used to iterate over other ClassAds as well.
*/
class ClassAdIterator
{
    public:
        /// Constructor
        ClassAdIterator () { ad = NULL ; index = -1; }

        /** Constructor which initializes the iterator to the given ClassAd.
            @param ca The ClassAd to iterate over.
            @see initialize
        */
        ClassAdIterator (const ClassAd &ca) { ad = &ca; index = -1; }

        /// Destructor
        ~ClassAdIterator(){ }

        /** Initializes the object to iterate over a ClassAd; the iterator
                begins at the "before first" position.  This method must be
                called before the iterator is usable.  (The iteration methods
                return false if the iterator has not been initialized.)  This
                method may be called any number of times, with different
                ClassAds as arguments.
            @param l The expression list to iterate over (i.e., the iteratee).
        */
        inline void Initialize (const ClassAd &ca) { ad = &ca ; index = -1; }

        /// Positions the iterator to the "before first" position.
        inline void ToBeforeFirst () { index = -1; }

        /// Positions the iterator to the "after last" position
        inline void ToAfterLast ()  { index = ad->last; }

        /** Gets the next attribute in the ClassAd.
            @param attr The name of the next attribute in the ClassAd.
            @param expr The expression of the next attribute in the ClassAd.
            @return false if the iterator has crossed the last attribute in the
                ClassAd, or true otherwise.
        */
        bool NextAttribute( const char*& attr, ExprTree*& expr );

        /** Gets the attribute currently referred to by the iterator.
            @param attr The name of the next attribute in the ClassAd.
            @param expr The expression of the next attribute in the ClassAd.
            @return false if the operation failed, true otherwise.
        */
        bool CurrentAttribute( const char*& attr, ExprTree*& expr );

        /** Gets the previous attribute in the ClassAd.
            @param attr The name of the previous attribute in the ClassAd.
            @param expr The expression of the previous attribute in the ClassAd.
            @return false if the iterator has crossed the first attribute in
                the ClassAd, or true otherwise.
        */
        bool PreviousAttribute( const char *&attr, ExprTree *&expr );

        /** Predicate to check the position of the iterator.
            @return true iff the iterator is before the first element.
        */
        inline bool IsBeforeFirst () { return (index == -1); }

        /** Predicate to check the position of the iterator.
            @return true iff the iterator is after the last element.
        */
        inline bool IsAfterLast () { return (ad ? (index==ad->last) : false); }

    private:
        int     index;
        const ClassAd   *ad;
};

END_NAMESPACE // classad

#endif//CLASSAD_ITOR
