#parse command line
if ( $#ARGV ) {
	print "usage: create_ca <full path of CA install directory>\n";
	exit( 1 );
}
@LINE = split( //, $ARGV[0] );
if ( $LINE[0] ne '/' ) {
	print "Error: CA install directory must be fully qualified (from root)\n";
	exit( 1 );
}
$INSTALLDIR = $ARGV[0];
shift;

#set some variables, only change these defaults WITH CAUTION!
$DAYS = "-days 365";
$SSLEAY_CONFIG = "$INSTALLDIR/condor_ssl.cnf";
$RAND = "$INSTALLDIR/rand.dat";

#intial setup, including create directory
startCA();

#generate the condor-ssl configuration file that admin, daemon, & users use.
generateCondorssl_cnf( $SSLEAY_CONFIG );

#do the rest of the steps necessary for creating the CA
finishCA();

#finally, generate the two admin scripts, condor_cert for users
generateCondor_cert( "$INSTALLDIR/condor_cert" );

#and condor_ca for the CA administrator
generateCondor_ca( "$INSTALLDIR/condor_ca" );

exit( 0 );


######## the rest of this file is subroutines called above ##########

sub generateCondor_ca {
	local( $CAFILE ) = @_;
	open( CAFILE, ">$CAFILE" );
	Printheader( CAFILE, $INSTALLDIR, $DAYS );
	print CAFILE '$DAEMON = 1;', "\n";
	PrintParseCommandLine( CAFILE );
	PrintDaemonOrUser( CAFILE );
	PrintRest( CAFILE );
	close( CAFILE );
	chmod 0700, $CAFILE;
}

sub generateCondor_cert {
	local( $CERTFILE ) = @_;
	open( CERTFILE, ">$CERTFILE" );
	Printheader( CERTFILE, $INSTALLDIR, $DAYS );
	print CERTFILE '$PARAM = "-des3 1024";', "\n";
	print CERTFILE '$USERDIR = $ARGV[0];', "\n";

	#add a command line check
	print CERTFILE 'if ( "$USERDIR" eq "" ) {', "\n";
	print CERTFILE '	print "usage: condor_cert <cert dir to create>\n";', "\n";
	print CERTFILE '	exit( 1 );', "\n";
	print CERTFILE '}',"\n";

	PrintDaemonOrUser( CERTFILE );
	close( CERTFILE );
	chmod 0755, $CERTFILE;
}

sub startCA {
	print "Creating certificate authority (CA) directory \"$INSTALLDIR\"";
	print "\ncontinue? (y/n) ";
	while ( $_ = <> ) {
		exit if ( /^n/i );
		last if ( /^y/i );
		print "\ncontinue? (y/n) ";
	}

	if ( ! mkdir $INSTALLDIR, 0744 ) {
		print "failure creating directory \"$INSTALLDIR\", $!\n";
		exit $!;
	}
}

sub finishCA {
	print "generating random state to $RAND\n";
	system( "ps -a>$RAND" );

	# create the directory hierarchy
	mkdir "$INSTALLDIR/certs", 0700;
	mkdir "$INSTALLDIR/crl", 0700;
	mkdir "$INSTALLDIR/usercert", 0700;
	mkdir "$INSTALLDIR/private", 0700;
	system( "echo \"01\" > $INSTALLDIR/serial" );
	system( "touch $INSTALLDIR/index.txt" );
	### NOTE! this could be a privacy violation! world readable user db!!!!
	### currently, schedd needs to read this. It should be owned by user
	### or at least group condor (or whatever daemons run as!!
	chmod 0644, "$INSTALLDIR/index.txt";
	print "\nMaking CA private key...\n";

	$RESULT = system("ssleay genrsa -rand $RAND -des3 1024 > "
			. "$INSTALLDIR/private/cakey.pem" );
		
	if ( !$RESULT ) {
		print "\nMaking CA certificate using CA private key...\n";
		if ( !( $RESULT = system("ssleay req -config $SSLEAY_CONFIG -new -x509 "
			. " -key $INSTALLDIR/private/cakey.pem "
			. " -out $INSTALLDIR/cacert.pem $DAYS")))
		{
			chmod 0600, "$INSTALLDIR/private/cakey.pem";
			chmod 0644, "$INSTALLDIR/cacert.pem";
			print "certificate authority (CA) directory is in \"$INSTALLDIR\"\n";
		}
	}
	if ( $RESULT ) {
		print "ERROR in CA setup, maybe 'ssleay' is not in your PATH\n";
		print "Interactively removing $INSTALLDIR and its children\n";
		system( "rm -ir $INSTALLDIR" );
	}
}

sub Printheader {
	local( $FILE, $INSTALLDIR, $DAYS ) = @_;
	print $FILE "#!", `which perl`, "\n";
	print $FILE "#definitions specified at CA installation\n";
	print $FILE '$CONDORCA=', "\"$INSTALLDIR\";", "\n";
	print $FILE '$DAYS=', "\"$DAYS\";", "\n";
	print $FILE '$SSLEAY_DIR="$CONDORCA";', "\n";
	print $FILE '$SSLEAY_CONFIG="$SSLEAY_DIR/condor_ssl.cnf";', "\n";
	print $FILE '$RAND="$CONDORCA/rand.dat";', "\n";
}


sub PrintParseCommandLine {
	local( $FILE ) = @_;

	print $FILE <<'PPCL';
	$USERDIR = $ARGV[$#ARGV];
	$_ = $ARGV[0];
	$INFILE = $ARGV[0];
	if ( /^-daemon/i ) {  #}
		#code for daemon
PPCL
}

sub PrintRest {
	local( $FILE ) = @_;

	print $FILE <<'PPCL2';
		#end of code for daemon
		exit;  #{
	}
	#else do certificate signing stuff
	#$_ should be input filename
	if (system("ssleay ca -config $SSLEAY_CONFIG -in $INFILE -out $USERDIR")){
		print "error signing certificate, no output written to \"$USERDIR\"\n";
		eval $CLEANUP;
		exit 1;
	}
	system( "cat $USERDIR" );
	print "Signed certificate is in $USERDIR\n";
PPCL2
}

sub PrintDaemonOrUser {
	local( $FILE ) = @_;

	print $FILE <<'PDOU';

	#create a certificate request
	print "creating certificate directory \"$USERDIR\"\n";
	if ( ! mkdir $USERDIR, 0700 ) {
		print "failure creating directory \"$USERDIR\", $!\n";
		exit $!;
	}
	if ( ! mkdir "$USERDIR/private", 0700 ) {
		print "failure creating directory \"$USERDIR/private\", $!\n";
		print "removing newly created directory \"$USERDIR\"\n";
		rmdir $USERDIR;
		exit $!;
	}
	if ( ! mkdir "$USERDIR/certdir/", 0700 ) {
		print "failure creating directory \"$USERDIR/certdir\n";
		rmdir $USERDIR/private, $USERDIR;
		exit 1;
	}

	$USERKEY = "$USERDIR/private/userkey.pem";

	print "\nCreating a private key...\n";
	if ( $RETVAL1 = system( "ssleay genrsa -rand $RAND $PARAM > $USERKEY" ) ) {
		print "error generating private key\n";
	}
	else {
		print "\nCreating a certificate request using the private key...\n";
		if ( $RETVAL2 = system( "ssleay req -config $SSLEAY_CONFIG -new -key "
				. "$USERKEY -out $USERDIR/usercert_request.pem $DAYS") ) 
		{
			print "error generating certificate request\n";
		}
		elsif ( $RETVAL2 = system("cp $CONDORCA/cacert.pem $USERDIR/certdir/" ) ) 
		{
			print "error copying Certificate Authority's certificate "
					. "to \"$USERDIR\"\n";
		}
		elsif ( $RETVAL2 = system( "cp $SSLEAY_CONFIG $USERDIR/" ) ) {
			print "error copying Certificate Authority's configuration file "
					. "to \"$USERDIR\"\n";
		}
		else {
			if ( defined( $DAEMON ) ) {
				if ( $RETVAL2 = 
					system( "ln -s $SSLEAY_DIR/index.txt $USERDIR/index.txt" ) ) 
				{
					print "error symlinking to Certificate Authority's user "
						. " database (index.txt) ";
				}
			}
		}
	}
	if ( $RETVAL1 || $RETVAL2 ) {
		print "interactively removing newly created directory tree \"$USERDIR\"\n";
		system( "rm -ir $USERDIR" );
		exit 1;
	}
	chmod 0600, "$USERDIR/private/userkey.pem";
	$SYMLINK = `c_hash $USERDIR/certdir/cacert.pem|sed 's/ .*//'`;
	chomp( $SYMLINK );
	if ( ! symlink( "certdir/cacert.pem", "$USERDIR/certdir/$SYMLINK" ) ) {
		print "error creating symlink to CA certificate, $!\n";
		print "removing newly created directory tree\"$USERDIR\"\n";
		system( "rm -ir $USERDIR" );
		exit $!;
	}
	print "Private key is in \"$USERDIR/private/userkey.pem\"\n";
	print "Certificate request is in \"$USERDIR/usercert_request.pem\",\n";
	print "\n";
	print "Remember to have your request signed by your condor administrator\n";
	print "and store the signed certificate as \"$USERDIR/usercert.pem\"\n";
	#end of section
PDOU
}

sub generateCondorssl_cnf {
	local( $CNFFILE ) = @_;
	open( CNFFILE, ">$CNFFILE" );

	print CNFFILE <<"GCSC";

##condor_ssl.cnf - modified version of standard ssleay.cnf

dir = $INSTALLDIR			# Where everything is kept
GCSC
	print CNFFILE <<'GCSC2';

####################################################################
[ ca ]
default_ca	= CA_default		# The default ca section

####################################################################
[ CA_default ]

certs		= $dir/certs		# Where the issued certs are kept
crl_dir		= $dir/crl		# Where the issued crl are kept
database	= $dir/index.txt	# database index file.
new_certs_dir	= $dir/usercert		# default place for new certs.

certificate	= $dir/cacert.pem 	# The CA certificate
serial		= $dir/serial 		# The current serial number
crl		= $dir/crl.pem 		# The current CRL
private_key	= $dir/private/cakey.pem# The private key
#change this RANDFILE to be used by daemon & USER
##RANDFILE	= $dir/private/.rand	# private random number file

#x509_extensions	= x509v3_extensions	# The extentions to add to the cert
default_days	= 365			# how long to certify for
default_crl_days= 30			# how long before next CRL
default_md	= md5			# which md to use.
preserve	= no			# keep passed DN ordering

#policy section
policy		= policy_anything


# For the anything policy
# At this point in time, you must list all acceptable object types.
[ policy_anything ]
countryName		= optional
stateOrProvinceName	= optional
localityName		= optional
organizationName	= optional
organizationalUnitName	= optional
commonName		= supplied
emailAddress		= optional

####################################################################
[ req ]
default_bits		= 1024
default_keyfile 	= privkey.pem
distinguished_name	= req_distinguished_name
attributes		= req_attributes

[ req_distinguished_name ]

commonName	= Condor Username (<username>@<domain> e.g. me\@x.y.edu)
commonName_max			= 64

[ req_attributes ]

[ x509v3_extensions ]

# under ASN.1, the 0 bit would be encoded as 80
nsCertType			= 0x40
GCSC2

###end of file output#####
	close( CNFFILE );
	chmod 0644, $CNFFILE;
}
