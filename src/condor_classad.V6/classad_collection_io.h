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

#ifndef CLASSAD_COLLECTION_IO
#define CLASSAD_COLLECTION_IO

#include "classad.h"
#include "classad_collection_types.h"
#include "classad_io.h"
#include <string>
#include <vector>

BEGIN_NAMESPACE( classad )

//----------------------------------------------------------------------
/// A handle to a Collection View
typedef int ViewID;

//----------------------------------------------------------------------
/** The CollectionIO abstract base class provides the interface to
    manipulate collections.  Manipulations can take place on a local
    collection, as with a server, or remotely, as with a client.  The
    behavior of the interface depends on whether a CollectionIOClient or
    CollectionIOServer is instantiated.
*/
class CollectionIO {

  public:
    
    //--------------------------------------------------
    /** @name View Manipulations */
    //@{

    /** Create a Constraint View, as a child of another view.  A constraint
        view always contains the subset of ads from the parent, which match
        the constraint.

        @param viewID The ID of the new child.  Invalid if false is
        returned.
        @param parentViewID The ID of the parent view.
        @param rank The rank expression in string format.
        Determines how the ads will be ordered in the collection.
        @param constraint sets the constraint expression for this view.
        @return true if successful.  The GetError{ID,Msg} meathods reveal
        why false was returned.
    */
    virtual bool CreateConstraintView (ViewID            & viewID,
                                       const ViewID      & parentViewID,
                                       const std::string & rank,
                                       const std::string & constraint) = 0;

    /** Create a partition parent view, as a child of another view. A
        partiton view defines a partition based on a set of
        attributes. For each distinct set of values (corresponding to
        these attributes), a new child partition view will be created,
        which will contain all the class-ads from the parent view that
        have these values. The partition parent view itself doesn't hold
        any class-ads, only its children do (the iteration methods for
        getting child views can be used to retrieve them).

        @param viewID The ID of the new child.  Invalid if false is
        returned.
        @param parentViewID The ID of the parent view.
        @param rank The rank expression in string format.
        Determines how the ads will be ordered in the collection.
        @param attribs The set of attribute names used to define the
        partition.
        @return true if successful.  The GetError{ID,Msg} meathods reveal
        why false was returned.  */
    virtual bool CreatePartitionView (
        ViewID             & viewID,
        const ViewID       & parentViewID,
        const std::string  & rank,
        const std::vector<std::string> & attribs) =0;
    
    /** Find the partition of a class-ad.  Returns (by reference) the ID of
        the child partition that would contain the specified class-ad.

        @param viewID The ID of view containing the match, invalid if
        false is returned.
        @param parentViewID The ID of the parent view.
        @param representative a pointer to the class-ad that will be
        checked.
        @return true if successful.  The GetError{ID,Msg} meathods reveal
        why false was returned.
    */
    virtual bool FindPartitionView (ViewID         & viewID,
                                    const ViewID   & parentViewID,
                                    const ClassAd  * representative) const = 0;
    
    /** Deletes a view and all of its descendants from the collection tree.

        @param viewID The ID of the view to be deleted.
        @return true if successful.  The GetError{ID,Msg} meathods reveal
        why false was returned.
    */
    virtual bool DeleteView       (const ViewID   & viewID) = 0;

    //@}

    //--------------------------------------------------
    /** All manipulations of the collection repository are atomic.  If the
        transaction fails, then the collection is unchanged.  Only
        completed transactions can succeed (no partial success is
        possible).
        @name ClassAd Manipulations
    */
    //@{

    /** Atomically insert a new class-ad with the specified key. The supplied
        class-ad will be inserted directly into the collection hierarchy,
        overwriting any previous class-ad with the same key.

        @param key The key of the classad.
        @param ad The class-ad to put into the repository.
        @return true if successful.  The GetError{ID,Msg} meathods reveal
        why false was returned.
    */
    virtual bool      AddClassAd   (const std::string & key,
                                      const ClassAd * ad) = 0;

    ///
    virtual bool      ModifyClassAd (const std::string & key,
                                      const ClassAd * ad) = 0;

    ///
    virtual bool      RemoveClassAd (const std::string & key) = 0;

    ///
    virtual ClassAd * GetClassAd    (const std::string & key) = 0;

    //@}

    //--------------------------------------------------
    /** Transaction explanation
       
     */
    //@{

    ///
    virtual bool OpenTransaction ();

    ///
    virtual bool CloseTransaction();

    //@}


    //--------------------------------------------------
    /** @name Iterators */
    //@{

    /** Initialize a content iterator to iterate over the ads in a view.
        @param viewID The ID of the view whose ads are to be iterated over.
        @param i The content iterator to be initialized, invalid if false
        is returned.
        @return true if successful.  The GetError{ID,Msg} meathods reveal
        why false was returned.
    */
    virtual bool IteratorInit (const ViewID & viewID,
                               CollContentIterator & i) = 0;

    /** Initialize a child iterator to iterate over the child views of a
        partition view.

        @param viewID The ID of the view whose child views are to be
        iterated over.
        @param i The child iterator to be initialized, invalid if
        false is returned.
        @return true if successful.  The GetError{ID,Msg}
        meathods reveal why false was returned.
    */
    virtual bool IteratorInit (const ViewID & viewID,
                               CollChildIterator & i) = 0;

    //@}

    //--------------------------------------------------
    /** @name Error Handling */
    //@{

    /// enumeration of error IDs
    enum ErrorID { };

    /** Get the ID of the Error that last occurred.
        @return The ID.
    */
    virtual ErrorID GetErrorID  () const;

    /** Get the message describing the error the last occurred.
        @return The message.
    */
    virtual std::string  GetErrorMsg () const;

    /** If a lock file exists, indicate that this CollectionIO needs to
        be recovered by calling Recover().
        @return true is recovery needed.
    */
    virtual bool    RecoveryNeeded () const;

    /** Recover this CollectionIO.
        @return true if successful.  The GetError{ID,Msg} meathods reveal
        why false was returned.
    */
    virtual bool    Recover ();

    //@}

  protected:

    /** @name Constructor and Destructor */
    //@{

    /** CollectionIO Constructor.
        @param atomic_log_name The file used to log transaction events for
        client and server recoverability.
    */
    CollectionIO (const std::string & atomic_log_name);

    /// CollectionIO Destructor.
    virtual ~CollectionIO();

    //@}

    /// Connects a CollectionIOClient with a CollectionIOServer.
    ByteStream * m_stream;

    /// The file that logs persistant, atomic transactions
    FileStream m_atomic_log;
};

//------------------------------------------------------------------------
/** CollectionIOClient represents the client's interface to a remote
    collection repository.  All transactions are atomic and recoverable.
*/
class CollectionIOClient : public CollectionIO {

  public:
    
    /** @name Constructor and Destructor */
    //@{

    /** CollectionIOClient Constructor.
        @param atomic_log_name The file used to log transaction events for
        recoverability.
    */
    CollectionIOClient (const std::string & atomic_log_name);

    /// CollectionIOClient Destructor.
    virtual ~CollectionIOClient ();

    //@}

    //--------------------------------------------------
    /** Bind this IO to an opened ByteStream, which is connected to
        a CollectionIOServer.

        @param stream An already opened ByteStream.  This IO object will
        not close the stream.
        @return true if successful.  The GetError{ID,Msg} meathods reveal
        why false was returned.
    */
    bool Bind (ByteStream * stream);

};

//------------------------------------------------------------------------
/** CollectionIOServer represents the server's interface to a local
    collection repository.  All transactions are atomic and recoverable.
*/
class CollectionIOServer : public CollectionIO {
    
  public:
    
    /** @name Constructor and Destructor */
    //@{
    
    /** CollectionIOServer Constructor.
        @param atomic_log_name The file used to log transaction events for
        recoverability.
        @param classad_log_name The name of the log file used to store
        persistant classads.  If a "" value is passed, no log file will
        be used (i.e., the classads will not be persistent).
        @param rank The rank expression for the collection
    */
    CollectionIOServer (const std::string & atomic_log_name,
                        const std::string & classad_log_name,
                        const std::string & rank);

    /// CollectionIOServer Destructor.
    virtual ~CollectionIOServer();

    //@}

    //--------------------------------------------------
    /** Transaction explanation
       
     */
    //@{

    ///
    virtual bool OpenTransaction();

    ///
    virtual bool CloseTransaction();

    ///
    void   TransactionSetName (const std::string & name);

    ///
    std::string GetTransactionName () const;

    /// Wrapper Function
    inline bool OpenTransaction (const std::string & name) {
        TransactionSetName(name);
        return OpenTransaction();
    }

    /// Wrapper Function
    inline bool CloseTransaction (const std::string & name) {
        TransactionSetName(name);
        return CloseTransaction();
    }

    //@}

    //--------------------------------------------------
    /** Handle a message that has arrived on an open stream.  The server
        will handle a single message and return.  The stream is left
        open.  This meathod will likely be registered to daemoncore
        as a callback.

        @param stream An already opened ByteStream.
        This IO object will not close the stream.
        @return true if successful.  The GetError{ID,Msg} meathods reveal
        why false was returned.
    */
    bool HandleMsg (ByteStream * stream);

    /** @name Diagnostic Meathods */
    //@{

    /** Print a single view in the repository (for debugging purposes).
        @param viewID The ID of the view to be printed.
        @param out The ostream to print to.
    */
    void   Print       (const ViewID & viewID, ostream & out = cout) const;

    /** Print the whole repository (for debugging purposes).
        @param out The ostream to print to.
    */
    void   Print       (ostream & out = cout) const;

    /** Print all of the class-ads (with their content).
        @param out The ostream to print to.
    */
    void   PrintAllAds (ostream & out = cout) const;

    //@}

  private:
    
};

END_NAMESPACE // classad

#endif // CLASSAD_COLLECTION_IO
