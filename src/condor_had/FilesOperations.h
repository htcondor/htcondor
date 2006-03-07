#ifndef FILES_OPERATIONS_H 
#define FILES_OPERATIONS_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "MyString.h"

class FilesOperations
{
public:
	/* Function   : initialize
     * Description: initializes static data members, i.e. configuration
     *              parameters-dependent data members
     */
	//static void initialize();
	/* Function    : safeRotateFile
	 * Arguments   : filePath - file, on which the manipulations are to be
	 *							performed
	 *				 temporaryFilesExtension - extension of temporary files,
 	 *               which are rotated with state and version files
 	 * Return value: bool - success/failure value
 	 * Description : rotates state and version files with their temporary
 	 *               counterparts
 	 */
	static bool safeRotateFile( const char* filePath,
							    const char* temporaryFilesExtension );
    /* Function    : safeCopyFile
  	 * Arguments   : filePath - file, on which the manipulations are to be
     *                          performed
	 *				 temporaryFilesExtension - extension of temporary files,
 	 *               which are copied from state and version files
 	 * Return value: bool - success/failure value
 	 * Description : copies state and version files to their temporary
 	 *               counterparts
 	 */
	static bool safeCopyFile ( const char* filePath,
							   const char* temporaryFilesExtension );
    /* Function    : safeUnlinkFile
 	 * Arguments   : filePath - file, on which the manipulations are to be
     *                          performed
	 *				 temporaryFilesExtension - extension of unlinked temporary
 	 *               files
 	 * Return value: bool - success/failure value
 	 * Description : unlinks state and version files' temporary counterparts
 	 */
	static bool safeUnlinkFile( const char* filePath,
					   		    const char* temporaryFilesExtension );
private:
	// functions operating with version and state files
	typedef bool (*ManipulationFunction)(const char*, const char* );
// Manipulation functions
	static bool rotateFile(const char* filePath,
                           const char* temporaryFilesExtension);
	static bool copyFile  (const char* filePath,
                           const char* temporaryFilesExtension);
	static bool unlinkFile(const char* filePath,
                           const char* temporaryFilesExtension);
// End of manipulation functions
	static bool safeManipulateFile(const char* filePath,
							       const char* temporaryFilesExtension,
                                   ManipulationFunction manipulation );
};

#endif // FILES_OPERATIONS_H
