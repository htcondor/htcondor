#include "condor_common.h"
#include "condor_debug.h"
#include "perm.h"

//
// get_permissions:  1 = yes, 0 = no, -1 = unknown/error
//
int perm::get_permissions( const char *file_name ) {
	DWORD retVal;
	TRUSTEE Trustee;
	ACCESS_MASK AccessRights;
	PACL pacl;

	PSECURITY_DESCRIPTOR pSD;
	DWORD pSD_length = 0;
	DWORD pSD_length_needed = 0;
	BOOL acl_present = FALSE;
	BOOL acl_defaulted = FALSE;

	// Do the call first to find out how much space is needed.

	pSD = NULL;

	GetFileSecurity(
			file_name,					// address of string for file name
			DACL_SECURITY_INFORMATION,	// requested information
			pSD,						// address of security descriptor
			pSD_length,					// size of security descriptor buffer
			&pSD_length_needed			// address of required size of buffer
			);

	if( pSD_length_needed <= 0 ) {					// Find out how much space is needed, if <=0 then error
		if (GetLastError() == 2) {

			// Here we have the tricky part of walking up the directory path
			// Typically it works like this:  If the filename exists, great, we'll
			//   get the permissions on that.  If the filename does not exist, then
			//   we pop the filename part off the file_name and look at the
			//   directory.  If that directory should be (for some odd reason) non-
			//   existant, then we just pop that off, until either we find something
			//   thats will give us a permissions bitmask, or we run out of places
			//   to look (which shouldn't happen since c:\ should always give us
			//   SOMETHING...
			int i = strlen( file_name ) - 1;
			while ( i >= 0 && ( file_name[i] != '\\' && file_name[i] != '/' ) ) {
				i--;
			}
			if ( i < 0 ) {	// We've nowhere else to look, and this is bad.
				return -1;  // Its not a no, its an unknown
			}
			char *new_file_name = new char[i+1];
			strncpy(new_file_name, file_name, i);
			new_file_name[i]= '\0';

			// Now that we've chopped off more of the filename, call get_permissions
			// again...
			DWORD retval = get_permissions( new_file_name );
			delete[] new_file_name;

			// ... and return what it returns. (after deleting the string that was
			// allocated.

			return retval;
		}
		dprintf(D_ALWAYS, "perm::GetFileSecurity failed (err=%d)\n", GetLastError());
		return -1;
	}

	pSD_length = pSD_length_needed + 2;		// Add 2 for safety.
	pSD_length_needed = 0;
	pSD = new BYTE[pSD_length];
	
	// Okay, now that we've found something, and know how large of an SD we need,
	// call the thing for real and lets get ON WITH IT ALREADY...

	if( !GetFileSecurity(
			file_name,					// address of string for file name
			DACL_SECURITY_INFORMATION,	// requested information
			pSD,						// address of security descriptor
			pSD_length,					// size of security descriptor buffer
			&pSD_length_needed			// address of required size of buffer
			) ) {
		dprintf(D_ALWAYS, "perm::GetFileSecurity(%s) failed (err=%d)\n", file_name, GetLastError());
		delete pSD;
		return -1;
	}

	// Now, get the ACL from the security descriptor
	if( !GetSecurityDescriptorDacl(
		pSD,							// address of security descriptor
		&acl_present,					// address of flag for presence of disc. ACL
		&pacl,							// address of pointer to ACL
		&acl_defaulted					// address of flag for default disc. ACL
		) ) {
		dprintf(D_ALWAYS, "perm::GetSecurityDescriptorDacl failed (file=%s err=%d)\n", file_name, GetLastError());
		delete pSD;
		return -1;
	}

	// Fill in the Trustee thing-a-ma-jig(tm).  What's a thing-a-ma-jig(tm) you ask?
	// Frankly, I don't know.  Its a Microsoft thing...
	Trustee.pMultipleTrustee = NULL;
	Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
	Trustee.TrusteeForm = TRUSTEE_IS_SID;
	Trustee.TrusteeType = TRUSTEE_IS_USER;
	Trustee.ptstrName = (char *)psid;

	retVal = GetEffectiveRightsFromAcl( pacl,	// ACL to get trustee's rights from
				&Trustee,						// trustee to get rights for
				&AccessRights					// receives trustee's access rights
				);

	if( retVal != ERROR_SUCCESS ) {
		dprintf(D_ALWAYS, "perm::GetEffectiveRightsFromAcl failed (file=%s err=%d)\n", file_name, GetLastError());
		return -1;
	}

	// And now, we have the access rights, so return those.
	return AccessRights;
}

perm::perm() {
	psid =(PSID) &sidBuffer;
	sidBufferSize = 100;
	domainBufferSize = 80;
	must_freesid = false;
/*  These shouldn't be needed...
	perm_read = FILE_GENERIC_READ;
	perm_write = FILE_GENERIC_WRITE;
	perm_execute = FILE_GENERIC_EXECUTE;
*/
}

perm::~perm() {
	if ( psid && must_freesid ) FreeSid(psid);
}

bool perm::init( char *accountname, char *domain ) 
{
	SID_NAME_USE snu;

	if ( psid && must_freesid ) FreeSid(psid);
	must_freesid = false;

	psid = (PSID) &sidBuffer;

	if ( !LookupAccountName( domain,		// Domain
		accountname,						// Acocunt name
		psid, &sidBufferSize,				// Sid
		domainBuffer, &domainBufferSize,	// Domain
		&snu ) )							// SID TYPE
	{
		dprintf(D_ALWAYS,
			"perm::init: Lookup Account Name %s failed, using Everyone\n",
			accountname);
		
		// SID_IDENTIFIER_AUTHORITY  NTAuth = SECURITY_NT_AUTHORITY;
		SID_IDENTIFIER_AUTHORITY  NTAuth = SECURITY_WORLD_SID_AUTHORITY;
		if ( !AllocateAndInitializeSid(&NTAuth,1,
						SECURITY_WORLD_RID,
						0,
						0,0,0,0,0,0,&psid) ) 
		{	
			EXCEPT("Failed to find group EVERYONE");
		}
		must_freesid = true;
	} else {
		dprintf(D_FULLDEBUG,"perm::init: Found Account Name %s\n",
			accountname);
	}

	return true;
}


// perm:[read|write|execute]_access.  These functions return:
// -1 == unknown (error), 0 == no, 1 == yes.

int perm::read_access( const char * filename ) {
	if ( !volume_has_acls(filename) ) {
		return 1;
	}

	int p = get_permissions( filename );

	if ( p < 0 ) return -1;
	return ( p & FILE_GENERIC_READ ) == FILE_GENERIC_READ;
}

int perm::write_access( const char * filename ) {
	if ( !volume_has_acls(filename) ) {
		return 1;
	}

	int p = get_permissions( filename );

	if ( p < 0 ) return -1;
	return ( p & FILE_GENERIC_WRITE ) == FILE_GENERIC_WRITE;
}

int perm::execute_access( const char * filename ) {
	if ( !volume_has_acls(filename) ) {
		return 1;
	}

	int p = get_permissions( filename );

	if ( p < 0 ) return -1;
	return ( p & FILE_GENERIC_EXECUTE ) == FILE_GENERIC_EXECUTE;
}


// volume_has_acls() returns true if the filename exists on an NTFS
// volume or some other file system which preserves ACLs on files.  
// false if otherwise (i.e. FAT, FAT32, CDFS, HPFS, etc) or error.
bool perm::volume_has_acls( const char *filename )
{
	char path_buf[5];
	char *root_path = path_buf;
	DWORD foo,fsflags;

	path_buf[0] = '\0';

	// This function will not work with UNC paths... yet.
	if ( !filename || (filename[0]=='\\' && filename[1]=='\\') ) {
		EXCEPT("UNC pathnames not supported yet");
	}

	if ( filename[1] == ':' ) {
		root_path[0] = filename[0];
		root_path[1] = ':';
		root_path[2] = '\\';
		root_path[3] = '\0';
	} else {
		root_path = NULL;
	}

	if ( !GetVolumeInformation(root_path,NULL,0,NULL,&foo,&fsflags,
			NULL,0) ) 
	{
		dprintf(D_ALWAYS,
			"perm: GetVolumeInformation on volume %s FAILED err=%d\n",
			path_buf,
			GetLastError());
		return false;
	}

	if ( fsflags & FS_PERSISTENT_ACLS )		// note: single & (bit comparison)
		return true;
	else {

		dprintf(D_FULLDEBUG,"perm: volume %s does not have ACLS\n",path_buf);
		return false;
	}
}

// set_acls sets the acls on the filename (typically a directory).
// This is currently set to add GENERIC_ALL (otherwise known
// in NT-land as Full) permissions.

// This code is based off the book that Todd gave me.

int perm::set_acls( const char *filename )
{
  BOOL ret;
  LONG err;
  SECURITY_DESCRIPTOR *sdData;
  SECURITY_DESCRIPTOR absSD;
  PACL pacl;
  PACL pNewACL;
  DWORD newACLSize;
  BOOL byDef;
  BOOL haveDACL;
  DWORD sizeRqd;
  UINT x;
  ACL_SIZE_INFORMATION aclSize;
  ACCESS_ALLOWED_ACE *pace;

    // If this is not on an NTFS volume, we're done.  In fact, we'll
	// likely crash if we try all the below ACL crap on a volume which
	// does not support ACLs. Dooo!
    if ( !volume_has_acls(filename) ) {
		dprintf(D_FULLDEBUG, "perm::set_acls(%s): volume has no ACLS\n",filename);
		return 1;
	}

	// Make sure we have the sid.

	if ( psid == NULL ) {
		dprintf(D_ALWAYS, "perm::set_acls(%s): do not have SID for user\n",filename);
		return -1;
	}

	// ----- Get a copy of the SD/DACL -----

	// find out how much mem is needed
	// to hold existing SD w/DACL
	sizeRqd=0;
	
	if (GetFileSecurity( filename,
		DACL_SECURITY_INFORMATION,
		NULL, 0, &sizeRqd )) {
		dprintf(D_ALWAYS,"perm::set_acls() Unable to get SD size.\n");
		return -1;
	}

	err = GetLastError();


	if ( err != ERROR_INSUFFICIENT_BUFFER ) {
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to get SD size.\n", filename);
		return -1;
	}

	// allocate that memory
	sdData=( SECURITY_DESCRIPTOR * ) malloc( sizeRqd );

	if ( sdData == NULL ) {
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to allocate memory.\n", filename);
		return -1;
	}

  // actually get the SD info
	if (!GetFileSecurity( filename,
		DACL_SECURITY_INFORMATION,
		sdData, sizeRqd, &sizeRqd )) {
		
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to get SD info.\n", filename);
		free(sdData);
		return -1;
	}

  // ----- Create a new absolute SD and DACL -----

  // initialize absolute SD
	ret=InitializeSecurityDescriptor(&absSD,
		SECURITY_DESCRIPTOR_REVISION);

	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to init new SD.\n", filename);
		free(sdData);
		return -1;
	}

	// get the DACL info
	ret=GetSecurityDescriptorDacl(sdData,
		&haveDACL, &pacl, &byDef);
	
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to get DACL info.\n", filename);
		free(sdData);
		return -1;
	}

	if (!haveDACL)
	{
		// compute size of new DACL
		newACLSize= sizeof(ACCESS_ALLOWED_ACE) +
			GetLengthSid(psid) - sizeof(DWORD);
	}
	else
	{
		// get size info about existing DACL
		ret=GetAclInformation(pacl, &aclSize,
			sizeof(ACL_SIZE_INFORMATION),
			AclSizeInformation);
		
		// compute size of new DACL
		newACLSize=aclSize.AclBytesInUse +
			sizeof(ACCESS_ALLOWED_ACE) +
			GetLengthSid(psid) -
			sizeof(DWORD);
	}

	// allocate memory
//	pNewACL=(PACL) GlobalAlloc(GPTR, newACLSize);
	pNewACL=(PACL) malloc(newACLSize);
	if (pNewACL == NULL)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to allocate memory.\n", filename);
		free(sdData);
		return -1;
	}

	// initialize the new DACL
	ret=InitializeAcl(pNewACL, newACLSize,
		ACL_REVISION);
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to init new DACL.\n", filename);
		free(pNewACL);
		free(sdData);
		return -1;
	}

	// ----- Copy existing DACL into new DACL -----

	if (haveDACL)
	{
		// copy ACEs from existing DACL
		// to new DACL
		for (x=0; x<aclSize.AceCount; x++)
		{
			ret=GetAce(pacl, x, (LPVOID *) &pace);
			if (!ret)
			{
				dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to get ACE.\n", filename);
				free(pNewACL);
				free(sdData);
				return -1;
			}
			
			ret=AddAce(pNewACL, ACL_REVISION, MAXDWORD,
				pace, pace->Header.AceSize);
			if (!ret)
			{
				dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to add ACE.\n", filename);
				free(pNewACL);
				free(sdData);
				return -1;
			}
		}
	}

	// ----- Add the new ACE to the new DACL -----

	// add access allowed ACE to new
	// DACL
	ret=AddAccessAllowedAce(pNewACL,
		ACL_REVISION, GENERIC_ALL, psid);
	
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to add access allowed ACE.\n", filename);
		free(pNewACL);
		free(sdData);
		return -1;
	}

	// set the new DACL
	// in the absolute SD
	ret=SetSecurityDescriptorDacl(&absSD,
		TRUE, pNewACL, FALSE);
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to install DACL.\n", filename);
		free(pNewACL);
		free(sdData);
		return -1;
	}

	// check the new SD
	ret=IsValidSecurityDescriptor(&absSD);
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): SD invalid.\n", filename);
		free(pNewACL);
		free(sdData);
		return -1;
	}

	// ----- Install the new DACL -----

	// install the updated SD
	err=SetFileSecurity( filename,
		DACL_SECURITY_INFORMATION,
		&absSD);
	if (!err)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to set file SD (err=%d).\n", 
			filename,GetLastError() );
		free(pNewACL);
		free(sdData);
		return 0;
	}

	// release memory
	free(pNewACL);
	free(sdData);

	return 1;
}
