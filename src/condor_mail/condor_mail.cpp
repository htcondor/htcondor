// condor_mail.cpp : main project file.
// compile with: /clr

#using <mscorlib.dll>
#using <System.dll>
#using <System.Net.dll>
#using <System.Security.dll>

using namespace System;
using namespace System::Net;
using namespace System::Net::Mail;
using namespace System::Security;
using namespace System::Security::Cryptography;
using namespace System::Text;
using namespace System::Collections;
using namespace System::Collections::Generic;
using namespace Microsoft::Win32;

void Usage(Int32 code) {
    Console::Error->WriteLine("usage: condor_mail [-f from] "
                    "[-s subject] [-relay relayhost] "
                    "[-savecred -u user@relayhost -p password] recipient ...");
    Environment::Exit(code);
}

String^ Username() {
    String^ username = nullptr;
    username = Environment::GetEnvironmentVariable("CONDOR_MAIL_USER");
    if (!String::IsNullOrEmpty(username)) {
        return username;
    }
    username = Environment::UserName;
    if (!String::IsNullOrEmpty(username)) {
        return username;
    }
    username = Environment::GetEnvironmentVariable("USER");
    if (!String::IsNullOrEmpty(username)) {
        return username;
    }
    return "unknown";
}

String^ Body() {
    return Console::In->ReadToEnd();
}

String^ SmtpRelaysSubKey() {
    return "SOFTWARE\\condor\\SmtpRelays";
}

void SaveCredentials(String^ relay, String^ login, String^ password) {
    try {
        RegistryKey^ key = Registry::LocalMachine->OpenSubKey(
            SmtpRelaysSubKey(), true);
        if (!key) {
            key = Registry::LocalMachine->CreateSubKey(SmtpRelaysSubKey());
        }
        array<Byte>^ securePassword = ProtectedData::Protect(
            Encoding::UTF8->GetBytes(password), nullptr,
            DataProtectionScope::LocalMachine);
        String^ value = String::Format("{0} {1}", login,
            Convert::ToBase64String(securePassword));
        key->SetValue(relay, value);
        key->Close();
    }
    catch (Exception^ e) {
        Console::Error->WriteLine("error : " + e->Message);
        Usage(1);
    }
}

NetworkCredential^ FindCredentials(String^ relay) {
    try {
        RegistryKey^ key = Registry::LocalMachine->OpenSubKey(
            SmtpRelaysSubKey());
        if (!key) {
            return nullptr;
        }
        String^ value = (String^) key->GetValue(relay);
        key->Close();
        if (!value) {
            return nullptr;
        }
        array<String^>^ split = value->Split();
        String^ login = split[0];
        String^ password = Encoding::UTF8->GetString(
            ProtectedData::Unprotect(Convert::FromBase64String(split[1]),
                                nullptr, DataProtectionScope::LocalMachine));
        return gcnew NetworkCredential(login, password);
    }
    catch (Exception^ e) {
        Console::Error->WriteLine("error : " + e->Message);
        Usage(1);
    }
    return nullptr;
}

int main(array<String^>^ args) {
    List<MailAddress^>^ recipients = gcnew List<MailAddress^>();
    String^             subject    = "";
    String^             from       = Username() + "@" + Dns::GetHostName();
    String^             relay      = "127.0.0.1";
    String^             login      = "";
    String^             password   = "";
    Boolean             saveCred   = false;
    try {
        for (int i = 0; i < args->Length; ++i) {
            if ("-s" == args[i]) {
                subject = args[++i];
            }
            else if ("-f" == args[i]) {
                from = args[++i];
            }
            else if ("-relay" == args[i]) {
                relay = args[++i];
            }
            else if ("-savecred" == args[i]) {
                saveCred = true;
            }
            else if ("-u" == args[i]) {
                login = args[++i];
            }
            else if ("-p" == args[i]) {
                password = args[++i];
            }
            else {
                // this is a recipient
                recipients->Add(gcnew MailAddress(args[i]));
            }
        }
    }
    catch (Exception^ e) {
        Console::Error->WriteLine("error: " + e->Message);
        Usage(1);
    }
    if (!saveCred && (!String::IsNullOrEmpty(login) ||
                      !String::IsNullOrEmpty(password))) {
        Console::Error->WriteLine("error: -u or -p cannot be used "
                                  "without -savecred");
        Usage(4);
    }
    if (saveCred) {
        if (String::IsNullOrEmpty(relay)) {
            Console::Error->WriteLine("error: -savecred cannot be used "
                                      "without -relay");
            Usage(4);
        }
        else if ("127.0.0.1" == relay) {
            Console::WriteLine("warning: saving credential for 127.0.0.1");
        }
        if (String::IsNullOrEmpty(login) || String::IsNullOrEmpty(password)) {
            Console::Error->WriteLine("error: -u and -p are required "
                                      "with -savecred");
            Usage(4);
        }
        SaveCredentials(relay, login, password);
        Console::WriteLine("info: saved credential for {0} at relay {1}",
                           login, relay);
        Environment::Exit(0);
    }
    if (!recipients->Count) {
        Console::Error->WriteLine("error: you must specify "
                                  "at least one recipient.");
        Usage(2);
    }
    try {
        SmtpClient^        client          = gcnew SmtpClient(relay);
        MailMessage^       msg             = gcnew MailMessage();
        NetworkCredential^ credentials     = FindCredentials(relay);
        if (credentials) {
            client->EnableSsl   = true;
            client->Port        = 587;
            client->Credentials = credentials;
        }
        msg->From = gcnew MailAddress(from);
        msg->Subject = subject;
        msg->Body = Body();
        for (int i = 0; i < recipients->Count; ++i) {
            msg->To->Add(recipients[i]);
        }
        client->Send(msg);
    }
    catch (Exception^ e) {
        Console::Error->WriteLine("error: " + e->Message);
        Usage(3);
    }
}
