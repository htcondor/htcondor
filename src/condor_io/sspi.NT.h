/****
 * Note: this header file is included in the Condor source code because
 * we want to use SSPI, and the only place we could find the header files
 * is in the MSN library.  so we copied it here.  it is property of Microsoft.
 * note: this file includes <sspi.h> and <issperr.h>.
 *
****/

//+--------------------------------------------------------------------------- 
// 
//  Microsoft Windows 
//  Copyright (C) Microsoft Corporation, 1992-1997. 
// 
//  File:       sspi.h 
// 
//  Contents:   Security Support Provider Interface 
//              Prototypes and structure definitions 
// 
//  Functions:  Security Support Provider API 
// 
//  History:    11-24-93   RichardW   Created 
// 
//---------------------------------------------------------------------------- 
 
#ifndef __SSPI_H__ 
#define __SSPI_H__ 
 
// added in... 
#ifndef _HRESULT_DEFINED
#define _HRESULT_DEFINED
typedef LONG HRESULT;

#endif // !_HRESULT_DEFINED 
// 
// Determine environment: 
// 
 
#ifdef SECURITY_WIN32 
#define ISSP_LEVEL  32 
#define ISSP_MODE   1 
#endif // SECURITY_WIN32 
 
#ifdef SECURITY_WIN16 
#define ISSP_LEVEL  16 
#define ISSP_MODE   1 
#endif // SECURITY_WIN16 
 
#ifdef SECURITY_KERNEL 
#define ISSP_LEVEL  32 
 
// 
// SECURITY_KERNEL trumps SECURITY_WIN32.  Undefine ISSP_MODE so that 
// we don't get redefine errors. 
// 
#ifdef ISSP_MODE 
#undef ISSP_MODE 
#endif 
#define ISSP_MODE   0 
#endif // SECURITY_KERNEL 
 
#ifdef SECURITY_OS212 
#define ISSP_LEVEL  16 
#define ISSP_MODE   1 
#endif // SECURITY_OS212 
 
#ifdef SECURITY_DOS 
#define ISSP_LEVEL  16 
#define ISSP_MODE   1 
#endif // SECURITY_DOS 
 
#ifdef SECURITY_MAC 
#define ISSP_LEVEL  32 
#define ISSP_MODE   1 
#endif // SECURITY_MAC 
 
 
#ifndef ISSP_LEVEL 
#error  You must define one of SECURITY_WIN32, SECURITY_WIN16, SECURITY_KERNEL 
#error  SECURITY_DOS, SECURITY_MAC or SECURITY_OS212 
#endif // !ISSP_LEVEL 
 
 
// 
// Now, define platform specific mappings: 
// 
 
#if ISSP_LEVEL == 16 
 
typedef short SECURITY_STATUS; 
typedef unsigned short SEC_WCHAR; 
typedef char SEC_CHAR; 
#define SEC_TEXT(_x_) _x_ 
 
#ifdef SECURITY_WIN16 
 
#define SEC_FAR __far 
#define SEC_ENTRY __pascal __far __export 
 
#else // SECURITY_WIN16 
 
#define SEC_FAR __far 
#define SEC_ENTRY __pascal __far __loadds 
#pragma warning(disable:4147) 
 
#endif // SECURITY_WIN16 
 
#elif defined(SECURITY_MAC)  // ISSP_LEVEL == 16 
 
#define SEC_ENTRY 
#define SEC_TEXT(_X_) _X_ 
#define SEC_FAR 
 
typedef unsigned short SEC_WCHAR; 
typedef char SEC_CHAR; 
typedef long SECURITY_STATUS; 
 
// No Unicode on the Mac 
 
typedef SEC_CHAR SEC_FAR * SECURITY_PSTR; 
typedef SEC_CHAR SEC_FAR * SECURITY_PCSTR; 
 
#else // ISSP_LEVEL == 16 
 
// 
// For NT-2 and up, wtypes will define HRESULT to be long. 
// 
 
// begin_ntifs 
 
typedef WCHAR SEC_WCHAR; 
typedef CHAR SEC_CHAR; 
 
 
typedef LONG SECURITY_STATUS; 
 
#define SEC_TEXT TEXT 
#define SEC_FAR 
#define SEC_ENTRY __stdcall 
 
// end_ntifs 
 
// 
// Decide what a string - 32 bits only since for 16 bits it is clear. 
// 
 
 
#ifdef UNICODE 
typedef SEC_WCHAR SEC_FAR * SECURITY_PSTR; 
typedef CONST SEC_WCHAR SEC_FAR * SECURITY_PCSTR; 
#else // UNICODE 
typedef SEC_CHAR SEC_FAR * SECURITY_PSTR; 
typedef CONST SEC_CHAR SEC_FAR * SECURITY_PCSTR; 
#endif // UNICODE 
 
 
#endif // ISSP_LEVEL == 16 
 
// 
// Equivalent string for rpcrt: 
// 
 
#define __SEC_FAR SEC_FAR 
 
 
// 
// Okay, security specific types: 
// 
 
// begin_ntifs 
 
typedef struct _SecHandle 
{ 
    unsigned long dwLower; 
    unsigned long dwUpper; 
} SecHandle, SEC_FAR * PSecHandle; 
 
typedef SecHandle CredHandle; 
typedef PSecHandle PCredHandle; 
 
typedef SecHandle CtxtHandle; 
typedef PSecHandle PCtxtHandle; 
 
// end_ntifs 
 
#if ISSP_LEVEL == 32 
 
 
#  ifdef WIN32_CHICAGO 
 
typedef unsigned __int64 QWORD; 
typedef QWORD SECURITY_INTEGER, *PSECURITY_INTEGER; 
#define SEC_SUCCESS(Status) ((Status) >= 0) 
 
#  elif defined(_NTDEF_) || defined(_WINNT_) 
 
typedef LARGE_INTEGER _SECURITY_INTEGER, SECURITY_INTEGER, *PSECURITY_INTEGER; // ntifs 
 
#  else // _NTDEF_ || _WINNT_ 
 
// BUGBUG:  Alignment for axp 
 
typedef struct _SECURITY_INTEGER 
{ 
    unsigned long LowPart; 
    long HighPart; 
} SECURITY_INTEGER, *PSECURITY_INTEGER; 
 
#  endif // _NTDEF_ || _WINNT_ 
 
#  ifndef SECURITY_MAC 
 
typedef SECURITY_INTEGER TimeStamp;                 // ntifs 
typedef SECURITY_INTEGER SEC_FAR * PTimeStamp;      // ntifs 
 
#  else // SECURITY_MAC 
typedef unsigned long TimeStamp; 
typedef unsigned long * PTimeStamp; 
#  endif // SECUIRT_MAC 
 
#else // ISSP_LEVEL == 32 
 
typedef unsigned long TimeStamp; 
typedef unsigned long SEC_FAR * PTimeStamp; 
#  ifdef WIN32_CHICAGO 
typedef TimeStamp LARGE_INTEGER; 
#endif // WIN32_CHICAGO 
 
#endif // ISSP_LEVEL == 32 
 
 
// 
// If we are in 32 bit mode, define the SECURITY_STRING structure, 
// as a clone of the base UNICODE_STRING structure.  This is used 
// internally in security components, an as the string interface 
// for kernel components (e.g. FSPs) 
// 
 
#if ISSP_LEVEL == 32 
#  ifndef _NTDEF_ 
typedef struct _SECURITY_STRING { 
    unsigned short      Length; 
    unsigned short      MaximumLength; 
#    ifdef MIDL_PASS 
    [size_is(MaximumLength / 2), length_is(Length / 2)] 
#    endif // MIDL_PASS 
    unsigned short *    Buffer; 
} SECURITY_STRING, * PSECURITY_STRING; 
#  else // _NTDEF_ 
typedef UNICODE_STRING SECURITY_STRING, *PSECURITY_STRING;  // ntifs 
#  endif // _NTDEF_ 
#endif // ISSP_LEVEL == 32 
 
 
// begin_ntifs 
 
// 
// SecPkgInfo structure 
// 
//  Provides general information about a security provider 
// 
 
typedef struct _SecPkgInfoW 
{ 
    unsigned long fCapabilities;        // Capability bitmask 
    unsigned short wVersion;            // Version of driver 
    unsigned short wRPCID;              // ID for RPC Runtime 
    unsigned long cbMaxToken;           // Size of authentication token (max) 
#ifdef MIDL_PASS 
    [string] 
#endif 
    SEC_WCHAR SEC_FAR * Name;           // Text name 
 
#ifdef MIDL_PASS 
    [string] 
#endif 
    SEC_WCHAR SEC_FAR * Comment;        // Comment 
} SecPkgInfoW, SEC_FAR * PSecPkgInfoW; 
 
// end_ntifs 
 
typedef struct _SecPkgInfoA 
{ 
    unsigned long fCapabilities;        // Capability bitmask 
    unsigned short wVersion;            // Version of driver 
    unsigned short wRPCID;              // ID for RPC Runtime 
    unsigned long cbMaxToken;           // Size of authentication token (max) 
#ifdef MIDL_PASS 
    [string] 
#endif 
    SEC_CHAR SEC_FAR * Name;            // Text name 
 
#ifdef MIDL_PASS 
    [string] 
#endif 
    SEC_CHAR SEC_FAR * Comment;         // Comment 
} SecPkgInfoA, SEC_FAR * PSecPkgInfoA; 
 
#ifdef UNICODE 
#  define SecPkgInfo SecPkgInfoW        // ntifs 
#  define PSecPkgInfo PSecPkgInfoW      // ntifs 
#else 
#  define SecPkgInfo SecPkgInfoA 
#  define PSecPkgInfo PSecPkgInfoA 
#endif // !UNICODE 
 
// begin_ntifs 
 
// 
//  Security Package Capabilities 
// 
#define SECPKG_FLAG_INTEGRITY       0x00000001  // Supports integrity on messages 
#define SECPKG_FLAG_PRIVACY         0x00000002  // Supports privacy (confidentiality) 
#define SECPKG_FLAG_TOKEN_ONLY      0x00000004  // Only security token needed 
#define SECPKG_FLAG_DATAGRAM        0x00000008  // Datagram RPC support 
#define SECPKG_FLAG_CONNECTION      0x00000010  // Connection oriented RPC support 
#define SECPKG_FLAG_MULTI_REQUIRED  0x00000020  // Full 3-leg required for re-auth. 
#define SECPKG_FLAG_CLIENT_ONLY     0x00000040  // Server side functionality not available 
#define SECPKG_FLAG_EXTENDED_ERROR  0x00000080  // Supports extended error msgs 
#define SECPKG_FLAG_IMPERSONATION   0x00000100  // Supports impersonation 
#define SECPKG_FLAG_ACCEPT_WIN32_NAME   0x00000200  // Accepts Win32 names 
#define SECPKG_FLAG_STREAM          0x00000400  // Supports stream semantics 
#define SECPKG_FLAG_NEGOTIABLE      0x00000800  // Can be used by the negotiate package 
#define SECPKG_FLAG_GSS_COMPATIBLE  0x00001000  // GSS Compatibility Available 
#define SECPKG_FLAG_LOGON           0x00002000  // Supports common LsaLogonUser 
 
 
#define SECPKG_ID_NONE      0xFFFF 
 
 
// 
// SecBuffer 
// 
//  Generic memory descriptors for buffers passed in to the security 
//  API 
// 
 
typedef struct _SecBuffer { 
    unsigned long cbBuffer;             // Size of the buffer, in bytes 
    unsigned long BufferType;           // Type of the buffer (below) 
    void SEC_FAR * pvBuffer;            // Pointer to the buffer 
} SecBuffer, SEC_FAR * PSecBuffer; 
 
typedef struct _SecBufferDesc { 
    unsigned long ulVersion;            // Version number 
    unsigned long cBuffers;             // Number of buffers 
#ifdef MIDL_PASS 
    [size_is(cBuffers)] 
#endif 
    PSecBuffer pBuffers;                // Pointer to array of buffers 
} SecBufferDesc, SEC_FAR * PSecBufferDesc; 
 
#define SECBUFFER_VERSION           0 
 
#define SECBUFFER_EMPTY             0   // Undefined, replaced by provider 
#define SECBUFFER_DATA              1   // Packet data 
#define SECBUFFER_TOKEN             2   // Security token 
#define SECBUFFER_PKG_PARAMS        3   // Package specific parameters 
#define SECBUFFER_MISSING           4   // Missing Data indicator 
#define SECBUFFER_EXTRA             5   // Extra data 
#define SECBUFFER_STREAM_TRAILER    6   // Security Trailer 
#define SECBUFFER_STREAM_HEADER     7   // Security Header 
#define SECBUFFER_NEGOTIATION_INFO  8   // Hints from the negotiation pkg 
 
#define SECBUFFER_ATTRMASK          0xF0000000 
#define SECBUFFER_READONLY          0x80000000  // Buffer is read-only 
#define SECBUFFER_RESERVED          0x40000000 
 
typedef struct _SEC_NEGOTIATION_INFO { 
    unsigned long       Size;           // Size of this structure 
    unsigned long       NameLength;     // Length of name hint 
    SEC_WCHAR SEC_FAR * Name;           // Name hint 
    void SEC_FAR *      Reserved;       // Reserved 
} SEC_NEGOTIATION_INFO, SEC_FAR * PSEC_NEGOTIATION_INFO ; 
 
// 
//  Data Representation Constant: 
// 
#define SECURITY_NATIVE_DREP        0x00000010 
#define SECURITY_NETWORK_DREP       0x00000000 
 
// 
//  Credential Use Flags 
// 
#define SECPKG_CRED_INBOUND         0x00000001 
#define SECPKG_CRED_OUTBOUND        0x00000002 
#define SECPKG_CRED_BOTH            0x00000003 
 
// 
//  InitializeSecurityContext Requirement and return flags: 
// 
 
#define ISC_REQ_DELEGATE                0x00000001 
#define ISC_REQ_MUTUAL_AUTH             0x00000002 
#define ISC_REQ_REPLAY_DETECT           0x00000004 
#define ISC_REQ_SEQUENCE_DETECT         0x00000008 
#define ISC_REQ_CONFIDENTIALITY         0x00000010 
#define ISC_REQ_USE_SESSION_KEY         0x00000020 
#define ISC_REQ_PROMPT_FOR_CREDS        0x00000040 
#define ISC_REQ_USE_SUPPLIED_CREDS      0x00000080 
#define ISC_REQ_ALLOCATE_MEMORY         0x00000100 
#define ISC_REQ_USE_DCE_STYLE           0x00000200 
#define ISC_REQ_DATAGRAM                0x00000400 
#define ISC_REQ_CONNECTION              0x00000800 
#define ISC_REQ_CALL_LEVEL              0x00001000 
#define ISC_REQ_EXTENDED_ERROR          0x00004000 
#define ISC_REQ_STREAM                  0x00008000 
#define ISC_REQ_INTEGRITY               0x00010000 
#define ISC_REQ_IDENTIFY                0x00020000 
#define ISC_REQ_NULL_SESSION            0x00040000 
 
#define ISC_RET_DELEGATE                0x00000001 
#define ISC_RET_MUTUAL_AUTH             0x00000002 
#define ISC_RET_REPLAY_DETECT           0x00000004 
#define ISC_RET_SEQUENCE_DETECT         0x00000008 
#define ISC_RET_CONFIDENTIALITY         0x00000010 
#define ISC_RET_USE_SESSION_KEY         0x00000020 
#define ISC_RET_USED_COLLECTED_CREDS    0x00000040 
#define ISC_RET_USED_SUPPLIED_CREDS     0x00000080 
#define ISC_RET_ALLOCATED_MEMORY        0x00000100 
#define ISC_RET_USED_DCE_STYLE          0x00000200 
#define ISC_RET_DATAGRAM                0x00000400 
#define ISC_RET_CONNECTION              0x00000800 
#define ISC_RET_INTERMEDIATE_RETURN     0x00001000 
#define ISC_RET_CALL_LEVEL              0x00002000 
#define ISC_RET_EXTENDED_ERROR          0x00004000 
#define ISC_RET_STREAM                  0x00008000 
#define ISC_RET_INTEGRITY               0x00010000 
#define ISC_RET_IDENTIFY                0x00020000 
#define ISC_RET_NULL_SESSION            0x00040000 
 
#define ASC_REQ_DELEGATE                0x00000001 
#define ASC_REQ_MUTUAL_AUTH             0x00000002 
#define ASC_REQ_REPLAY_DETECT           0x00000004 
#define ASC_REQ_SEQUENCE_DETECT         0x00000008 
#define ASC_REQ_CONFIDENTIALITY         0x00000010 
#define ASC_REQ_USE_SESSION_KEY         0x00000020 
#define ASC_REQ_ALLOCATE_MEMORY         0x00000100 
#define ASC_REQ_USE_DCE_STYLE           0x00000200 
#define ASC_REQ_DATAGRAM                0x00000400 
#define ASC_REQ_CONNECTION              0x00000800 
#define ASC_REQ_CALL_LEVEL              0x00001000 
#define ASC_REQ_EXTENDED_ERROR          0x00008000 
#define ASC_REQ_STREAM                  0x00010000 
#define ASC_REQ_INTEGRITY               0x00020000 
#define ASC_REQ_LICENSING               0x00040000 
#define ASC_REQ_IDENTIFY                0x00080000 
#define ASC_REQ_ALLOW_NULL_SESSION      0x00100000 
 
#define ASC_RET_DELEGATE                0x00000001 
#define ASC_RET_MUTUAL_AUTH             0x00000002 
#define ASC_RET_REPLAY_DETECT           0x00000004 
#define ASC_RET_SEQUENCE_DETECT         0x00000008 
#define ASC_RET_CONFIDENTIALITY         0x00000010 
#define ASC_RET_USE_SESSION_KEY         0x00000020 
#define ASC_RET_ALLOCATED_MEMORY        0x00000100 
#define ASC_RET_USED_DCE_STYLE          0x00000200 
#define ASC_RET_DATAGRAM                0x00000400 
#define ASC_RET_CONNECTION              0x00000800 
#define ASC_RET_CALL_LEVEL              0x00002000 // skipped 1000 to be like ISC_ 
#define ASC_RET_THIRD_LEG_FAILED        0x00004000 
#define ASC_RET_EXTENDED_ERROR          0x00008000 
#define ASC_RET_STREAM                  0x00010000 
#define ASC_RET_INTEGRITY               0x00020000 
#define ASC_RET_LICENSING               0x00040000 
#define ASC_RET_IDENTIFY                0x00080000 
#define ASC_RET_NULL_SESSION            0x00100000 
 
// 
//  Security Credentials Attributes: 
// 
 
#define SECPKG_CRED_ATTR_NAMES 1 
 
typedef struct _SecPkgCredentials_NamesW 
{ 
    SEC_WCHAR SEC_FAR * sUserName; 
} SecPkgCredentials_NamesW, SEC_FAR * PSecPkgCredentials_NamesW; 
 
// end_ntifs 
 
typedef struct _SecPkgCredentials_NamesA 
{ 
    SEC_CHAR SEC_FAR * sUserName; 
} SecPkgCredentials_NamesA, SEC_FAR * PSecPkgCredentials_NamesA; 
 
#ifdef UNICODE 
#  define SecPkgCredentials_Names SecPkgCredentials_NamesW      // ntifs 
#  define PSecPkgCredentials_Names PSecPkgCredentials_NamesW    // ntifs 
#else 
#  define SecPkgCredentials_Names SecPkgCredentials_NamesA 
#  define PSecPkgCredentials_Names PSecPkgCredentials_NamesA 
#endif // !UNICODE 
 
// begin_ntifs 
 
// 
//  Security Context Attributes: 
// 
 
#define SECPKG_ATTR_SIZES           0 
#define SECPKG_ATTR_NAMES           1 
#define SECPKG_ATTR_LIFESPAN        2 
#define SECPKG_ATTR_DCE_INFO        3 
#define SECPKG_ATTR_STREAM_SIZES    4 
#define SECPKG_ATTR_KEY_INFO        5 
#define SECPKG_ATTR_AUTHORITY       6 
#define SECPKG_ATTR_PROTO_INFO      7 
#define SECPKG_ATTR_PASSWORD_EXPIRY 8 
#define SECPKG_ATTR_SESSION_KEY     9 
#define SECPKG_ATTR_PACKAGE_INFO    10 
 
typedef struct _SecPkgContext_Sizes 
{ 
    unsigned long cbMaxToken; 
    unsigned long cbMaxSignature; 
    unsigned long cbBlockSize; 
    unsigned long cbSecurityTrailer; 
} SecPkgContext_Sizes, SEC_FAR * PSecPkgContext_Sizes; 
 
typedef struct _SecPkgContext_StreamSizes 
{ 
    unsigned long   cbHeader; 
    unsigned long   cbTrailer; 
    unsigned long   cbMaximumMessage; 
    unsigned long   cBuffers; 
    unsigned long   cbBlockSize; 
} SecPkgContext_StreamSizes, * PSecPkgContext_StreamSizes; 
 
typedef struct _SecPkgContext_NamesW 
{ 
    SEC_WCHAR SEC_FAR * sUserName; 
} SecPkgContext_NamesW, SEC_FAR * PSecPkgContext_NamesW; 
 
// end_ntifs 
 
typedef struct _SecPkgContext_NamesA 
{ 
    SEC_CHAR SEC_FAR * sUserName; 
} SecPkgContext_NamesA, SEC_FAR * PSecPkgContext_NamesA; 
 
#ifdef UNICODE 
#  define SecPkgContext_Names SecPkgContext_NamesW          // ntifs 
#  define PSecPkgContext_Names PSecPkgContext_NamesW        // ntifs 
#else 
#  define SecPkgContext_Names SecPkgContext_NamesA 
#  define PSecPkgContext_Names PSecPkgContext_NamesA 
#endif // !UNICODE 
 
// begin_ntifs 
 
typedef struct _SecPkgContext_Lifespan 
{ 
    TimeStamp tsStart; 
    TimeStamp tsExpiry; 
} SecPkgContext_Lifespan, SEC_FAR * PSecPkgContext_Lifespan; 
 
typedef struct _SecPkgContext_DceInfo 
{ 
    unsigned long AuthzSvc; 
    void SEC_FAR * pPac; 
} SecPkgContext_DceInfo, SEC_FAR * PSecPkgContext_DceInfo; 
 
// end_ntifs 
 
typedef struct _SecPkgContext_KeyInfoA 
{ 
    SEC_CHAR SEC_FAR *  sSignatureAlgorithmName; 
    SEC_CHAR SEC_FAR *  sEncryptAlgorithmName; 
    unsigned long       KeySize; 
    unsigned long       SignatureAlgorithm; 
    unsigned long       EncryptAlgorithm; 
} SecPkgContext_KeyInfoA, SEC_FAR * PSecPkgContext_KeyInfoA; 
 
// begin_ntifs 
 
typedef struct _SecPkgContext_KeyInfoW 
{ 
    SEC_WCHAR SEC_FAR * sSignatureAlgorithmName; 
    SEC_WCHAR SEC_FAR * sEncryptAlgorithmName; 
    unsigned long       KeySize; 
    unsigned long       SignatureAlgorithm; 
    unsigned long       EncryptAlgorithm; 
} SecPkgContext_KeyInfoW, SEC_FAR * PSecPkgContext_KeyInfoW; 
 
// end_ntifs 
 
#ifdef UNICODE 
#define SecPkgContext_KeyInfo   SecPkgContext_KeyInfoW      // ntifs 
#define PSecPkgContext_KeyInfo  PSecPkgContext_KeyInfoW     // ntifs 
#else 
#define SecPkgContext_KeyInfo   SecPkgContext_KeyInfoA 
#define PSecPkgContext_KeyInfo  PSecPkgContext_KeyInfoA 
#endif 
 
typedef struct _SecPkgContext_AuthorityA 
{ 
    SEC_CHAR SEC_FAR *  sAuthorityName; 
} SecPkgContext_AuthorityA, * PSecPkgContext_AuthorityA; 
 
// begin_ntifs 
 
typedef struct _SecPkgContext_AuthorityW 
{ 
    SEC_WCHAR SEC_FAR * sAuthorityName; 
} SecPkgContext_AuthorityW, * PSecPkgContext_AuthorityW; 
 
// end_ntifs 
 
#ifdef UNICODE 
#define SecPkgContext_Authority SecPkgContext_AuthorityW        // ntifs 
#define PSecPkgContext_Authority    PSecPkgContext_AuthorityW   // ntifs 
#else 
#define SecPkgContext_Authority SecPkgContext_AuthorityA 
#define PSecPkgContext_Authority    PSecPkgContext_AuthorityA 
#endif 
 
typedef struct _SecPkgContext_ProtoInfoA 
{ 
    SEC_CHAR SEC_FAR *  sProtocolName; 
    unsigned long       majorVersion; 
    unsigned long       minorVersion; 
} SecPkgContext_ProtoInfoA, SEC_FAR * PSecPkgContext_ProtoInfoA; 
 
// begin_ntifs 
 
typedef struct _SecPkgContext_ProtoInfoW 
{ 
    SEC_WCHAR SEC_FAR * sProtocolName; 
    unsigned long       majorVersion; 
    unsigned long       minorVersion; 
} SecPkgContext_ProtoInfoW, SEC_FAR * PSecPkgContext_ProtoInfoW; 
 
// end_ntifs 
 
#ifdef UNICODE 
#define SecPkgContext_ProtoInfo   SecPkgContext_ProtoInfoW      // ntifs 
#define PSecPkgContext_ProtoInfo  PSecPkgContext_ProtoInfoW     // ntifs 
#else 
#define SecPkgContext_ProtoInfo   SecPkgContext_ProtoInfoA 
#define PSecPkgContext_ProtoInfo  PSecPkgContext_ProtoInfoA 
#endif 
 
// begin_ntifs 
 
typedef struct _SecPkgContext_PasswordExpiry 
{ 
    TimeStamp tsPasswordExpires; 
} SecPkgContext_PasswordExpiry, SEC_FAR * PSecPkgContext_PasswordExpiry; 
 
typedef struct _SecPkgContext_SessionKey 
{ 
    unsigned long SessionKeyLength; 
    unsigned char SEC_FAR * SessionKey; 
} SecPkgContext_SessionKey, *PSecPkgContext_SessionKey; 
 
// end_ntifs 
// begin_ntifs 
 
 
typedef struct _SecPkgContext_PackageInfoW 
{ 
    PSecPkgInfoW PackageInfo; 
} SecPkgContext_PackageInfoW, SEC_FAR * PSecPkgContext_PackageInfoW; 
 
// end_ntifs 
 
typedef struct _SecPkgContext_PackageInfoA 
{ 
    PSecPkgInfoA PackageInfo; 
} SecPkgContext_PackageInfoA, SEC_FAR * PSecPkgContext_PackageInfoA; 
 
 
#ifdef UNICODE 
#define SecPkgContext_PackageInfo   SecPkgContext_PackageInfoW      // ntifs 
#define PSecPkgContext_PackageInfo  PSecPkgContext_PackageInfoW     // ntifs 
#else 
#define SecPkgContext_PackageInfo   SecPkgContext_PackageInfoA 
#define PSecPkgContext_PackageInfo  PSecPkgContext_PackageInfoA 
#endif 
 
// begin_ntifs 
 
typedef void 
(SEC_ENTRY SEC_FAR * SEC_GET_KEY_FN) ( 
    void SEC_FAR * Arg,                 // Argument passed in 
    void SEC_FAR * Principal,           // Principal ID 
    unsigned long KeyVer,               // Key Version 
    void SEC_FAR * SEC_FAR * Key,       // Returned ptr to key 
    SECURITY_STATUS SEC_FAR * Status    // returned status 
    ); 
 
// 
// Flags for ExportSecurityContext 
// 
 
#define SECPKG_CONTEXT_EXPORT_RESET_NEW         0x00000001      // New context is reset to initial state 
#define SECPKG_CONTEXT_EXPORT_DELETE_OLD        0x00000002      // Old context is deleted during export 
 
 
SECURITY_STATUS SEC_ENTRY 
AcquireCredentialsHandleW( 
#if ISSP_MODE == 0                      // For Kernel mode 
    PSECURITY_STRING pPrincipal, 
    PSECURITY_STRING pPackage, 
#else 
    SEC_WCHAR SEC_FAR * pszPrincipal,   // Name of principal 
    SEC_WCHAR SEC_FAR * pszPackage,     // Name of package 
#endif 
    unsigned long fCredentialUse,       // Flags indicating use 
    void SEC_FAR * pvLogonId,           // Pointer to logon ID 
    void SEC_FAR * pAuthData,           // Package specific data 
    SEC_GET_KEY_FN pGetKeyFn,           // Pointer to GetKey() func 
    void SEC_FAR * pvGetKeyArgument,    // Value to pass to GetKey() 
    PCredHandle phCredential,           // (out) Cred Handle 
    PTimeStamp ptsExpiry                // (out) Lifetime (optional) 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * ACQUIRE_CREDENTIALS_HANDLE_FN_W)( 
#if ISSP_MODE == 0 
    PSECURITY_STRING, 
    PSECURITY_STRING, 
#else 
    SEC_WCHAR SEC_FAR *, 
    SEC_WCHAR SEC_FAR *, 
#endif 
    unsigned long, 
    void SEC_FAR *, 
    void SEC_FAR *, 
    SEC_GET_KEY_FN, 
    void SEC_FAR *, 
    PCredHandle, 
    PTimeStamp); 
 
// end_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
AcquireCredentialsHandleA( 
    SEC_CHAR SEC_FAR * pszPrincipal,    // Name of principal 
    SEC_CHAR SEC_FAR * pszPackage,      // Name of package 
    unsigned long fCredentialUse,       // Flags indicating use 
    void SEC_FAR * pvLogonId,           // Pointer to logon ID 
    void SEC_FAR * pAuthData,           // Package specific data 
    SEC_GET_KEY_FN pGetKeyFn,           // Pointer to GetKey() func 
    void SEC_FAR * pvGetKeyArgument,    // Value to pass to GetKey() 
    PCredHandle phCredential,           // (out) Cred Handle 
    PTimeStamp ptsExpiry                // (out) Lifetime (optional) 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * ACQUIRE_CREDENTIALS_HANDLE_FN_A)( 
    SEC_CHAR SEC_FAR *, 
    SEC_CHAR SEC_FAR *, 
    unsigned long, 
    void SEC_FAR *, 
    void SEC_FAR *, 
    SEC_GET_KEY_FN, 
    void SEC_FAR *, 
    PCredHandle, 
    PTimeStamp); 
 
#ifdef UNICODE 
#  define AcquireCredentialsHandle AcquireCredentialsHandleW            // ntifs 
#  define ACQUIRE_CREDENTIALS_HANDLE_FN ACQUIRE_CREDENTIALS_HANDLE_FN_W // ntifs 
#else 
#  define AcquireCredentialsHandle AcquireCredentialsHandleA 
#  define ACQUIRE_CREDENTIALS_HANDLE_FN ACQUIRE_CREDENTIALS_HANDLE_FN_A 
#endif // !UNICODE 
 
// begin_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
FreeCredentialsHandle( 
    PCredHandle phCredential            // Handle to free 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * FREE_CREDENTIALS_HANDLE_FN)( 
    PCredHandle ); 
 
#ifdef WIN32_CHICAGO 
SECURITY_STATUS SEC_ENTRY 
SspiLogonUserW( 
    SEC_WCHAR SEC_FAR * pszPackage,     // Name of package 
    SEC_WCHAR SEC_FAR * pszUserName,     // Name of package 
    SEC_WCHAR SEC_FAR * pszDomainName,     // Name of package 
    SEC_WCHAR SEC_FAR * pszPassword      // Name of package 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * SSPI_LOGON_USER_FN_W)( 
    SEC_CHAR SEC_FAR *, 
    SEC_CHAR SEC_FAR *, 
    SEC_CHAR SEC_FAR *, 
    SEC_CHAR SEC_FAR *); 
 
SECURITY_STATUS SEC_ENTRY 
SspiLogonUserA( 
    SEC_CHAR SEC_FAR * pszPackage,     // Name of package 
    SEC_CHAR SEC_FAR * pszUserName,     // Name of package 
    SEC_CHAR SEC_FAR * pszDomainName,     // Name of package 
    SEC_CHAR SEC_FAR * pszPassword      // Name of package 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * SSPI_LOGON_USER_FN_A)( 
    SEC_CHAR SEC_FAR *, 
    SEC_CHAR SEC_FAR *, 
    SEC_CHAR SEC_FAR *, 
    SEC_CHAR SEC_FAR *); 
 
#ifdef UNICODE 
#define SspiLogonUser SspiLogonUserW            // ntifs 
#define SSPI_LOGON_USER_FN SSPI_LOGON_USER_FN_W 
#else 
#define SspiLogonUser SspiLogonUserA 
#define SSPI_LOGON_USER_FN SSPI_LOGON_USER_FN_A 
#endif // !UNICODE 
#endif // WIN32_CHICAGO 
 
// end_ntifs 
 
// begin_ntifs 
 
//////////////////////////////////////////////////////////////////////// 
/// 
/// Context Management Functions 
/// 
//////////////////////////////////////////////////////////////////////// 
 
SECURITY_STATUS SEC_ENTRY 
InitializeSecurityContextW( 
    PCredHandle phCredential,               // Cred to base context 
    PCtxtHandle phContext,                  // Existing context (OPT) 
#if ISSP_MODE == 0 
    PSECURITY_STRING pTargetName, 
#else 
    SEC_WCHAR SEC_FAR * pszTargetName,      // Name of target 
#endif 
    unsigned long fContextReq,              // Context Requirements 
    unsigned long Reserved1,                // Reserved, MBZ 
    unsigned long TargetDataRep,            // Data rep of target 
    PSecBufferDesc pInput,                  // Input Buffers 
    unsigned long Reserved2,                // Reserved, MBZ 
    PCtxtHandle phNewContext,               // (out) New Context handle 
    PSecBufferDesc pOutput,                 // (inout) Output Buffers 
    unsigned long SEC_FAR * pfContextAttr,  // (out) Context attrs 
    PTimeStamp ptsExpiry                    // (out) Life span (OPT) 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * INITIALIZE_SECURITY_CONTEXT_FN_W)( 
    PCredHandle, 
    PCtxtHandle, 
#if ISSP_MODE == 0 
    PSECURITY_STRING, 
#else 
    SEC_WCHAR SEC_FAR *, 
#endif 
    unsigned long, 
    unsigned long, 
    unsigned long, 
    PSecBufferDesc, 
    unsigned long, 
    PCtxtHandle, 
    PSecBufferDesc, 
    unsigned long SEC_FAR *, 
    PTimeStamp); 
 
// end_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
InitializeSecurityContextA( 
    PCredHandle phCredential,               // Cred to base context 
    PCtxtHandle phContext,                  // Existing context (OPT) 
    SEC_CHAR SEC_FAR * pszTargetName,       // Name of target 
    unsigned long fContextReq,              // Context Requirements 
    unsigned long Reserved1,                // Reserved, MBZ 
    unsigned long TargetDataRep,            // Data rep of target 
    PSecBufferDesc pInput,                  // Input Buffers 
    unsigned long Reserved2,                // Reserved, MBZ 
    PCtxtHandle phNewContext,               // (out) New Context handle 
    PSecBufferDesc pOutput,                 // (inout) Output Buffers 
    unsigned long SEC_FAR * pfContextAttr,  // (out) Context attrs 
    PTimeStamp ptsExpiry                    // (out) Life span (OPT) 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * INITIALIZE_SECURITY_CONTEXT_FN_A)( 
    PCredHandle, 
    PCtxtHandle, 
    SEC_CHAR SEC_FAR *, 
    unsigned long, 
    unsigned long, 
    unsigned long, 
    PSecBufferDesc, 
    unsigned long, 
    PCtxtHandle, 
    PSecBufferDesc, 
    unsigned long SEC_FAR *, 
    PTimeStamp); 
 
#ifdef UNICODE 
#  define InitializeSecurityContext InitializeSecurityContextW              // ntifs 
#  define INITIALIZE_SECURITY_CONTEXT_FN INITIALIZE_SECURITY_CONTEXT_FN_W   // ntifs 
#else 
#  define InitializeSecurityContext InitializeSecurityContextA 
#  define INITIALIZE_SECURITY_CONTEXT_FN INITIALIZE_SECURITY_CONTEXT_FN_A 
#endif // !UNICODE 
 
// begin_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
AcceptSecurityContext( 
    PCredHandle phCredential,               // Cred to base context 
    PCtxtHandle phContext,                  // Existing context (OPT) 
    PSecBufferDesc pInput,                  // Input buffer 
    unsigned long fContextReq,              // Context Requirements 
    unsigned long TargetDataRep,            // Target Data Rep 
    PCtxtHandle phNewContext,               // (out) New context handle 
    PSecBufferDesc pOutput,                 // (inout) Output buffers 
    unsigned long SEC_FAR * pfContextAttr,  // (out) Context attributes 
    PTimeStamp ptsExpiry                    // (out) Life span (OPT) 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * ACCEPT_SECURITY_CONTEXT_FN)( 
    PCredHandle, 
    PCtxtHandle, 
    PSecBufferDesc, 
    unsigned long, 
    unsigned long, 
    PCtxtHandle, 
    PSecBufferDesc, 
    unsigned long SEC_FAR *, 
    PTimeStamp); 
 
 
 
SECURITY_STATUS SEC_ENTRY 
CompleteAuthToken( 
    PCtxtHandle phContext,              // Context to complete 
    PSecBufferDesc pToken               // Token to complete 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * COMPLETE_AUTH_TOKEN_FN)( 
    PCtxtHandle, 
    PSecBufferDesc); 
 
 
SECURITY_STATUS SEC_ENTRY 
ImpersonateSecurityContext( 
    PCtxtHandle phContext               // Context to impersonate 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * IMPERSONATE_SECURITY_CONTEXT_FN)( 
    PCtxtHandle); 
 
 
 
SECURITY_STATUS SEC_ENTRY 
RevertSecurityContext( 
    PCtxtHandle phContext               // Context from which to re 
    ); 
 
typedef SECURITY_STATUS 

(SEC_ENTRY * REVERT_SECURITY_CONTEXT_FN)( 
    PCtxtHandle); 
 
 
SECURITY_STATUS SEC_ENTRY 
QuerySecurityContextToken( 
    PCtxtHandle phContext, 
    void SEC_FAR * SEC_FAR * Token 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * QUERY_SECURITY_CONTEXT_TOKEN_FN)( 
    PCtxtHandle, void SEC_FAR * SEC_FAR *); 
 
 
 
SECURITY_STATUS SEC_ENTRY 
DeleteSecurityContext( 
    PCtxtHandle phContext               // Context to delete 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * DELETE_SECURITY_CONTEXT_FN)( 
    PCtxtHandle); 
 
 
 
SECURITY_STATUS SEC_ENTRY 
ApplyControlToken( 
    PCtxtHandle phContext,              // Context to modify 
    PSecBufferDesc pInput               // Input token to apply 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * APPLY_CONTROL_TOKEN_FN)( 
    PCtxtHandle, PSecBufferDesc); 
 
 
 
SECURITY_STATUS SEC_ENTRY 
QueryContextAttributesW( 
    PCtxtHandle phContext,              // Context to query 
    unsigned long ulAttribute,          // Attribute to query 
    void SEC_FAR * pBuffer              // Buffer for attributes 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * QUERY_CONTEXT_ATTRIBUTES_FN_W)( 
    PCtxtHandle, 
    unsigned long, 
    void SEC_FAR *); 
 
// end_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
QueryContextAttributesA( 
    PCtxtHandle phContext,              // Context to query 
    unsigned long ulAttribute,          // Attribute to query 
    void SEC_FAR * pBuffer              // Buffer for attributes 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * QUERY_CONTEXT_ATTRIBUTES_FN_A)( 
    PCtxtHandle, 
    unsigned long, 
    void SEC_FAR *); 
 
#ifdef UNICODE 
#  define QueryContextAttributes QueryContextAttributesW            // ntifs 
#  define QUERY_CONTEXT_ATTRIBUTES_FN QUERY_CONTEXT_ATTRIBUTES_FN_W // ntifs 
#else 
#  define QueryContextAttributes QueryContextAttributesA 
#  define QUERY_CONTEXT_ATTRIBUTES_FN QUERY_CONTEXT_ATTRIBUTES_FN_A 
#endif // !UNICODE 
 
// begin_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
QueryCredentialsAttributesW( 
    PCredHandle phCredential,              // Credential to query 
    unsigned long ulAttribute,          // Attribute to query 
    void SEC_FAR * pBuffer              // Buffer for attributes 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * QUERY_CREDENTIALS_ATTRIBUTES_FN_W)( 
    PCredHandle, 
    unsigned long, 
    void SEC_FAR *); 
 
// end_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
QueryCredentialsAttributesA( 
    PCredHandle phCredential,              // Credential to query 
    unsigned long ulAttribute,          // Attribute to query 
    void SEC_FAR * pBuffer              // Buffer for attributes 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * QUERY_CREDENTIALS_ATTRIBUTES_FN_A)( 
    PCredHandle, 
    unsigned long, 
    void SEC_FAR *); 
 
#ifdef UNICODE 
#  define QueryCredentialsAttributes QueryCredentialsAttributesW            // ntifs 
#  define QUERY_CREDENTIALS_ATTRIBUTES_FN QUERY_CREDENTIALS_ATTRIBUTES_FN_W // ntifs 
#else 
#  define QueryCredentialsAttributes QueryCredentialsAttributesA 
#  define QUERY_CREDENTIALS_ATTRIBUTES_FN QUERY_CREDENTIALS_ATTRIBUTES_FN_A 
#endif // !UNICODE 
 
// begin_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
FreeContextBuffer( 
    void SEC_FAR * pvContextBuffer      // buffer to free 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * FREE_CONTEXT_BUFFER_FN)( 
    void SEC_FAR *); 
 
// end_ntifs 
 
// begin_ntifs 
/////////////////////////////////////////////////////////////////// 
//// 
////    Message Support API 
//// 
////////////////////////////////////////////////////////////////// 
 
SECURITY_STATUS SEC_ENTRY 
MakeSignature( 
    PCtxtHandle phContext,              // Context to use 
    unsigned long fQOP,                 // Quality of Protection 
    PSecBufferDesc pMessage,            // Message to sign 
    unsigned long MessageSeqNo          // Message Sequence Num. 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * MAKE_SIGNATURE_FN)( 
    PCtxtHandle, 
    unsigned long, 
    PSecBufferDesc, 
    unsigned long); 
 
 
 
SECURITY_STATUS SEC_ENTRY 
VerifySignature( 
    PCtxtHandle phContext,              // Context to use 
    PSecBufferDesc pMessage,            // Message to verify 
    unsigned long MessageSeqNo,         // Sequence Num. 
    unsigned long SEC_FAR * pfQOP       // QOP used 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * VERIFY_SIGNATURE_FN)( 
    PCtxtHandle, 
    PSecBufferDesc, 
    unsigned long, 
    unsigned long SEC_FAR *); 
 
 
SECURITY_STATUS SEC_ENTRY 
EncryptMessage( PCtxtHandle         phContext, 
                unsigned long       fQOP, 
                PSecBufferDesc      pMessage, 
                unsigned long       MessageSeqNo); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * ENCRYPT_MESSAGE_FN)( 
    PCtxtHandle, unsigned long, PSecBufferDesc, unsigned long); 
 
 
SECURITY_STATUS SEC_ENTRY 
DecryptMessage( PCtxtHandle         phContext, 
                PSecBufferDesc      pMessage, 
                unsigned long       MessageSeqNo, 
                unsigned long *     pfQOP); 
 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * DECRYPT_MESSAGE_FN)( 
    PCtxtHandle, PSecBufferDesc, unsigned long, 
    unsigned long SEC_FAR *); 
 
 
// end_ntifs 
 
// begin_ntifs 
/////////////////////////////////////////////////////////////////////////// 
//// 
////    Misc. 
//// 
/////////////////////////////////////////////////////////////////////////// 
 
 
SECURITY_STATUS SEC_ENTRY 
EnumerateSecurityPackagesW( 
    unsigned long SEC_FAR * pcPackages,     // Receives num. packages 
    PSecPkgInfoW SEC_FAR * ppPackageInfo    // Receives array of info 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * ENUMERATE_SECURITY_PACKAGES_FN_W)( 
    unsigned long SEC_FAR *, 
    PSecPkgInfoW SEC_FAR *); 
 
// end_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
EnumerateSecurityPackagesA( 
    unsigned long SEC_FAR * pcPackages,     // Receives num. packages 
    PSecPkgInfoA SEC_FAR * ppPackageInfo    // Receives array of info 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * ENUMERATE_SECURITY_PACKAGES_FN_A)( 
    unsigned long SEC_FAR *, 
    PSecPkgInfoA SEC_FAR *); 
 
#ifdef UNICODE 
#  define EnumerateSecurityPackages EnumerateSecurityPackagesW              // ntifs 
#  define ENUMERATE_SECURITY_PACKAGES_FN ENUMERATE_SECURITY_PACKAGES_FN_W   // ntifs 
#else 
#  define EnumerateSecurityPackages EnumerateSecurityPackagesA 
#  define ENUMERATE_SECURITY_PACKAGES_FN ENUMERATE_SECURITY_PACKAGES_FN_A 
#endif // !UNICODE 
 
// begin_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
QuerySecurityPackageInfoW( 
#if ISSP_MODE == 0 
    PSECURITY_STRING pPackageName, 
#else 
    SEC_WCHAR SEC_FAR * pszPackageName,     // Name of package 
#endif 
    PSecPkgInfoW SEC_FAR *ppPackageInfo              // Receives package info 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * QUERY_SECURITY_PACKAGE_INFO_FN_W)( 
#if ISSP_MODE == 0 
    PSECURITY_STRING, 
#else 
    SEC_WCHAR SEC_FAR *, 
#endif 
    PSecPkgInfoW SEC_FAR *); 
 
// end_ntifs 
 
SECURITY_STATUS SEC_ENTRY 
QuerySecurityPackageInfoA( 
    SEC_CHAR SEC_FAR * pszPackageName,      // Name of package 
    PSecPkgInfoA SEC_FAR *ppPackageInfo              // Receives package info 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * QUERY_SECURITY_PACKAGE_INFO_FN_A)( 
    SEC_CHAR SEC_FAR *, 
    PSecPkgInfoA SEC_FAR *); 
 
#ifdef UNICODE 
#  define QuerySecurityPackageInfo QuerySecurityPackageInfoW                // ntifs 
#  define QUERY_SECURITY_PACKAGE_INFO_FN QUERY_SECURITY_PACKAGE_INFO_FN_W   // ntifs 
#else 
#  define QuerySecurityPackageInfo QuerySecurityPackageInfoA 
#  define QUERY_SECURITY_PACKAGE_INFO_FN QUERY_SECURITY_PACKAGE_INFO_FN_A 
#endif // !UNICODE 
 
 
#if ISSP_MODE == 0 
 
#define DeleteSecurityContextDefer  DeleteSecurityContext 
#define FreeCredentialsHandleDefer  FreeCredentialsHandle 
 
#if 0 
// 
// Deferred mode calls for rdr 
// 
 
SECURITY_STATUS SEC_ENTRY 
DeleteSecurityContextDefer( 
    PCtxtHandle     phContext); 
 
SECURITY_STATUS SEC_ENTRY 
FreeCredentialsHandleDefer( 
    PCredHandle     phCreds); 
 
#endif 
 
#endif 
 
typedef enum _SecDelegationType { 
    SecFull, 
    SecService, 
    SecTree, 
    SecDirectory, 
    SecObject 
} SecDelegationType, * PSecDelegationType; 
 
SECURITY_STATUS SEC_ENTRY 
DelegateSecurityContext( 
    PCtxtHandle         phContext,          // IN Active context to delegate 
#if ISSP_MODE == 0 
    PSECURITY_STRING    pTarget,            // IN Target path 
#else 
    SEC_CHAR SEC_FAR *  pszTarget, 
#endif 
    SecDelegationType   DelegationType,     // IN Type of delegation 
    PTimeStamp          pExpiry,            // IN OPTIONAL time limit 
    PSecBuffer          pPackageParameters, // IN OPTIONAL package specific 
    PSecBufferDesc      pOutput);           // OUT Token for applycontroltoken. 
 
 
/////////////////////////////////////////////////////////////////////////// 
//// 
////    Proxies 
//// 
/////////////////////////////////////////////////////////////////////////// 
 
 
// 
// Proxies are only available on NT platforms 
// 
 
// begin_ntifs 
 
/////////////////////////////////////////////////////////////////////////// 
//// 
////    Context export/import 
//// 
/////////////////////////////////////////////////////////////////////////// 
 
 
 
SECURITY_STATUS SEC_ENTRY 
ExportSecurityContext( 
    PCtxtHandle          phContext,             // (in) context to export 
    ULONG                fFlags,                // (in) option flags 
    PSecBuffer           pPackedContext,        // (out) marshalled context 
    void SEC_FAR * SEC_FAR * pToken                 // (out, optional) token handle for impersonation 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * EXPORT_SECURITY_CONTEXT_FN)( 
    PCtxtHandle, 
    ULONG, 
    PSecBuffer, 
    void SEC_FAR * SEC_FAR * 
    ); 
 
SECURITY_STATUS SEC_ENTRY 
ImportSecurityContextW( 
    SEC_WCHAR SEC_FAR * pszPackage, 
    PSecBuffer           pPackedContext,        // (in) marshalled context 
    void SEC_FAR *       Token,                 // (in, optional) handle to token for context 
    PCtxtHandle          phContext              // (out) new context handle 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * IMPORT_SECURITY_CONTEXT_FN_W)( 
    SEC_WCHAR SEC_FAR *, 
    PSecBuffer, 
    VOID SEC_FAR *, 
    PCtxtHandle 
    ); 
 
// end_ntifs 
SECURITY_STATUS SEC_ENTRY 
ImportSecurityContextA( 
    SEC_CHAR SEC_FAR * pszPackage, 
    PSecBuffer           pPackedContext,        // (in) marshalled context 
    VOID SEC_FAR *       Token,                 // (in, optional) handle to token for context 
    PCtxtHandle          phContext              // (out) new context handle 
    ); 
 
typedef SECURITY_STATUS 
(SEC_ENTRY * IMPORT_SECURITY_CONTEXT_FN_A)( 
    SEC_CHAR SEC_FAR *, 
    PSecBuffer, 
    void SEC_FAR *, 
    PCtxtHandle 
    ); 
 
#ifdef UNICODE 
#  define ImportSecurityContext ImportSecurityContextW              // ntifs 
#  define IMPORT_SECURITY_CONTEXT_FN IMPORT_SECURITY_CONTEXT_FN_W   // ntifs 
#else 
#  define ImportSecurityContext ImportSecurityContextA 
#  define IMPORT_SECURITY_CONTEXT_FN IMPORT_SECURITY_CONTEXT_FN_A 
#endif // !UNICODE 
 
 
/////////////////////////////////////////////////////////////////////////////// 
//// 
////  Fast access for RPC: 
//// 
/////////////////////////////////////////////////////////////////////////////// 
 
#define SECURITY_ENTRYPOINT_ANSIW "InitSecurityInterfaceW" 
#define SECURITY_ENTRYPOINT_ANSIA "InitSecurityInterfaceW" 
#define SECURITY_ENTRYPOINTW SEC_TEXT("InitSecurityInterfaceW")     // ntifs 
#define SECURITY_ENTRYPOINTA SEC_TEXT("InitSecurityInterfaceA") 
#define SECURITY_ENTRYPOINT16 "INITSECURITYINTERFACEA" 
 
#ifdef SECURITY_WIN32 
#  ifdef UNICODE 
#    define SECURITY_ENTRYPOINT SECURITY_ENTRYPOINTW                // ntifs 
#    define SECURITY_ENTRYPOINT_ANSI SECURITY_ENTRYPOINT_ANSIW 
#  else // UNICODE 
#    define SECURITY_ENTRYPOINT SECURITY_ENTRYPOINTA 
#    define SECURITY_ENTRYPOINT_ANSI SECURITY_ENTRYPOINT_ANSIA 
#  endif // UNICODE 
#else // SECURITY_WIN32 
#  define SECURITY_ENTRYPOINT SECURITY_ENTRYPOINT16 
#  define SECURITY_ENTRYPOINT_ANSI SECURITY_ENTRYPOINT16 
#endif // SECURITY_WIN32 
 
// begin_ntifs 
 
typedef struct _SECURITY_FUNCTION_TABLE_W { 
    unsigned long                       dwVersion; 
    ENUMERATE_SECURITY_PACKAGES_FN_W    EnumerateSecurityPackagesW; 
    QUERY_CREDENTIALS_ATTRIBUTES_FN_W   QueryCredentialsAttributesW; 
    ACQUIRE_CREDENTIALS_HANDLE_FN_W     AcquireCredentialsHandleW; 
    FREE_CREDENTIALS_HANDLE_FN          FreeCredentialHandle; 
#ifndef WIN32_CHICAGO 
    void SEC_FAR *                      Reserved2; 
#else // WIN32_CHICAGO 
    SSPI_LOGON_USER_FN                       SspiLogonUserW; 
#endif // WIN32_CHICAGO 
    INITIALIZE_SECURITY_CONTEXT_FN_W    InitializeSecurityContextW; 
    ACCEPT_SECURITY_CONTEXT_FN          AcceptSecurityContext; 
    COMPLETE_AUTH_TOKEN_FN              CompleteAuthToken; 
    DELETE_SECURITY_CONTEXT_FN          DeleteSecurityContext; 
    APPLY_CONTROL_TOKEN_FN              ApplyControlToken; 
    QUERY_CONTEXT_ATTRIBUTES_FN_W       QueryContextAttributesW; 
    IMPERSONATE_SECURITY_CONTEXT_FN     ImpersonateSecurityContext; 
    REVERT_SECURITY_CONTEXT_FN          RevertSecurityContext; 
    MAKE_SIGNATURE_FN                   MakeSignature; 
    VERIFY_SIGNATURE_FN                 VerifySignature; 
    FREE_CONTEXT_BUFFER_FN              FreeContextBuffer; 
    QUERY_SECURITY_PACKAGE_INFO_FN_W    QuerySecurityPackageInfoW; 
    void SEC_FAR *                      Reserved3; 
    void SEC_FAR *                      Reserved4; 
    EXPORT_SECURITY_CONTEXT_FN          ExportSecurityContext; 
    IMPORT_SECURITY_CONTEXT_FN_W        ImportSecurityContextW; 
    void SEC_FAR *                      Reserved7; 
    void SEC_FAR *                      Reserved8; 
    QUERY_SECURITY_CONTEXT_TOKEN_FN     QuerySecurityContextToken; 
    ENCRYPT_MESSAGE_FN                  EncryptMessage; 
    DECRYPT_MESSAGE_FN                  DecryptMessage; 
} SecurityFunctionTableW, SEC_FAR * PSecurityFunctionTableW; 
 
// end_ntifs 
 
typedef struct _SECURITY_FUNCTION_TABLE_A { 
    unsigned long                       dwVersion; 
    ENUMERATE_SECURITY_PACKAGES_FN_A    EnumerateSecurityPackagesA; 
    QUERY_CREDENTIALS_ATTRIBUTES_FN_A   QueryCredentialsAttributesA; 
    ACQUIRE_CREDENTIALS_HANDLE_FN_A     AcquireCredentialsHandleA; 
    FREE_CREDENTIALS_HANDLE_FN          FreeCredentialHandle; 
#ifndef WIN32_CHICAGO 
    void SEC_FAR *                      Reserved2; 
#else // WIN32_CHICAGO 
    SSPI_LOGON_USER_FN                       SspiLogonUserA; 
#endif // WIN32_CHICAGO 
    INITIALIZE_SECURITY_CONTEXT_FN_A    InitializeSecurityContextA; 
    ACCEPT_SECURITY_CONTEXT_FN          AcceptSecurityContext; 
    COMPLETE_AUTH_TOKEN_FN              CompleteAuthToken; 
    DELETE_SECURITY_CONTEXT_FN          DeleteSecurityContext; 
    APPLY_CONTROL_TOKEN_FN              ApplyControlToken; 
    QUERY_CONTEXT_ATTRIBUTES_FN_A       QueryContextAttributesA; 
    IMPERSONATE_SECURITY_CONTEXT_FN     ImpersonateSecurityContext; 
    REVERT_SECURITY_CONTEXT_FN          RevertSecurityContext; 
    MAKE_SIGNATURE_FN                   MakeSignature; 
    VERIFY_SIGNATURE_FN                 VerifySignature; 
    FREE_CONTEXT_BUFFER_FN              FreeContextBuffer; 
    QUERY_SECURITY_PACKAGE_INFO_FN_A    QuerySecurityPackageInfoA; 
    void SEC_FAR *                      Reserved3; 
    void SEC_FAR *                      Reserved4; 
    EXPORT_SECURITY_CONTEXT_FN          ExportSecurityContext; 
    IMPORT_SECURITY_CONTEXT_FN_A        ImportSecurityContextA; 
   void SEC_FAR *                      Reserved7; 
    void SEC_FAR *                      Reserved8; 
    QUERY_SECURITY_CONTEXT_TOKEN_FN     QuerySecurityContextToken; 
    ENCRYPT_MESSAGE_FN                  EncryptMessage; 
    DECRYPT_MESSAGE_FN                  DecryptMessage; 
} SecurityFunctionTableA, SEC_FAR * PSecurityFunctionTableA; 
 
#ifdef UNICODE 
#  define SecurityFunctionTable SecurityFunctionTableW 
#  define PSecurityFunctionTable PSecurityFunctionTableW 
#else 
#  define SecurityFunctionTable SecurityFunctionTableA 
#  define PSecurityFunctionTable PSecurityFunctionTableA 
#endif // !UNICODE 
 
#define SECURITY_ 
 
#define SECURITY_SUPPORT_PROVIDER_INTERFACE_VERSION     1 
 
 
PSecurityFunctionTableA SEC_ENTRY 
InitSecurityInterfaceA( 
    void 
    ); 
 
typedef PSecurityFunctionTableA 
(SEC_ENTRY * INIT_SECURITY_INTERFACE_A)(void); 
 
// begin_ntifs 
 
PSecurityFunctionTableW SEC_ENTRY 
InitSecurityInterfaceW( 
    void 
    ); 
 
typedef PSecurityFunctionTableW 
(SEC_ENTRY * INIT_SECURITY_INTERFACE_W)(void); 
 
// end_ntifs 
 
#ifdef UNICODE 
#  define InitSecurityInterface InitSecurityInterfaceW          // ntifs 
#  define INIT_SECURITY_INTERFACE INIT_SECURITY_INTERFACE_W     // ntifs 
#else 
#  define InitSecurityInterface InitSecurityInterfaceA 
#  define INIT_SECURITY_INTERFACE INIT_SECURITY_INTERFACE_A 
#endif // !UNICODE 
 
typedef struct _SECURITY_PACKAGE_OPTIONS { 
    unsigned long   Size; 
    unsigned long   Type; 
    unsigned long   Flags; 
    unsigned long   SignatureSize; 
    void SEC_FAR *  Signature; 
} SECURITY_PACKAGE_OPTIONS, SEC_FAR * PSECURITY_PACKAGE_OPTIONS; 
 
#define SECPKG_OPTIONS_TYPE_UNKNOWN 0 
#define SECPKG_OPTIONS_TYPE_LSA     1 
#define SECPKG_OPTIONS_TYPE_SSPI    2 
 
#define SECPKG_OPTIONS_PERMANENT    0x00000001 
 
SECURITY_STATUS 
SEC_ENTRY 
AddSecurityPackageA( 
    SEC_CHAR SEC_FAR *  pszPackageName, 
    SECURITY_PACKAGE_OPTIONS SEC_FAR * Options 
    ); 
 
SECURITY_STATUS 
SEC_ENTRY 
AddSecurityPackageW( 
    SEC_WCHAR SEC_FAR * pszPackageName, 
    SECURITY_PACKAGE_OPTIONS SEC_FAR * Options 
    ); 
 
#ifdef UNICODE 
#define AddSecurityPackage  AddSecurityPackageW 
#else 
#define AddSecurityPackage  AddSecurityPackageA 
#endif 
 
SECURITY_STATUS 
SEC_ENTRY 
DeleteSecurityPackageA( 
    SEC_CHAR SEC_FAR *  pszPackageName ); 
 
SECURITY_STATUS 
SEC_ENTRY 
DeleteSecurityPackageW( 
    SEC_WCHAR SEC_FAR * pszPackageName ); 
 
#ifdef UNICODE 
#define DeleteSecurityPackage   DeleteSecurityPackageW 
#else 
#define DeleteSecurityPackage   DeleteSecurityPackageA 
#endif 
 
 
// 
// Extended Name APIs for NTDS 
// 
 
 
typedef enum 
{ 
    // Examples for the following formats assume a fictitous company 
    // which hooks into the global X.500 and DNS name spaces as follows. 
    // 
    // Enterprise root domain in DNS is 
    // 
    //      widget.com 
    // 
    // Enterprise root domain in X.500 (RFC 1779 format) is 
    // 
    //      O=Widget, C=US 
    // 
    // There exists the child domain 
    // 
    //      engineering.widget.com 
    // 
    // equivalent to 
    // 
    //      OU=Engineering, O=Widget, C=US 
    // 
    // There exists a container within the Engineering domain 
    // 
    //      OU=Software, OU=Engineering, O=Widget, C=US 
    // 
    // There exists the user 
    // 
    //      CN=Spencer Katt, OU=Software, OU=Engineering, O=Widget, C=US 
    // 
    // And this user's downlevel (pre-NTDS) user name is 
    // 
    //      Engineering\SpencerK 
 
    // unknown name type 
    NameUnknown = 0, 
 
    // CN=Spencer Katt, OU=Software, OU=Engineering, O=Widget, C=US 
    NameFullyQualifiedDN = 1, 
 
    // Engineering\SpencerK 
    NameSamCompatible = 2, 
 
    // Probably "Spencer Katt" but could be something else.  I.e. The 
    // display name is not necessarily the defining RDN. 
    NameDisplay = 3, 
 
    // xxx@engineering.widget.com where xxx could be "SpencerK" or 
    // anything else.  Could be multi-valued to handle migration and aliasing. 
    NameDomainSimple = 4, 
 
    // xxx@widget.com where xxx could be "SpencerK" or anything else. 
    // Could be multi-valued to handle migration and aliasing. 
    NameEnterpriseSimple = 5, 
 
    // String-ized GUID as returned by IIDFromString(). 
    // eg: {4fa050f0-f561-11cf-bdd9-00aa003a77b6} 
    NameUniqueId = 6, 
 
    // engineering.widget.com/software/spencer katt 
    NameCanonical = 7 
 
} EXTENDED_NAME_FORMAT, * PEXTENDED_NAME_FORMAT ; 
 
BOOLEAN 
SEC_ENTRY 
GetUserNameExA( 
    EXTENDED_NAME_FORMAT  NameFormat, 
    LPSTR lpNameBuffer, 
    PULONG nSize 
    ); 
BOOLEAN 
SEC_ENTRY 
GetUserNameExW( 
    EXTENDED_NAME_FORMAT NameFormat, 
    LPWSTR lpNameBuffer, 
    PULONG nSize 
    ); 
 
#ifdef UNICODE 
#define GetUserNameEx   GetUserNameExW 
#else 
#define GetUserNameEx   GetUserNameExA 
#endif 
 
BOOLEAN 
SEC_ENTRY 
TranslateNameA( 
    LPCSTR lpAccountName, 
    EXTENDED_NAME_FORMAT AccountNameFormat, 
    EXTENDED_NAME_FORMAT DesiredNameFormat, 
    LPSTR lpTranslatedName, 
    PULONG nSize 
    ); 
BOOLEAN 
SEC_ENTRY 
TranslateNameW( 
    LPCWSTR lpAccountName, 
    EXTENDED_NAME_FORMAT AccountNameFormat, 
    EXTENDED_NAME_FORMAT DesiredNameFormat, 
    LPWSTR lpTranslatedName, 
    PULONG nSize 
    ); 
#ifdef UNICODE 
#define TranslateName   TranslateNameW 
#else 
#define TranslateName   TranslateNameA 
#endif 
 
#ifdef SECURITY_DOS 
#pragma warning(default:4147) 
#endif 
 
#endif // __SSPI_H__ 

//+------------------------------------------------------------------------- 
// 
//  Microsoft Windows 
//  Copyright 1992 - 1998 Microsoft Corporation. 
// 
//  File:      issperr.h 
// 
//  Contents:  Constant definitions for error codes. 
// 
// 
//-------------------------------------------------------------------------- 
#ifndef _ISSPERR_H_ 
#define _ISSPERR_H_ 
// Define the status type. 
 
#ifdef FACILITY_SECURITY 
#undef FACILITY_SECURITY 
#endif 
 
#ifdef STATUS_SEVERITY_SUCCESS 
#undef STATUS_SEVERITY_SUCCESS 
#endif 
 
#ifdef STATUS_SEVERITY_COERROR 
#undef STATUS_SEVERITY_COERROR 
#endif 
 
// 
// Define standard security success code 
// 
 
#define SEC_E_OK                         ((HRESULT)0x00000000L) 
 
// Define the severities 
// 
//  Values are 32 bit values layed out as follows: 
// 
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 
//  +---+-+-+-----------------------+-------------------------------+ 
//  |Sev|C|R|     Facility          |               Code            | 
//  +---+-+-+-----------------------+-------------------------------+ 
// 
//  where 
// 
//      Sev - is the severity code 
// 
//          00 - Success 
//          01 - Informational 
//          10 - Warning 
//          11 - Error 
// 
//      C - is the Customer code flag 
// 
//      R - is a reserved bit 
// 
//      Facility - is the facility code 
// 
//      Code - is the facility's status code 
// 
// 
// Define the facility codes 
// 
#define FACILITY_SECURITY                0x9 
 
 
// 
// Define the severity codes 
// 
#define STATUS_SEVERITY_SUCCESS          0x0 
#define STATUS_SEVERITY_COERROR          0x2 
 
 
// 
// MessageId: SEC_E_INSUFFICIENT_MEMORY 
// 
// MessageText: 
// 
//  Not enough memory is available to complete this request 
// 
#define SEC_E_INSUFFICIENT_MEMORY        ((HRESULT)0x80090300L) 
 
// 
// MessageId: SEC_E_INVALID_HANDLE 
// 
// MessageText: 
// 
//  The handle specified is invalid 
// 
#define SEC_E_INVALID_HANDLE             ((HRESULT)0x80090301L) 
 
// 
// MessageId: SEC_E_UNSUPPORTED_FUNCTION 
// 
// MessageText: 
// 
//  The function requested is not supported 
// 
#define SEC_E_UNSUPPORTED_FUNCTION       ((HRESULT)0x80090302L) 
 
// 
// MessageId: SEC_E_TARGET_UNKNOWN 
// 
// MessageText: 
// 
//  The specified target is unknown or unreachable 
// 
#define SEC_E_TARGET_UNKNOWN             ((HRESULT)0x80090303L) 
 
// 
// MessageId: SEC_E_INTERNAL_ERROR 
// 
// MessageText: 
// 
//  The Local Security Authority cannot be contacted 
// 
#define SEC_E_INTERNAL_ERROR             ((HRESULT)0x80090304L) 
 
// 
// MessageId: SEC_E_SECPKG_NOT_FOUND 
// 
// MessageText: 
// 
//  The requested security package does not exist 
// 
#define SEC_E_SECPKG_NOT_FOUND           ((HRESULT)0x80090305L) 
 
// 
// MessageId: SEC_E_NOT_OWNER 
// 
// MessageText: 
// 
//  The caller is not the owner of the desired credentials 
// 
#define SEC_E_NOT_OWNER                  ((HRESULT)0x80090306L) 
 
// 
// MessageId: SEC_E_CANNOT_INSTALL 
// 
// MessageText: 
// 
//  The security package failed to initialize, and cannot be installed 
// 
#define SEC_E_CANNOT_INSTALL             ((HRESULT)0x80090307L) 
 
// 
// MessageId: SEC_E_INVALID_TOKEN 
// 
// MessageText: 
// 
//  The token supplied to the function is invalid 
// 
#define SEC_E_INVALID_TOKEN              ((HRESULT)0x80090308L) 
 
// 
// MessageId: SEC_E_CANNOT_PACK 
// 
// MessageText: 
// 
//  The security package is not able to marshall the logon buffer, 
//  so the logon attempt has failed 
// 
#define SEC_E_CANNOT_PACK                ((HRESULT)0x80090309L) 
 
// 
// MessageId: SEC_E_QOP_NOT_SUPPORTED 
// 
// MessageText: 
// 
//  The per-message Quality of Protection is not supported by the 
//  security package 
// 
#define SEC_E_QOP_NOT_SUPPORTED          ((HRESULT)0x8009030AL) 
 
// 
// MessageId: SEC_E_NO_IMPERSONATION 
// 
// MessageText: 
// 
//  The security context does not allow impersonation of the client 
// 
#define SEC_E_NO_IMPERSONATION           ((HRESULT)0x8009030BL) 
 
// 
// MessageId: SEC_E_LOGON_DENIED 
// 
// MessageText: 
// 
//  The logon attempt failed 
// 
#define SEC_E_LOGON_DENIED               ((HRESULT)0x8009030CL) 
 
// 
// MessageId: SEC_E_UNKNOWN_CREDENTIALS 
// 
// MessageText: 
// 
//  The credentials supplied to the package were not 
//  recognized 
// 
#define SEC_E_UNKNOWN_CREDENTIALS        ((HRESULT)0x8009030DL) 
 
// 
// MessageId: SEC_E_NO_CREDENTIALS 
// 
// MessageText: 
// 
//  No credentials are available in the security package 
// 
#define SEC_E_NO_CREDENTIALS             ((HRESULT)0x8009030EL) 
 
// 
// MessageId: SEC_E_MESSAGE_ALTERED 
// 
// MessageText: 
// 
//  The message supplied for verification has been altered 
// 
#define SEC_E_MESSAGE_ALTERED            ((HRESULT)0x8009030FL) 
 
// 
// MessageId: SEC_E_OUT_OF_SEQUENCE 
// 
// MessageText: 
// 
//  The message supplied for verification is out of sequence 
// 
#define SEC_E_OUT_OF_SEQUENCE            ((HRESULT)0x80090310L) 
 
// 
// MessageId: SEC_E_NO_AUTHENTICATING_AUTHORITY 
// 
// MessageText: 
// 
//  No authority could be contacted for authentication. 
// 
#define SEC_E_NO_AUTHENTICATING_AUTHORITY ((HRESULT)0x80090311L) 
 
// 
// MessageId: SEC_I_CONTINUE_NEEDED 
// 
// MessageText: 
// 
//  The function completed successfully, but must be called 
//  again to complete the context 
// 
#define SEC_I_CONTINUE_NEEDED            ((HRESULT)0x00090312L) 
 
// 
// MessageId: SEC_I_COMPLETE_NEEDED 
// 
// MessageText: 
// 
//  The function completed successfully, but CompleteToken 
//  must be called 
// 
#define SEC_I_COMPLETE_NEEDED            ((HRESULT)0x00090313L) 
 
// 
// MessageId: SEC_I_COMPLETE_AND_CONTINUE 
// 
// MessageText: 
// 
//  The function completed successfully, but both CompleteToken 
//  and this function must be called to complete the context 
// 
#define SEC_I_COMPLETE_AND_CONTINUE      ((HRESULT)0x00090314L) 
 
// 
// MessageId: SEC_I_LOCAL_LOGON 
// 
// MessageText: 
// 
//  The logon was completed, but no network authority was 
//  available.  The logon was made using locally known information 
// 
#define SEC_I_LOCAL_LOGON                ((HRESULT)0x00090315L) 
 
// 
// MessageId: SEC_E_BAD_PKGID 
// 
// MessageText: 
// 
//  The requested security package does not exist 
// 
#define SEC_E_BAD_PKGID                  ((HRESULT)0x80090316L) 
 
// 
// MessageId: SEC_E_CONTEXT_EXPIRED 
// 
// MessageText: 
// 
//  The context has expired and can no longer be used. 
// 
#define SEC_E_CONTEXT_EXPIRED            ((HRESULT)0x80090317L) 
 
// 
// MessageId: SEC_E_INCOMPLETE_MESSAGE 
// 
// MessageText: 
// 
//  The supplied message is incomplete.  The signature was not verified. 
// 
#define SEC_E_INCOMPLETE_MESSAGE         ((HRESULT)0x80090318L) 
 
// 
// Provided for backwards compatibility 
// 
 
#define SEC_E_NO_SPM SEC_E_INTERNAL_ERROR 
#define SEC_E_NOT_SUPPORTED SEC_E_UNSUPPORTED_FUNCTION 
 
#endif // _ISSPERR_H_ 
 

 
