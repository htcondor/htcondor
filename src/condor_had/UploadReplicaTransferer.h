#ifndef UPLOAD_REPLICA_TRANSFERER_H
#define UPLOAD_REPLICA_TRANSFERER_H

#include "BaseReplicaTransferer.h"

/* Class      : UploadReplicaTransferer
 * Description: class, encapsulating the uploading 'condor_transferer'
 *              process behaviour
 */
class UploadReplicaTransferer : public BaseReplicaTransferer
{
public:
    /* Function  : UploadReplicaTransferer constructor
     * Arguments : pDaemonSinfulString  - downloading daemon sinfull string
     *             pVersionFilePath     - version string in dot-separated format
     *             pStateFilesPathsList - list of paths to the state files
     */
    UploadReplicaTransferer(const MyString&  pDaemonSinfulString,
                            const MyString&  pVersionFilePath,
                            //const MyString&  pStateFilePath):
							const StringList& pStateFilePathsList):
         BaseReplicaTransferer( pDaemonSinfulString,
                                pVersionFilePath,
                                //pStateFilePath ) {};
								pStateFilePathsList ) {}; 
	/* Function    : initialize
     * Return value: TRANSFERER_TRUE  - upon success
     *               TRANSFERER_FALSE - upon failure
     * Description : main function of uploading 'condor_transferer' process,
     *               where the uploading of important files happens
     */
     virtual int initialize();

private:
    int upload();
    int uploadFile(MyString& filePath, MyString& extension);
};

#endif // UPLOAD_REPLICA_TRANSFERER_H
