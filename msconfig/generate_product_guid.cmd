@rem = 'use perl to generate a GUID from a MD5 string hash
@setlocal
@for %%I in (perl.exe) do @set $_=%%~$PATH:I
@if "%$_%"=="" @for /f "tokens=3 skip=2 usebackq" %%I in (`reg query HKLM\Software\Perl /v BinDir 2^>NUL`) do @set $_=%%~sfI 
@if "%$_%"=="" @for /f "tokens=3 skip=2 usebackq" %%I in (`reg query HKLM\Software\Wow6432Node\Perl /v BinDir 2^>NUL`) do @set $_=%%~sfI 
@if "%$_%"=="" @set $_=perl.exe 
@%$_% -- "%~f0" %*
@goto :EOF ';
#!/user/bin/env perl
use strict;
use warnings;
use File::Basename;
my $dir=dirname($0);
require "$dir/UUIDTiny.pm";
my $guid_v3 = 3; # MD5
my $guid_v5 = 5; # SHA1 (perl support may not be available)

# use MD5 (GUID type 3) for condor version since we can't count on SHA1 support
my $vn = $guid_v3;

# Prefix the string with the guid namespace for condor_versions
my $condor_version_namespace = "{ca10cb1e-24f8-4b31-9569-4088034951a3}";
my $var = $condor_version_namespace . join("",@ARGV);
my $guid = UUID::Tiny::create_UUID_as_string($vn, $var);
print $guid;
# for debugging
# print "\" V" . $vn . "GUIDoF=\"$var";
exit 0;
