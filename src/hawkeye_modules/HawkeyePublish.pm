# CondorPublish.pm - a Perl module for exporting data to a startd classad
#
# 2002-Feb-04 Nick LeRoy
use strict;
use Carp;
require 5.0;
require Exporter;

package HawkeyePublish;

# Constructor
sub new {
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

# Perform initializations..
sub _initialize {
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
sub Quiet {
    my $self = shift;
    $self->{Quiet} = shift;
}

# Publish a value
sub StoreValue {
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
    if ( $Type == $self->TypeAuto )
    {
	if ( ( $Value =~ /^\d+$/ ) || ( $Value =~ /^\d*\.\d+$/ ) )
	{
	    $Type = HawkeyePublish::TypeNumber;
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

    # Store 'em off
    my $Rec = ();
    $Rec->{Type} = $Type;
    $Rec->{Value} = $Value;
    $self->{Publish}{$Attr} = $Rec;
}

# Publish a value
sub Store {
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 1 );
    my $Var = shift;
    my $Value = shift;
    $self->AttrCheck( "Store: Attribute", \$Var );

    $self->StoreValue( $Var, $Value, HawkeyePublish::TypeString );
}

# Publish a value
sub StoreString {
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 1 );
    my $Var = shift;
    my $Value = shift;

    $self->StoreValue( $Var, $Value, HawkeyePublish::TypeString );
}

# Publish a value
sub StoreNum {
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 1 );
    my $Var = shift;
    my $Value = shift;

    $self->StoreValue( $Var, $Value, HawkeyePublish::TypeNumber );
}

# Publish a boolean value
sub StoreBool {
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "Store: wrong args" ) if ( $#_ != 1 );
    my $Var = shift;
    my $Value = shift;

    $self->StoreValue( $Var, $Value, HawkeyePublish::TypeBool );
}

# Turn on/off auto indexing
sub AutoIndexSet {
    my $self = shift;

    # Check that we're passed the correct # of args...
    Carp::confess( "AutoIndexSet: wrong args" ) if ( $#_ != 0 );
    $self->{AutoIndex} = shift;
}

# Add to the index
sub StoreIndex {
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
sub Publish {
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
	    $Value = $Rec->{Value} * 1.0;
	    print "$Attr = $Value\n";
	}
	else
	{
	    print STDERR "Unknown type " . $Rec->{Type} . " of " . $Rec->{Attr} . "\n";
	}
    }
    print "--\n";
}

# Check an attribute
sub AttrCheck() {
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
