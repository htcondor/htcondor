/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/*
Module : SMTP.H
Purpose: Defines the interface for a MFC class encapsulation of the SMTP protocol
Created: PJN / 22-05-1998
History: None

Copyright (c) 1998 by PJ Naughter.  
All rights reserved.

*/


/////////////////////////////// Defines ///////////////////////////////////////
#ifndef __SMTP_H__
#define __SMTP_H__

#ifndef __AFXTEMPL_H__
#pragma message("SMTP classes require afxtempl.h in your PCH")                                                                                
#endif

#ifndef _WINSOCKAPI_
#pragma message("SMTP classes require afxsock.h or winsock.h in your PCH")
#endif

#ifndef __AFXPRIV_H__
#pragma message("SMTP classes requires afxpriv.h in your PCH")
#endif
  

/////////////////////////////// Classes ///////////////////////////////////////

//Simple Socket wrapper class
class CSMTPSocket
{
public:
//Constructors / Destructors
  CSMTPSocket();
  ~CSMTPSocket();

//methods
  BOOL  Create();
  BOOL  Connect(LPCTSTR pszHostAddress, int nPort = 110);
  BOOL  Send(LPCSTR pszBuf, int nBuf);
  void  Close();
  int   Receive(LPSTR pszBuf, int nBuf);
  BOOL  IsReadible(BOOL& bReadible);

protected:
  BOOL   Connect(const SOCKADDR* lpSockAddr, int nSockAddrLen);
  SOCKET m_hSocket;
};

//Encapsulation of an SMTP email address, used for recipients and in the From: field
class CSMTPAddress
{
public: 
//Constructors / Destructors
  CSMTPAddress() {};
	CSMTPAddress(const CString& sAddress);
	CSMTPAddress(const CString& sFriendly, const CString& sAddress);
	CSMTPAddress& operator=(const CSMTPAddress& r);

//Methods
  CString GetRegularFormat() const;

//Data members
	CString m_sFriendlyName; //Would set it to contain something like "PJ Naughter"
  CString m_sEmailAddress; //Would set it to contains something like "pjn@indigo.ie"
};

//Encapsulation of an SMTP File attachment
class CSMTPAttachment
{
public:
//Constructors / Destructors
	CSMTPAttachment();
  ~CSMTPAttachment();

//methods
  BOOL Attach(const CString& sFilename);
  CString GetFilename() const { return m_sFilename; };
  const char* GetEncodedBuffer() const { return m_pszEncoded; };
  int GetEncodedSize() const { return m_nEncodedSize; };

protected:
  int Base64BufferSize(int nInputSize);
  BOOL EncodeBase64(const char* aIn, int aInLen, char* aOut, int aOutSize, int* aOutLen);
  static char m_base64tab[];

  CString  m_sFilename;    //The filename you want to send
  char*    m_pszEncoded;   //The encoded representation of the file
  int      m_nEncodedSize; //size of the encoded string
};


////////////////// Forward declaration
class CSMTPConnection;

//Encapsulation of an SMTP message
class CSMTPMessage
{
public:
//Constructors / Destructors
  CSMTPMessage();
  ~CSMTPMessage();

//methods
	int              AddRecipient(CSMTPAddress& recipient);
	void             RemoveRecipient(int nIndex); 
	CSMTPAddress     GetRecipient(int nIndex) const;
	int              GetNumberOfRecipients() const;
	int              AddAttachment(CSMTPAttachment* pAttachment);
	void             RemoveAttachment(int nIndex);
	CSMTPAttachment* GetAttachment(int nIndex) const;
  int              GetNumberOfAttachments() const;
  virtual CString  GetHeader() const;
  void             AddBody(const CString& sBody);
  BOOL             AddMultipleRecipients(const CString& sRecipients);
	

//Data Members
	CSMTPAddress m_From;
	CString      m_sSubject;
  CString      m_sXMailer;
	CSMTPAddress m_ReplyTo;

protected:
  void FixSingleDot(CString& sBody);

	CString m_sBody;
	CArray<CSMTPAddress, CSMTPAddress&> m_Recipients;
  CArray<CSMTPAttachment*, CSMTPAttachment*&> m_Attachments;

  friend class CSMTPConnection;
};

//The main class which encapsulates the SMTP connection
class CSMTPConnection
{
public:
//Constructors / Destructors
  CSMTPConnection();
  ~CSMTPConnection();

//Methods
  BOOL    Connect(LPCTSTR pszHostName, int nPort=25);
  BOOL    Disconnect();
  CString GetLastCommandResponse() const { return m_sLastCommandResponse; };
  int     GetLastCommandResponseCode() const { return m_nLastCommandResponseCode; };
  DWORD   GetTimeout() const { return m_dwTimeout; };
  void    SetTimeout(DWORD dwTimeout) { m_dwTimeout = dwTimeout; };
	BOOL    SendMessage(CSMTPMessage& Message);

protected:
	BOOL ReadCommandResponse(int nExpectedCode);
	BOOL ReadResponse(LPSTR pszBuffer, int nBuf, LPSTR pszTerminator, int nExpectedCode);

  CSMTPSocket m_SMTP;
  BOOL        m_bConnected;
  CString     m_sLastCommandResponse;
	DWORD       m_dwTimeout;
  int         m_nLastCommandResponseCode;
};


#endif //__SMTP_H__

