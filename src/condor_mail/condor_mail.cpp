// condor_mail.cpp : main project file.

using namespace System;
using namespace System::Net;
using namespace System::Net::Mail;
using namespace System::Collections;
using namespace System::Collections::Generic;

void Usage(System::Int32 code) {
	System::Console::Error->WriteLine("usage: condor_mail [-f from] "
		"[-s subject] [-relay relayhost] recipient ...");
	Environment::Exit(code);
}

System::String ^Username() {

	System::String ^username = nullptr;	

	username = System::Environment::GetEnvironmentVariable("CONDOR_MAIL_USER");
	if (username != nullptr && username->Length > 0) {
		return username;
	}

	username = System::Environment::UserName;
	if (username != nullptr && username->Length > 0) {
		return username;
	}

	username = System::Environment::GetEnvironmentVariable("USER");
	if (username != nullptr && username->Length > 0) {
		return username;
	}

	return "unknown";

}

System::String^ Body() {
	return System::Console::In->ReadToEnd();
}

int main(array<System::String ^> ^args) {
	
	List<MailAddress^>	^recipients = gcnew List<MailAddress^>();
	System::String		^subject	= "";
	System::String		^from		= Username() + "@" +
										Dns::GetHostName();			
	System::String		^relay		= "127.0.0.1";

	try {

		for (int i = 0; i < args->Length; i++) {
			if (args[i]->CompareTo("-s") == 0) {
				subject = args[++i];
			} else if (args[i]->CompareTo("-f") == 0) {
				from = args[++i];
			} else if (args[i]->CompareTo("-relay") == 0) {
				relay = args[++i];
			} else {
				// this is a recipient
				recipients->Add(gcnew MailAddress(args[i]));
			}				
		}

	} catch (Exception ^e) {

		System::Console::Error->WriteLine("error: " + e->Message);
		Usage(1);

	}

	if (0 == recipients->Count) {
		
		System::Console::Error->WriteLine("error: you must specify "
			"at least one recipient.");
		Usage(2);

	}

	try {
		
		SmtpClient	^client = gcnew SmtpClient(relay);
		MailMessage	^msg	= gcnew MailMessage();
		msg->From			= gcnew MailAddress(from);
		msg->Subject		= subject;
		msg->Body			= Body();
		for (int i = 0; i < recipients->Count; i++) {
			msg->To->Add(recipients[i]);
		}
		client->Send(msg);

	} catch (Exception ^e) {

		System::Console::Error->WriteLine("error: " + e->Message);
		Usage(3);

	}

}
