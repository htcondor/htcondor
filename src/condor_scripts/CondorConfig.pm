#***********************************************************************
#
# Copyright (C) 2004, Computer Sciences Department,
# University of Wisconsin-Madison, WI, and Nick LeRoy,
# and other members of the Condor Project
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License version
# 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
#
# For contact information, please visit:
#      http://www.cs.wisc.edu/condor
#
#**********************************************************************/
use strict;
use warnings;


# ******************************************************
# Simple class to config macros
# ******************************************************
package CondorConfigMacros;


# ******************************************************
# Constructor
# ******************************************************
sub new( $ )
{
    my $class = shift;
    my $self = {};

    $self->{Macros} = ();

    bless $self, $class;

    # Pre-define some things:
    if ( ! exists $ENV{HOSTNAME} )
    {
	$ENV{HOSTNAME} = `/bin/hostname`;
	chomp $ENV{HOSTNAME};
    }

    # Setup some basic things
    my $Host = $ENV{HOSTNAME};
    $self->Set( "FULL_HOSTNAME", $Host );
    $Host =~ s/\..*$//;
    $self->Set( "HOSTNAME", $Host );
    $self->Set( "TILDE", $ENV{HOME} );

    return $self;

} # new()
# ******************************************************

# ******************************************************
# Dump all macros for Debuging
# ******************************************************
sub Dump( $ )
{
    my $self = shift;

    print scalar( keys %{$self->{Macros}} ) . " macros defined:\n";
    foreach my $Var ( sort keys %{$self->{Macros}} )
    {
	my $Macro = $self->{Macros}{$Var};
	my $File = $Macro->{File} ? $Macro->{File} : "<NONE>";
	print "\t$File:$Macro->{Line}:$Var = '$Macro->{Value}'\n";
    }

} # Dump()
# ******************************************************

# ******************************************************
# Set a macro's value
# ******************************************************
sub Set( $$$ )
{
    my $self = shift;
    my $Var = shift;
    my $Value = shift;

    # Get the macro name
    my $Macro = uc( $Var );

    # Expand self references
    if ( exists $self->{Macros}{$Macro} )
    {
	my $CurValue = $self->{Macros}{$Macro}{Value};
	$Value =~ s/\$\($Macro\)/$CurValue/g;
    }

    # And, set it
    $self->{Macros}{$Macro}{Value} = $Value;
    $self->{Macros}{$Macro}{File} = undef;
    $self->{Macros}{$Macro}{Line} = -1;

} # Set()
# ******************************************************

# ******************************************************
# Set a macro's value
# ******************************************************
sub SetWithFile( $$$$$ )
{
    my $self = shift;
    my $File = shift;
    my $Line = shift;
    my $Var = shift;
    my $Value = shift;

    # Get the macro name
    my $Macro = uc( $Var );

    # Expand self references
    if ( exists $self->{Macros}{$Macro} )
    {
	my $CurValue = $self->{Macros}{$Macro}{Value};
	$Value =~ s/\$\($Macro\)/$CurValue/g;
    }

    # And, set it
    $self->{Macros}{$Macro}{Value} = $Value;
    $self->{Macros}{$Macro}{File} = $File;
    $self->{Macros}{$Macro}{Line} = $Line;

} # SetWithFile()
# ******************************************************

# ******************************************************
# Get a macro without expanding it
# ******************************************************
sub Get( $ )
{
    my $self = shift;
    my $Var = uc( shift );

    # One more check
    return undef if ( ! exists ( $self->{Macros}{$Var} ) );
    return $self->{Macros}{$Var}{Value};

} # Get()
# ******************************************************

# ******************************************************
# Get info on a macro
# ******************************************************
sub GetInfo( $$ )
{
    my $self = shift;
    my $Var = uc( shift );

    # One more check
    return undef if ( ! exists ( $self->{Macros}{$Var} ) );
    return $self->{Macros}{$Var};

} # Get()
# ******************************************************

# ******************************************************
# Expand a macro from the config file
# ******************************************************
sub Expand( $ )
{
    my $self = shift;
    my $Var = uc( shift );

    # One more check
    return undef if ( ! exists ( $self->{Macros}{$Var} ) );

    # Now, expand it out...
    my $Value = $self->{Macros}{$Var}{Value};
    while( $Value =~ /(.*)\$\((\w+)\)(.*)/ )
    {
	if ( $2 eq $Var )
	{
	    $Value = $1 . $3;
	}
	elsif ( exists ( $self->{Macros}{$2} ) )
	{
	    $Value = $1 . $self->{Macros}{$2}{Value} . $3;
	    last if ( $2 eq $Var );
	}
	elsif ( exists ( $ENV{$2} ) )
	{
	    $Value = $1 . $ENV{$2} . $3;
	    last if ( $2 eq $Var );
	}
	else
	{
	    print STDERR "Warning: Unable to expand $Value\n";
	    last;
	}
    }
    $Value;

} # Expand()
# ******************************************************

# ******************************************************
# Expand a string
# ******************************************************
sub ExpandString( $ )
{
    my $self = shift;
    my $Value = shift;

    # Now, expand it out...
    while( $Value =~ /(.*)\$\((\w+)\)(.*)/ )
    {
	if ( exists ( $self->{Macros}{$2} ) )
	{
	    $Value = $1 . $self->{Macros}{$2}{Value} . $3;
	}
	elsif ( exists ( $ENV{$2} ) )
	{
	    $Value = $1 . $ENV{$2} . $3;
	}
	else
	{
	    print STDERR "Warning: Unable to expand $Value\n";
	    last;
	}
    }
    $Value;

} # ExpandString()
# ******************************************************

# ******************************************************
# Simple class to handle config files
# ******************************************************
package CondorConfigFiles;

# ******************************************************
# Constructor
# ******************************************************
sub new( $$ )
{
    my $class = shift;
    my $Macros = shift;
    my $self = {};

    $self->{Base} = "";
    $self->{Files} = [];
    $self->{FilesRead} = 0;
    $self->{Macros} = $Macros;
    $self->{NewText} = [];

    bless $self, $class;

    return $self;
}
# ******************************************************

# ******************************************************
# Find the 'base' config file to use
# ******************************************************
sub FindConfig( $$$$ )
{
    my $self = shift;
    my $ConfigEnvVar = shift;
    my $UserSpecified = shift;
    my $Distribution = shift;

    # Setup
    $UserSpecified = "" if ( ! $UserSpecified );
    my $BaseFile = "";

    # Check the config env variable...
    if ( ( $UserSpecified eq "" ) && ( exists $ENV{$ConfigEnvVar} ) )
    {
	warn "$ENV{$ConfigEnvVar} (from ENV) does not exist!"
	    if ( ! -f $ENV{$ConfigEnvVar} );
	$BaseFile = $ENV{$ConfigEnvVar};
    }

    # Fallbacks...
    my @FallbackConfigs = [ "/home/$Distribution", ];
    my $RootEnv = uc $Distribution . "_ROOT_DIR";
    unshift @FallbackConfigs, $ENV{$RootEnv}
	if ( exists $ENV{$RootEnv} );
    foreach my $TempConfig ( @FallbackConfigs )
    {
	if (  ( $BaseFile eq "" ) && ( -f $TempConfig )  )
	{
	    $BaseFile = $TempConfig;
	}
    }

    # Store it
    if ( $BaseFile ne "" )
    {
	$self->SetBase( $BaseFile );

	# Make sure the CONDOR_CONFIG is defined...
	if ( ! exists $ENV{$ConfigEnvVar} )
	{
	    $ENV{$ConfigEnvVar} = $BaseFile;
	}
	return $BaseFile;
    }

    return undef;

} # FindConfig()
# ******************************************************

# ******************************************************
# Set the base config file
# ******************************************************
sub SetBase( $$ )
{
    my $self = shift;
    my $Name = shift;

    # Create a record for it, push into the list
    my $r = ();
    $r->{Name} = $Name;
    $r->{Required} = 1;
    $r->{IsBase} = 1;
    push( @{$self->{Files}}, $r );
    $r = ();

    return 1;
}
# ******************************************************

# ******************************************************
# Set the config file to update
# ******************************************************
sub SetUpdateFile( $$ )
{
    my $self = shift;
    my $Name = shift;

    return 1 if ( exists $self->{Update} );

    # Create a record for this file
    my $r = ();
    $r->{Name} = $Name;
    $r->{Required} = 0;
    $r->{IsBase} = 1;

    # And, insert it into the list
    push( @{$self->{Files}}, $r );

    # Store off the name
    $self->{Update} = $r;

    # If we've read all of the other files, read this one, too
    # (Note: required is zero)
    if ( $self->{FilesRead} )
    {
	$self->ReadFile( $r );
    }

    $r = ();
    return 1;

} # SetUpdateFile
# ******************************************************

# ******************************************************
# Set the config file to update
# ******************************************************
sub GetUpdateFile( $ )
{
    my $self = shift;

    return undef if ( ! exists $self->{Update} );
    return $self->{Update}->{Name};

} # GetUpdateFile
# ******************************************************

# ******************************************************
# Set the config file to update
# ******************************************************
sub AddText( $$ )
{
    my $self = shift;
    my $New = shift;

    push( @{$self->{NewText}}, $New );

} # AddText
# ******************************************************

# ******************************************************
# Return the number of new text lines
# ******************************************************
sub NewTextLines( $ )
{
    my $self = shift;

    return scalar @{$self->{NewText}};

} # NewTextLines
# ******************************************************

# ******************************************************
# Read all of the config files
# ******************************************************
sub ReadAll( $ )
{
    my $self = shift;

    # Walk through the list
    foreach my $File ( @{$self->{Files}} )
    {
	# Read it...
	if ( ! $self->ReadFile( $File ) )
	{
	    if ( $File->{Required} )
	    {
		die "Can't read config @ " . $File->{File};
	    }
	    else
	    {
		print "WARNING: Can't read config file '$File->{File}'\n";
	    }
	}

	# Evaluate the local configs
	if ( $File->{IsBase} )
	{
	    my $Local = $self->{Macros}->Get( "LOCAL_CONFIG_FILE" );

	    if ( $Local )
	    {
		my $Expanded = $self->{Macros}->ExpandString( $Local );
		$Expanded =~ s/^\s+//;
		$Expanded =~ s/\s+$//;

		foreach my $File ( split( /[\s\,]+/, $Expanded ) )
		{
		    next if ( $File =~ /^\s+$/ );
		    my $r = ();
		    $r->{Name} = $File;
		    $r->{BaseName} = $r;
		    $r->{Required} = 1;
		    $r->{IsBase} = 0;
		    push( @{$self->{Files}}, $r );
		    $r = ();
		}
	    }
	}
	$self->{FilesRead}++;
    }
    print "\n";
}
# ******************************************************

# ******************************************************
# Read a single config file
# ******************************************************
sub ReadFile( $$ )
{
    my $self = shift;
    my $FileRef = shift;

    # Have we read in this file?
    if ( exists $FileRef->{Inode} )
    {
	return 1;
    }

    # Generate it's real name
    my $Name = $FileRef->{Name};
    my $File;
    if ( ! exists $FileRef->{File} )
    {
	$File = $self->{Macros}->ExpandString( $Name );
	$File = $Name if ( ! $File );
    }
    $FileRef->{File} = $File;

    # Open it for reading
    open( CONFIG, $File ) || return 0;

    # Find out what inode it is..
    my @StatBuf = stat( CONFIG );
    if ( $#StatBuf < 0 )
    {
	print STDERR "Error stating $File!\n";
	close CONFIG;
	return 0
    }
    $FileRef->{Dev} = $StatBuf[0];
    $FileRef->{Inode} = $StatBuf[1];

    # Read it in
    print "Reading config file $Name ...";
    $FileRef->{Text} = [];
    my $Line = "";
    while( <CONFIG> )
    {
	chomp;
	push( @{$FileRef->{Text}}, $_ );
	s/^\s+//;
	s/\s+$//;

	# Comment? ( /\#/ )
	if ( /\#/ )
	{
	    $self->ProcessConfigLine( $File, $., $Line ) if ( $Line ne "" );
	    $Line = "";
	}

	# Continuation?
	elsif ( /(.*)\\$/ )
	{
	    $Line = ( $Line eq "" ) ? $1 : $Line . " " . $1;
	}

	# "Normal" line
	else
	{
	    $Line = ( $Line eq "" ) ? $_ : $Line . " " . $_;
	    $self->ProcessConfigLine( $File, $., $Line ) if ( $Line ne "" );
	    $Line = "";
	}
    }
    close( CONFIG );
    print "Done\n";

    return 1;

} # ReadFile
# ******************************************************

# ******************************************************
# Process a line from a config file
# ******************************************************
sub ProcessConfigLine( $$$$ )
{
    my $self = shift;
    my $File = shift;
    my $Num = shift;
    my $Line = shift;

    # Some cleanup..
    $Line =~ s/^\s+//;
    $Line =~ s/\s+$//;
    return 0 if ( $Line eq "" ) ;

    # MACRO = value
    if ( $Line =~ /^(\S+)\s*=\s*(.+)/ )
    {
	$self->{Macros}->SetWithFile( $File, $Num, $1, $2 );
    }
    elsif ( $Line =~ /^(\S+)\s*=$/ )
    {
	$self->{Macros}->SetWithFile( $File, $Num, $1, "" );
    }

} # ProcessConfigLine()
# ******************************************************

# ******************************************************
# Write updates to the "update" file
# ******************************************************
sub WriteUpdates( $ )
{
    my $self = shift;

    if ( ! $self->{Update} )
    {
	print STDERR "Can't figure out which file to update!\n";
	return undef;
    }

    my $FileRef = $self->{Update};
    my $Name = $FileRef->{Name};
    my $File = $FileRef->{File};
    my $TmpFile = "$File.tmp.$$";
    unlink( $TmpFile );
    if ( ! open( TMP, ">$TmpFile" ) )
    {
	print STDERR "Can't create temp config '$TmpFile'\n";
	return undef;
    }

    # Write it out..
    my $EmacsLocal = 0;
    foreach ( @{$FileRef->{Text}} )
    {
	if ( /\#\#\# Local Variables:/ )
	{
	    $EmacsLocal = 1;
	    $self->AddText( "" );
	    $self->AddText( $_ );
	    next;
	}
	if ( $EmacsLocal )
	{
	    if ( /\#\#\#/ )
	    {
		$self->AddText( $_ );
		$EmacsLocal = 0 if ( /End:/ );
		next;
	    }
	    else
	    {
		$EmacsLocal = 0;
	    }
	}
	print TMP "$_\n";
    }

    # Now, append the new text
    foreach ( @{$self->{NewText}} )
    {
	print TMP "$_\n";
    }
    close( TMP );

    # Return the name of the temp file
    return ( $File, $TmpFile );

} # WriteUpdates
# ******************************************************

# ******************************************************
# Grep through the configs like 'grep -l'
# ******************************************************
sub GrepText_l( $$ )
{
    my $self = shift;
    my $Pattern = shift;

    # Search through them all....
    my @List;
    foreach my $File ( @{$self->{Files}} )
    {
	my @Tmp = grep( /$Pattern/, @{$File->{Text}} );
	if ( $#Tmp > 0 )
	{
	    push( @List, $File );
	}
    }

    # Return the list of files that match
    return @List;

} # GrepText_l
# ******************************************************

# ******************************************************
# Grep through the configs like 'grep'
# ******************************************************
sub Grep( $$ )
{
    my $self = shift;
    my $Pattern = shift;

    # Search through them all....
    my @List;
    foreach my $File ( @{$self->{Files}} )
    {
	for my $i ( 0 .. $#{@{$File->{Text}}} )
	{
	    my $TextLine = $File->{Text}[$i];
	    if ( $TextLine =~ /$Pattern/ )
	    {
		my $r = ();
		$r->{File} = $File;
		$r->{Line} = $i;
		$r->{Text} = $TextLine;
		push( @List, $r );
		$r = ();
	    }
	}
    }

    # Return the list of files that match
    return @List;

} # Grep
# ******************************************************

# ******************************************************
# Grep through the config names
# ******************************************************
sub GrepNames( $$ )
{
    my $self = shift;
    my $Pattern = shift;

    # Search through them all....
    my @List;
    foreach my $File ( @{$self->{Files}} )
    {
	push( @List, $File ) if ( $File->{File} =~ /$Pattern/ );
    }

    # Return the list of files that match
    return @List;

} # GrepNames
# ******************************************************

# ******************************************************
# Text compare through list of config names
# ******************************************************
sub FindName( $$ )
{
    my $self = shift;
    my $Name = shift;

    # Search through them all....
    my @List;
    foreach my $File ( @{$self->{Files}} )
    {
	return 1 if ( $File->{File} eq $Name );
    }

    # Return the list of files that match
    return 0;

} # FindName
# ******************************************************
1;
