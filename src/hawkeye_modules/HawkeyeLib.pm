##################################################
package HawkeyeLib;# $pkg_name = "HawkeyeLib";
use HawkeyePublish;

# Hard config from the command line
my %HardConfigs;
my $ModuleName = $0;
if ( $ModuleName =~ /\/([^\/]+)$/ )
{
    $ModuleName = $1;
}

# Config stuff
sub DoConfig( );
sub DoConfig( )
{
    $ModuleName = shift( @ARGV) if ( $#ARGV >= 0 );

    chop( $ENV{OS_TYPE} = `uname -s` );
    chop( $ENV{OS_REV} = `uname -r` );
}

# Get my name..
sub GetModuleName( )
{
    return $ModuleName;
}

# Hard coded / command line config
sub HardConfig( $;$ )
{
    my $Name = shift;
    my $Label = "$ModuleName$Name";

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
    my $Label = "$ModuleName$Ext";
    my $String;
    if ( exists( $HardConfigs{$Label} )  ) {
	$String = $HardConfigs{$Label};
    } else {
	$String = `hawkeye_config_val -startd hawkeye_$ModuleName$Ext`;
	$String = "not defined" if ( -1 == $? );
    }

    # Parse it
    if ( $String =~ /not defined/i )
    {
	return lc( $Default );
    }
    else
    {
	chomp $String;
	$String =~ s/\"//g;
	return lc( $String );
    }
}

sub ParseUptime( $$ )
{
    local $_ = shift;
    my $HashRef = shift;

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
sub new {
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
    my $Name = shift;
    my $Type = shift;
    my $Value = shift;

    my $NewElem = ();
    $NewElem->{Type} = $Type;
    $NewElem->{Value} = $Value;
    $self->{Hash}{$Name} = $NewElem;
}

sub Store
{
    my $self = shift;
    my $Label = $self->{Label};
    foreach my $Key ( keys %{$self->{Hash}} )
    {
	my $Name = $Label . $Key;
	my $Value = $self->{Hash}{$Key}->{Value};
	my $Type = $self->{Hash}{$Key}->{Type};
	if ( $self->{Hash}{$Key}->{Type} =~ /n/i )
        {
	    ${$self->{Hawkeye}}->StoreNum( $Name, $Value );
        }
	else
        {
	    ${$self->{Hawkeye}}->Store( $Name, $Value );
        }
    }
}

1;
