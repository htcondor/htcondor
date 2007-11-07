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

# CondorPublish.pm - a Perl module for exporting data to a startd classad
#
# 2002-Feb-04 Nick LeRoy
use strict;
use Carp;
require 5.0;
require Exporter;

package HawkeyePublish;

# Constructor
sub new
{
    my $class = shift;
    my $self = {};
    bless $self, $class;
    $self->_initialize();
    return $self;
}

sub TypeAuto   { 1 }
sub TypeString { 2 }
sub TypeNumber { 3 }
sub TypeBool   { 4 }
sub TypePercent { 5 }

# Perform initializations..
sub _initialize
{
    my $self = shift;

    # Hashes
    $self->{Publish} = {};		# Hash of things to publish
    $self->{Published} = {};		# Hash of things currently published

    # These control debugging & diagnostics
    $self->{RunIt} = 1;
    $self->{Debug} = 0;
    $self->{Quiet} = 0;

    # My index list
    $self->{Index} = {};		# Index
    $self->{AutoIndex} = 1;		# Auto index y/n {prefix}
}

# Quiet attribute warnings
sub Quiet
{
    my $self = shift;
    $self->{Quiet} = shift;
}

# Publish a value
sub StoreValue
{
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 2 );
    my $Var = shift;
    my $Value = shift;
    my $Type = shift;
    $self->AttrCheck( "Store: Attribute", \$Var );

    my $Attr = $Var;
    $self->StoreIndex( $Var ) if ( $self->{AutoIndex} );

    # Auto; let's guess
    if ( $Type == HawkeyePublish::TypeAuto )
    {
	my $Orig = $Value;
	if ( $Value =~ /^((\d+(\.\d+)?)|(\.\d+))$/ )
	{
	    $Type = HawkeyePublish::TypeNumber;
	}
	elsif ( $Value =~ /^((?:\d+(?:\.\d+)?)|(?:\.\d+))\%$/ )
	{
	    $Value = $1 * 0.01;
	    $Type = HawkeyePublish::TypePercent;
	}
	elsif ( $Value =~ /(^true$)|(^false$)/i )
	{
	    $Type = HawkeyePublish::TypeBool;
	}
	else
	{
	    $Type = HawkeyePublish::TypeString;
	}
    }
    elsif ( $Type == HawkeyePublish::TypeNumber )
    {
	if ( ! ( $Value =~ /(^\d+(\.\d+)?$)|(^\.\d+$)/ ) )
	{
	    print STDERR "Attempting to store non-number: ".
		"'$Attr = $Value'\n";
	    Carp::carp( "Store: Bad number '$Value' to '$Var'" );
	}
    }
    elsif ( $Type == HawkeyePublish::TypePercent )
    {
	if ( $Value =~ /^((?:\d+(?:\.\d+)?)|(?:\.\d+))(\%?)$/ )
	{
	    $Value = $1;
	    $Value *= 0.01 if ( defined $2 and $2 eq "%");
	}
	else
	{
	    print STDERR "Attempting to store non-number percent: ".
		"'$Attr = $Value'\n";
	    Carp::carp( "Store: Bad percent number '$Value' to '$Var'" );
	}
    }

    # Store 'em off
    my $Rec = ();
    $Rec->{Type} = $Type;
    $Rec->{Value} = $Value;
    $self->{Publish}{$Attr} = $Rec;
}

# These are depricated methods for storing.  Use StoreValue() instead.

# Publish a string value
sub Store
{
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 1 );
    my $Var = shift;
    my $Value = shift;
    $self->AttrCheck( "Store: Attribute", \$Var );

    $self->StoreValue( $Var, $Value, HawkeyePublish::TypeString );
}

# Publish a string value
sub StoreString
{
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 1 );
    my $Var = shift;
    my $Value = shift;

    $self->StoreValue( $Var, $Value, HawkeyePublish::TypeString );
}

# Publish a numeric value
sub StoreNum
{
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 1 );
    my $Var = shift;
    my $Value = shift;

    $self->StoreValue( $Var, $Value, HawkeyePublish::TypeNumber );
}

# Publish a boolean boolean value
sub StoreBool
{
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 1 );
    my $Var = shift;
    my $Value = shift;

    $self->StoreValue( $Var, $Value, HawkeyePublish::TypeBool );
}

# Publish a boolean boolean value
sub StorePercent
{
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 1 );
    my $Var = shift;
    my $Value = shift;

    $self->StoreValue( $Var, $Value, HawkeyePublish::TypePercent );
}

# Turn on/off auto indexing
sub AutoIndexSet
{
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "AutoIndexSet: wrong args" ) if ( $#_ != 0 );
    $self->{AutoIndex} = shift;
}

# Add to the index
sub StoreIndex
{
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "StoreIndex: wrong args" ) if ( $#_ != 0 );
    my $Index = shift;
    $self->AttrCheck( "Store: Index", \$Index );

    # Put it in the index hash
    $self->{Index}{$Index} = 1;
}

# Unstore one or more variables
sub UnStore {
    my $self = shift;
    Carp::confess( "Store: wrong args" ) if ( $#_ < 0 );

    # Walk through the @_ array...
    my $Name;
    foreach $Name ( @_ )
    {
	my $Var = $self->{CurrentPrefix} . $Name;
	$self->AttrCheck( "Unstore: Attribute", \$Var );
	delete $self->{Publish}{$Var}
	    if ( exists ( $self->{Publish}{$Var} ) );
	delete $self->{Index}{$Var}
	    if ( exists ( $self->{Index}{$Var} ) );
    }
}

# Do the real work..
sub Publish
{
    my $self = shift;
    my $Key;

    # Add the prefix list to the publish list...
    if ( scalar %{$self->{Index}} )
    {
	print "INDEX = \"" . join( " ", keys%{$self->{Index}} ) . "\"\n";
    }

    # Walk through the publish hash, build a command line from hell..
    foreach $Key ( sort keys %{$self->{Publish}} )
    {
	my $Attr = $Key;
	my $Rec = $self->{Publish}{$Key};
	my $Value;

	if ( ! defined $Rec->{Value} )
	{
	    print STDERR "Attempt to publish attribute w/o value: '$Attr'\n";
	    next;
	}

	# Publish it
	if ( $Rec->{Type} == HawkeyePublish::TypeString )
	{
	    $Value = $Rec->{Value};
	    print "$Attr = \"$Value\"\n";
	}
	elsif ( $Rec->{Type} == HawkeyePublish::TypeBool )
	{
	    $Value = $Rec->{Value};
	    print "$Attr = $Value\n";
	}
	elsif ( $Rec->{Type} == HawkeyePublish::TypeNumber )
	{
	    if ( ! ( $Rec->{Value} =~ /(?:(?:\d+(?:\.\d+)?)|(?:\.\d+))/ ) )
	    {
		print STDERR "Attempting to publish non-number: ".
		    "'$Attr = $Rec->{Value}'\n";
		next;
	    }
	    $Value = $Rec->{Value} * 1.0;
	    print "$Attr = $Value\n";
	}
	elsif ( $Rec->{Type} == HawkeyePublish::TypePercent )
	{
	    if ( ! ( $Rec->{Value} =~ /(?:(?:\d+(?:\.\d+)?)|(?:\.\d+))/ ) )
	    {
		print STDERR "Attempting to publish non-number as percent: ".
		    "'$Attr = $Rec->{Value}'\n";
		next;
	    }
	    $Value = $Rec->{Value} * 100.0;
	    print "$Attr = \"$Value%\"\n";
	}
	else
	{
	    print STDERR "Unknown type " .
		$Rec->{Type} . " of " . $Rec->{Attr} . "\n";
	    next;
	}
    }
    print "--\n";
}

# Check an attribute
sub AttrCheck()
{
    my $self = shift;
    my $String = shift;
    my $Attr = shift;
    if ( $$Attr =~ /\W/ )
    {
	my $NewAttr = $$Attr;
	$NewAttr =~ s/\W/_/g;
	Carp::carp( "Warning: $String '$$Attr' replaced with '$NewAttr'\n" )
	      if (! $self->{Quiet} );
	$$Attr = $NewAttr;
    }
}

# Return true
1;
