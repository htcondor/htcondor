# condor_mail

## Usage

`condor_mail.exe` retains backwards compatibility with existing parameters, and three new command line parameters have been introduced: `-savecred`, `-u`, and `-p` to support sending email through authenticated relays.

### condor_mail parameters

Parameter|Purpose|Required
---|---|---
`-f`|From address|No, discovered if not given, overriden by `-u`
`-s`|Message subject|No
`-relay`|SMTP relay to use|No, only used on Windows
`-savecred`|Save credentials for authenticated relay|No, but requires `-relay` and both `-u` and `-p`
`-u`|Username for the account on the relay|No, but requires `-savecred` and `-p`
`-p`|Password for the account on the relay|No, but requires `-savecred` and `-u`
`recipient recipient ...`|Recipients, separated by whitespace|Yes, at least one

## Compiling

The code is easy to compile with the standalone build tools so no vcproj file has been added. A vcproj can be added if there is a desire to start building this again, or it can continue to be prebuilt and copied during build.

`condor_mail.exe` sat unchanged for 10 years and was last compiled with Visual C++ 2008 (Build tools v90). The latest version of `condor_mail.exe` has been compiled and tested with Visual C++/CLI 2012 (Build tools v110), and Visual C++/CLI 2019 (Build tools v140).


Compile with icon:

```cmd
rc.exe app.rc
cl.exe /clr condor_mail.cpp /link app.res
```

Compile without icon:

```cmd
cl.exe /clr condor_mail.cpp
```
