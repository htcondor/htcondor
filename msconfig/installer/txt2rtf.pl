#!/usr/bin/perl
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

#  -*-coding:latin-1;-*-  «sburke»                                          µ
# desc{  convert ascii text file(s) to RTF  } sburke~cpan.org
my $VERSION = ('Time-stamp: "2005-09-02 17:01:40 ADT"' =~ m/\"([0-9-]+)/g)[0];

use constant DEBUG => 0;
use strict;
use warnings;
use utf8;

my $opt_help;
use Getopt::Long;
GetOptions(
  'help'       => \   $opt_help,
  'version'    => \my $opt_version,
  'noheader'   => \my $noheader,
  'noheaderfilename' => \my $noheaderfilename,
  'points=s'   => \my $points,
  'font=s'     => \my $font           )
|| do {
  DEBUG and print "Options-parsing error.\n";
  $opt_help = 1;
};

if($opt_version) { print "text2rtf version $VERSION\n"; exit; }

if($opt_help or !@ARGV) {
  print
"text2rtf : make an rtf file out of some text files
                    [ version $VERSION  sburke\x40cpan.org ]
Options:
 --help          Show this message, then quit.
 --version       Report version, then quit.
 --font

Examples:
 text2rtf thing.txt > thing.rtf
 ls | text2rtf - > thing.rtf
 text2rtf --noheader *.xml > ../xml_source.rtf
 text2rtf --font=Palatino `cat ../all.txt` > ../all.rtf

";
  exit;
}

main();

#---------------------------------------------------------------------------
my $is_first_file;
my $halfpoints;

sub main {
  $halfpoints = (int(2*($points||0))) || 17;

  prolog();

  $is_first_file = 1;
  foreach my $file (@ARGV) {
    my($fh, $inname);
    next unless length $file;
    if($file eq '-') {
      $fh = *STDIN{IO};
      $inname = '';
    }
    else {
      next if -d $file;
      open $fh, '<', $file or die "Can't read-open $file: $!\n Aborting";
      $inname = $file;
    }
    do_file($fh, $inname);
  }
  postlog();
  exit;
}

#---------------------------------------------------------------------------

sub prolog {
  printf q[{\rtf1\ansi\deff0
{\fonttbl
{\f0\froman Times New Roman;}
{\f1\fswiss Arial;}
{\f2\fmodern Courier New;}
{\f3\fmodern %s;}
}
{\info {\author .}{\company .}{\title .}}

]
   => esc($font || "Courier New")
  ;
  return;
}

#---------------------------------------------------------------------------

sub postlog { print "\n}\n"; }


#---------------------------------------------------------------------------

my $is_new_file;

sub new_file {
  my $fn = $_[0];

  print qq[\n\\sectd\\uc1\n];

  $noheader or printf q[
\pgnrestart
{\header \pard\qr\plain\f0\fs16{\i
%s
}\chpgn\par}

]
   => ( length($fn) && !$noheaderfilename ) ? (esc($fn).'  ') : ''
  ;

  printf q[
{\pard\nowidctlpar\plain\f3\fs%s
],  $halfpoints;

  $is_new_file = 0;
  return;
}

sub end_file { print "\n\\par}\\sect\n\n"; return; }

#---------------------------------------------------------------------------

sub do_file {
  my($fh, $inname) = @_;
  $is_new_file = 1;

  new_file($inname);

  my $i = 0;
  while(<$fh>) {
    s/[\n\r]+$//s;

    # Tab expansion:
    while(
	  # Sort of adapted from Text::Tabs -- yes, it's hardwired in that
	  # tabs are at every eighth column.
	  s/^([^\t]*)(\t+)/$1.(" " x ((length($2)<<3)-(length($1)&7)))/e
    ) {}

    print $i++ ? "\\line " : '', esc($_), "\n";

  }
  end_file($inname);

  $is_first_file = 0;
  return;
}

#---------------------------------------------------------------------------

my @Escape;

BEGIN {
  @Escape = map sprintf("\\'%02x", $_), 0x00 .. 0xFF;
  foreach my $i ( 0x20 .. 0x7E ) {  $Escape[$i] = chr($i) }

  my @refinements = (
   "\\" => "\\'5c",
   "{"  => "\\'7b",
   "}"  => "\\'7d",
   
   "\cm"  => '',
   "\cj"  => '',
   "\n"   => "\n\\line ",
    # This bit of voodoo means that whichever of \cm | \cj isn't synonymous
    #  with \n, is aliased to empty-string, and whichever of them IS "\n",
    #  turns into the "\n\\line ".
   
   "\t"   => "\\tab ",     # Tabs (altho theoretically raw \t's might be okay)
   "\f"   => "\n\\page\n", # Formfeed
   "-"    => "\\_",        # Turn plaintext '-' into a non-breaking hyphen
                           #   I /think/ that's for the best.
   "\xA0" => "\\~",        # \xA0 is Latin-1/Unicode non-breaking space
   "\xAD" => "\\-",        # \xAD is Latin-1/Unicode soft (optional) hyphen
   '.' => "\\'2e",
   'F' => "\\'46",
  );
  my($char, $esc);
  while(@refinements) {
    ($char, $esc) = splice @refinements,0,2;
    $Escape[ord $char] = $esc;
  }
}

sub esc {
  my $x = ((@_ == 1) ? $_[0] : join '', @_);

  $x =~ s/([\x00-\x1F\-\\\{\}\x7F-\xFF])/$Escape[ord$1]/g;
  $x =~ s/([^\x00-\xFF])/'\\u'.((ord($1)<32768)?ord($1):(ord($1)-65536)).'?'/eg;
  $x =~ s/^([F\.])/$Escape[ord$1]/sg;

  return $x;
}

#---------------------------------------------------------------------------

__END__

