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

##################################################
package HawkeyeLib;# $pkg_name = "HawkeyeLib";
use HawkeyePublish;

# Hawkeye interface version number
my $MyInterfaceVersion = 1;
my $HawkeyeInterfaceVersion;
my $InterfaceVersionEnv = "HAWKEYE_INTERFACE_VERSION";
my $StartdNameEnv = "STARTD_CRON_NAME";
my $StartdName = "Hawkeye";
my $ConfigVal = "hawkeye_config_val";

# Hard config from the command line
my %HardConfigs;
my $ModuleName = $0;
my $ConfigQuery = 1;
if ( $ModuleName =~ /\/([^\/]+)$/ )
{
    $ModuleName = $1;
}

# Config stuff
# sub DoConfig( );
sub DoConfig( )
{
    # Prevent recursion
    if ( exists $ENV{HAWKEYE_MODULE} )
    {
	print STDERR "$$: Recursion detected in Hawkeye module '" .
	    $ENV{HAWKEYE_MODULE} . "'\n";
	sleep( 5 );
	die;
    }
    $ENV{HAWKEYE_MODULE} = "$0";

    # Get the startd name
    if ( exists $ENV{$StartdNameEnv} )
    {
	$StartdName = $ENV{$StartdNameEnv};
	my $NameUc = uc( $StartdName );
	$InterfaceVersionEnv = $NameUc . "_INTERFACE_VERSION";
	$ConfigVal = "condor_config_val" if ( $NameUc ne "HAWKEYE" );
    }

    # Look for a config val environment
    {
	my $Tmp = uc( $StartdName . "_CONFIG_VAL" );
	if ( exists $ENV{$Tmp} )
	{
	    $ConfigVal = $ENV{$Tmp};
	}
    }

    # Get the module name from the command line
    $ModuleName = shift( @ARGV) if ( $#ARGV >= 0 );

    # Handle "--ifversion";
    if ( $ModuleName =~ /^--ifversion/ )
    {
	print "$InterfaceVersionEnv=$MyInterfaceVersion\n";
	exit 0;
    }

    # Parameters like --module_name=value are used for configs on cmdline
    # Process these here...
    my @NewArgv;
    foreach my $Arg ( @ARGV )
    {
	# Command line parameter value
	if ( $Arg =~ /^(?:--)?($ModuleName\_\w+)=(.*)/i )
	{
	    my $Var = uc $1;
	    $HardConfigs{$Var} = $2;
	}
	# Special case; just an no "name" (i.e. "--df=/home,/scratch" )
	elsif ( $Arg =~ /^--($ModuleName)=(.*)/i )
	{
	    my $Var = uc $1;
	    $HardConfigs{$Var} = $2;
	}
	# Easier for user: --(variable)=value
	elsif ( $Arg =~ /^(?:--)?(\w+)=(.*)/ )
	{
	    my $Var = uc( $ModuleName . "_" . $1 );
	    $HardConfigs{$Var} = $2;
	}
	# Don't queury the config from the startd _at all_
	elsif ( ( $Arg eq "--noquery" ) or
		( $Arg eq "--no-query" ) or
		( $Arg eq "-nq" ) )
	{
	    $ConfigQuery = 0;
	}
	# Query published interface version
	elsif ( $Arg eq "--ifversion" )
	{
	    print "$InterfaceVersionEnv=$MyInterfaceVersion\n";
	    exit 0;
	}
	# Normal param, just keep going..
	elsif ( $Arg ne "" )
	{
	    push @NewArgv, $Arg;
	}
    }

    # Install the new argv...
    @ARGV = @NewArgv;

    # Setup some default stuff...
    chop( $ENV{OS_TYPE} = `uname -s` );
    chop( $ENV{OS_REV} = `uname -r` );
    chop( $ENV{HOST} = `hostname` );

    # Interface version stuff...
    $HawkeyeInterfaceVersion = 0;
    if ( exists $ENV{$InterfaceVersionEnv} )
    {
	$HawkeyeInterfaceVersion = $ENV{$InterfaceVersionEnv};
    }
}

# Get my name..
sub GetModuleName( )
{
    return $ModuleName;
}

# Get my interface version
sub GetMyInterfaceVersion( )
{
    return $MyInterfaceVersion;
}

# Get the HAWKEYE interface version
sub GetHawkeyeInterfaceVersion( )
{
    return $HawkeyeInterfaceVersion;
}

# Hard coded / command line config
sub HardConfig( $;$ )
{
    my $Name = shift;
    my $Label = uc "$ModuleName$Name";

    if ( $#_ >= 0 ) {
	$HardConfigs{$Label} = shift;
    } else {
	$HardConfigs{$Label} = "not defined";
    }
}

sub ReadConfig( $$ )
{
    my $Ext = shift;
    my $Default = shift;

    # Get the config
    my $Label = uc "$ModuleName$Ext";
    my $String = "not defined";

    if ( exists( $HardConfigs{$Label} )  )
    {
	$String = $HardConfigs{$Label};
    }
    elsif ( exists $ENV{$Label} )
    {
	$String = $ENV{$Label};
    }
    elsif ( $ConfigQuery )
    {
	my $Cmd = $ConfigVal . " -startd " . $StartdName . "_$ModuleName$Ext";
	$String = `$Cmd`;
	$String = "not defined" if ( -1 == $? );
    }

    # Parse it
    if ( $String =~ /not defined/i )
    {
	return $Default;
    }
    else
    {
	chomp $String;
	$String =~ s/\"//g;
	return $String;
    }
}

sub ParseUptime( $$ )
{
    local $_ = shift;
    my $HashRef = shift;

    # Cleanup the line
    chomp $_;
    s/^\s+//;

    # Get rid of all the damn commas
    s/,//g;

    # If we've been up for less than 1 day, make the lines consistent!
    if ( ! /day/ ) {
	s/up/up 0 days/;
    }

    # Now, split it up and go
    my @Fields = split;

    # Pull out the time
    if ( $Fields[0] =~ /(\d+):(\d+)([ap])m/ ) {
	my $Time = ( $1 * 60 ) + $2;
	$Time += (12 * 60 ) if ( $3 =~ /p/ );
	$HashRef->{Time} = $Time;
	$HashRef->{TimeString} = $Fields[0];
    }

    # Parse the actual uptime field
    if ( $Fields[5] eq "min" ) {
    	splice( @Fields, 5, 1 );
	my $Hours = 0;
	my $Minutes = $Fields[4];
	$HashRef->{Uptime} =
	    ( $Fields[2] * 24 * 60 ) + ( $Hours * 60 ) + $Minutes;
    } else {
	my ( $Hours, $Minutes ) = split( /:/, $Fields[4] );
	$HashRef->{Uptime} =
	    ( $Fields[2] * 24 * 60 ) + ( $Hours * 60 ) + $Minutes;
    }

    # Rest of the line
    $HashRef->{Users}  = $Fields[5];
    $HashRef->{Load01} = $Fields[9];
    $HashRef->{Load05} = $Fields[10];
    $HashRef->{Load15} = $Fields[11];
}

# Parse a seconds expression (i.e. 1s or 5m)
sub ParseSeconds( $$ )
{
    my $TimeString = shift;
    my $Seconds = shift;

    if ( $TimeString eq "" )
    {
	# Do nothing
    }
    # We handle s=seconds, m=minutes, h=hours, d=days
    elsif ( $TimeString =~ /([\d\.]+)([sSmMhHdD]?)/ )
    {
	$Seconds = $1;
	if ( ( $2 eq "" ) || ( $2 eq "s" ) || ( $2 eq "S" ) )
	{
	    # Do nothing
	}
	elsif ( ( $2 eq "m" ) || ( $2 eq "M" ) )
	{
	    $Seconds *= 60;
	}
	elsif ( ( $2 eq "h" ) || ( $2 eq "H" ) )
	{
	    $Seconds *= (60 * 60);
	}
	elsif ( ( $2 eq "d" ) || ( $2 eq "D" ) )
	{
	    $Seconds *= (60 * 60 * 24);
	}
	else
	{
	    print STDERR "Uknown time modifier '$2'\n";
	}
    }
    else
    {
	print STDERR "Bad time string '$TimeString'\n";
    }

    # Done; return it
    $Seconds;
}

# Parse a byte count expression (i.e. 1b or 5m or 10g)
sub ParseBytes( $$ )
{
    my $ByteString = shift;
    my $Bytes = shift;

    if ( $ByteString eq "" )
    {
	# Do nothing
    }
    # We handle b=bytes, k=kilo, m=mega, g=giga
    elsif ( $ByteString =~ /([\d\.]+)([bBmkKMgG]?)/ )
    {
	if ( ( $2 eq "" ) || ( $2 eq "b" ) || ( $2 eq "B" ) )
	{
	    $Bytes = $1;
	}
	elsif ( ( $2 eq "k" ) || ( $2 eq "K" ) )
	{
	    $Bytes = $1 * 1024;
	}
	elsif ( ( $2 eq "m" ) || ( $2 eq "M" ) )
	{
	    $Bytes = $1 * 1024 * 1024;
	}
	elsif ( ( $2 eq "g" ) || ( $2 eq "G" ) )
	{
	    $Bytes = $1 * 1024 * 1024 * 1024;
	}
	else
	{
	    $Bytes = $1;
	    print STDERR "Uknown byte modifier '$2'\n";
	}
    }
    else
    {
	print STDERR "Bad byte string '$ByteString'\n";
    }

    # Done; return it
    $Bytes;
}

# Detect the running O/S
sub DetectOs( $ )
{
    my $OsRef = shift;
    my $Os = undef;
    foreach my $OsNum ( 0 .. $#{$OsRef} )
    {
	my $OsType = @{$OsRef}[$OsNum]->{ostype};
	my $OsRev = @{$OsRef}[$OsNum]->{osrev};
	if (  ( $ENV{OS_TYPE} =~ /$OsType/ ) &&
	      ( $ENV{OS_REV}  =~ /$OsRev/ )  )
	{
	    $Os = @{$OsRef}[$OsNum];
	    last;
	}
    }
    return $Os;
}

sub AddHash( $$$$ )
{
    my $HashRef = shift;
    my $Name = shift;
    my $Type = shift;
    my $Value = shift;

    my $NewElem = ();
    $NewElem->{type} = $Type;
    $NewElem->{value} = $Value;
    ${$HashRef}{$Name} = $NewElem;
}

sub StoreHash( $$ )
{
    my $Label = shift;
    my $HashRef = shift;

    foreach my $Key ( keys %$HashRef )
    {
	if ( ${$HashRef}{$Key}->{type} =~ /n/i )
        {
	    $main::Hawkeye->StoreNum( $Label . $Key, 
				      ${$HashRef}{$Key}->{value} );
        }
	else
        {
	    $main::Hawkeye->Store( $Label . $Key, ${$HashRef}{$Key}->{value} );
        }
    }
}

1;

# Simple type to create manage & publish hashes of related things
package HawkeyeHash;
@EXPORT = qw( new Add Store );

# Constructor
sub new
{
    my $class = shift;
    my $self = {};
    $self->{Hash} = {};
    $self->{Hawkeye} = shift;
    $self->{Label} = shift;
    bless $self->{Hawkeye}, HawkeyePublish;
    bless $self, $class;
    # $self->_initialize();
    return $self;
}

sub Add
{
    my $self = shift;

    Carp::confess( "HawkeyeHash::Add: Bad args" ) if ( scalar @_ < 2 );
    my $Name = shift;
    my $Type;
    my $Value;

    if ( scalar @_ == 1 )
    {
	$Value = shift;
	$Type = HawkeyePublish::TypeAuto;
    }
    else
    {
	$Type = shift;
	$Value = shift;
    }

    my $NewElem = ();

    # If the type is a number, it's modern invocation.
    if ( $Type =~ /^\d+$/ )
    {
	$NewElem->{Type} = $Type;
    }
    # Otherwise, we understand "n" == "number"
    elsif ( $Type =~ /^n/i )
    {
	$NewElem->{Type} = HawkeyePublish::TypeNumber;
    }
    # Anything else is a string
    else
    {
	$NewElem->{Type} = HawkeyePublish::TypeString;
    }
    $NewElem->{Value} = $Value;
    $self->{Hash}{$Name} = $NewElem;
}

sub Store
{
    my $self = shift;
    my $Label = $self->{Label};
    my $Count = 0;
    foreach my $Key ( keys %{$self->{Hash}} )
    {
	my $Name = $Label . $Key;
	my $Value = $self->{Hash}{$Key}->{Value};
	my $TypeNew = $self->{Hash}{$Key}->{TypeNew};
	my $Type = $self->{Hash}{$Key}->{Type};

	${$self->{Hawkeye}}->StoreValue( $Name, $Value, $Type );
	$Count++;
    }
    return $Count;
}

1;
