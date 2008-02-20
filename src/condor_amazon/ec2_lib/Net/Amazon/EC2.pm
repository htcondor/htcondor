package Net::Amazon::EC2;

use strict;
use vars qw($VERSION);

use XML::Simple;
use LWP::UserAgent;
use Digest::HMAC_SHA1;
use URI;
use MIME::Base64 qw(encode_base64 decode_base64);
use HTTP::Date qw(time2isoz);
use Params::Validate qw(validate SCALAR ARRAYREF);

$VERSION = '0.04';

=head1 NAME

Net::Amazon::EC2 - Perl interface to the Amazon Elastic Compute Cloud (EC2)
environment.

=head1 VERSION

This document describes version 0.04 of Net::Amazon::EC2, released
April 1, 2007. This module is coded against the Query version of the '2007-01-19' version of the EC2 API.

=head1 SYNOPSIS

 use Net::Amazon::EC2;

 my $ec2 = Net::Amazon::EC2->new(
	AWSAccessKeyId => 'PUBLIC_KEY_HERE', 
	SecretAccessKey => 'SECRET_KEY_HERE'
 );

 # Start 1 new instance from AMI: ami-XXXXXXXX
 my $instance = $ec2->run_instances(ImageId => 'ami-XXXXXXXX', MinCount => 1, MaxCount => 1);

 my $running_instances = $ec2->describe_instances();

 foreach my $inst (@{$running_instances}) {
 	print "$inst->{instance}[0]{instanceId}\n";
 }

 # Terminate instance

 my $result = $ec2->terminate_instances(InstanceId => $instance->{instance}[0]{instanceId});

If an error occurs in communicating with EC2, the return value will be undef and $ec2->{error} will be populated with the message.

=head1 DESCRIPTION

This module is a Perl interface to Amazon's Elastic Compute Cloud. It uses the Query API to communicate with Amazon's Web Services framework.

=head1 CLASS METHODS

=head2 new(%params)

This is the constructor, it will return you a Net::Amazon::EC2 object to work with.  It takes these parameters:

=over

=item AWSAccessKeyId (required)

Your AWS access key.

=item SecretAccessKey (required)

Your secret key, WARNING! don't give this out or someone will be able to use your account and incur charges on your behalf.

=item debug (optional)

A flag to turn on debugging. It is turned off by default

=back

=cut

sub new {
	my $class = shift;
	my %args = validate(@_, {
								AWSAccessKeyId	=> { type => SCALAR },
								SecretAccessKey => { type => SCALAR },
								debug 			=> { type => SCALAR, default => 0},
							}
	);
	
	my $self = {};
	
	bless $self, $class;

	$self->_init(%args);
	
	return $self;
}

sub _init {
	my $self = shift;
	my %args = @_;
	my $ts = time2isoz();

	$self->{signature_version} = 1;
	#$self->{version} = '2007-01-19';
	$self->{version} = '2007-08-29';
	$self->{base_url} = 'http://ec2.amazonaws.com';
	$self->{AWSAccessKeyId} = $args{AWSAccessKeyId};
	$self->{SecretAccessKey} = $args{SecretAccessKey};
	$self->{debug} = $args{debug};
	
	chop($ts);
	$ts .= '.000Z';
	$ts =~ s/\s+/T/g;
	$self->{timestamp} = $ts;	
}

sub _sign {
	my $self = shift;
	my %args = @_;
	my $action = delete $args{Action};
	
	my %sign_hash = %args;
	$sign_hash{AWSAccessKeyId} = $self->{AWSAccessKeyId};
	$sign_hash{Action} = $action;
	$sign_hash{Timestamp} = $self->{timestamp};
	$sign_hash{Version} = $self->{version};
	$sign_hash{SignatureVersion} = $self->{signature_version};
	my $sign_this;	

	# The sign string must be alphabetical in a case-insensitive manner.
	foreach my $key (sort { lc($a) cmp lc($b) } keys %sign_hash) {
		$sign_this .= $key . $sign_hash{$key};
	}

	$self->_debug("QUERY TO SIGN: $sign_this");
	my $encoded = $self->_hashit($self->{SecretAccessKey}, $sign_this);

	my $uri = URI->new($self->{base_url});
	my %params = (
		Action => $action,
		SignatureVersion => $self->{signature_version},
		AWSAccessKeyId => $self->{AWSAccessKeyId},
		Timestamp => $self->{timestamp},
		Version => $self->{version},
		Signature => $encoded,
		%args
	);
	
	$uri->query_form(\%params);
	
	my $ur = $uri->as_string();
	$self->_debug("GENERATED QUERY URL: $ur");
	my $ua = LWP::UserAgent->new();
	my $res = $ua->get($ur);
	
	# We should force <item> elements to be in an array
	my $xs = XML::Simple->new(ForceArray => qr/item/);
	
	my $ref = $xs->XMLin($res->content());

	return $ref;
}

sub _debug {
	my $self = shift;
	my $message = shift;
	
	if ((grep { defined && length} $self->{debug}) && $self->{debug} == 1) {
		print "$message\n";
	}
}

# HMAC sign the query with the aws secret access key and base64 encodes the result.
sub _hashit {
	my $self = shift;
	my ($secret_access_key, $query_string) = @_;
	
	my $hashed = Digest::HMAC_SHA1->new($secret_access_key);
	$hashed->add($query_string);
	
	my $encoded = encode_base64($hashed->digest, '');

	return $encoded;
}

=head1 OBJECT METHODS

=head2 register_image(%params)

This method registers an AMI on the EC2. It takes the following parameter:

=over

=item ImageLocation (required)

The location of the AMI manifest on S3

=back

Returns the image id of the new image on EC2.

=cut

sub register_image {
	my $self = shift;
	my %args = validate( @_, {
								ImageLocation => { type => SCALAR },
	});
		
	my $xml = $self->_sign(Action  => 'RegisterImage', %args);

	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml->{imageId};
	}
}

=head2 describe_images(%params)

This method pulls a list of the AMIs which can be run.  The list can be modified by passing in some of the following parameters:

=over 

=item ImageId (optional)

Either a scalar or an array ref can be passed in, will cause just these AMIs to be 'described'

=item Owner (optional)

Either a scalar or an array ref can be passed in, will cause AMIs owned by the Owner's provided will be 'described'. Pass either account ids, or 'amazon' for all amazon-owned AMIs, or 'self' for your own AMIs.

=item ExecutableBy (optional)

Either a scalar or an array ref can be passed in, will cause AMIs executable by the account id's specified.  Or 'self' for your own AMIs.

=back

Returns: a data structure like this: or undef if an error occured

	[
		{
			'imageOwnerId' => '',
			'imageState' => '',
			'isPublic' => '',
			'imageLocation' => '',
			'imageId' => ''
		},
		...
	]

=cut

sub describe_images {
	my $self = shift;
	my %args = validate( @_, {
										ImageId			=> { type => SCALAR | ARRAYREF, optional => 1 },
										Owner			=> { type => SCALAR | ARRAYREF, optional => 1 },
										ExecutableBy	=> { type => SCALAR | ARRAYREF, optional => 1 },
	});
	
	# If we have a array ref of instances lets split them out into their ImageId.n format
	if (ref ($args{ImageId}) eq 'ARRAY') {
		my $image_ids = delete $args{ImageId};
		my $count = 1;
		foreach my $image_id (@{$image_ids}) {
			$args{"ImageId." . $count} = $image_id;
			$count++;
		}
	}
	
	# If we have a array ref of instances lets split them out into their Owner.n format
	if (ref ($args{Owner}) eq 'ARRAY') {
		my $owners = delete $args{Owner};
		my $count = 1;
		foreach my $owner (@{$owners}) {
			$args{"Owner." . $count} = $owner;
			$count++;
		}
	}

	# If we have a array ref of instances lets split them out into their ExecutableBy.n format
	if (ref ($args{ExecutableBy}) eq 'ARRAY') {
		my $executors = delete $args{ExecutableBy};
		my $count = 1;
		foreach my $executor (@{$executors}) {
			$args{"ExecutableBy." . $count} = $executor;
			$count++;
		}
	}

	my $xml = $self->_sign(Action  => 'DescribeImages', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};
		
		return undef;
	}
	else {
		return $xml->{imagesSet}{item};
	}
}

=head2 deregister_image(%params)

This method will deregister an AMI. It takes the following parameter:

=over

=item ImageId (required)

The image id of the AMI you want to deregister.

=back

Returns a boolean 1 for success 0 for failure.

=cut

sub deregister_image {
	my $self = shift;
	my %args = validate( @_, {
								ImageId	=> { type => SCALAR },
	});
	

	my $xml = $self->_sign(Action  => 'DeregisterImage', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml->{'return'};
	}
}

=head2 run_instances(%params)

This method will start instance(s) of AMIs on EC2. The parameters indicate which AMI to instantiate and how many / what properties they have:

=over

=item ImageId (required)

The image id you want to start an instance of.

=item MinCount (required)

The minimum number of instances to start.

=item MaxCount (required)

The maximum number of instances to start.

=item KeyName (optional)

The keypair name to associate this instance with.  If omitted, will use your default keypair.

=item SecurityGroup (optional)

An scalar or array ref. Will associate this instance with the group names passed in.  If omitted, will be associated with the default security group.

=item UserData (optional)

Optional data to pass into the instance being started.  Needs to be base64 encoded.

=item AddressingType (options)

Optional addressing scheme to launch the instance.  If passed in it should have a value of either "direct" or "public".

=back

Returns a data structure which looks like this:

 {
	'groups' => [
				'group1',
				'group2',
				'....'
			  ],
	'instance' => {
				  'instanceState' => {
									 'name' => '',
									 'code' => ''
								   },
				  'instanceId' => '',
				  'reason' => {},
				  'dnsName' => '',
				  'privateDnsName' => '',
				  'amiLaunchIndex' => '',
				  'keyName' => '',
				  'imageId' => ''
				},
	'ownerId' => '',
	'reservationId' => ''
 }

=cut 

sub run_instances {
	my $self = shift;
	my %args = validate( @_, {
								ImageId			=> { type => SCALAR },
								MinCount		=> { type => SCALAR },
								MaxCount		=> { type => SCALAR },
								KeyName			=> { type => SCALAR, optional => 1 },
								SecurityGroup	=> { type => SCALAR | ARRAYREF, optional => 1 },
								UserData		=> { type => SCALAR, optional => 1 },
								AddressingType	=> { type => SCALAR, optional => 1 },
	});
	
	# If we have a array ref of instances lets split them out into their SecurityGroup.n format
	if (ref ($args{SecurityGroup}) eq 'ARRAY') {
		my $security_groups = delete $args{SecurityGroup};
		my $count = 1;
		foreach my $security_group (@{$security_groups}) {
			$args{"SecurityGroup." . $count} = $security_group;
			$count++;
		}
	}

	my $xml = $self->_sign(Action  => 'RunInstances', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		# Lets create our custom data struct here
		my $struct;
		
		$struct->{reservationId} = $xml->{reservationId};
		$struct->{ownerId} = $xml->{ownerId};
		$struct->{instance} = $xml->{instancesSet}{item};
		foreach my $group_arr (@{$xml->{groupSet}{item}}) {
			push @{$struct->{groups}}, $group_arr->{groupId};
		}
		
		return $struct;
	}
}

=head2 describe_instances(%params)

This method pulls a list of the instances which are running or were just running.  The list can be modified by passing in some of the following parameters:

=over

=item InstanceId (optional)

Either a scalar or an array ref can be passed in, will cause just these instances to be 'described'

=back

Returns the following data structure, or undef if a failure has occured:

 [
          {
            'instance' => {
                            'instanceState' => {
                                               'name' => '',
                                               'code' => ''
                                             },
                            'instanceId' => '',
                            'reason' => {},
                            'privateDnsName' => '',
                            'dnsName' => '',
                            'keyName' => '',
                            'imageId' => ''
                          },
            'ownerId' => '',
            'reservationId' => '',
            'groups' => [
                          ''
                        ],            
          },
 ];

=cut

sub describe_instances {
	my $self = shift;
	my %args = validate( @_, {
								InstanceId => { type => SCALAR | ARRAYREF, optional => 1 },
	});
	
	# If we have a array ref of instances lets split them out into their InstanceId.n format
	if (ref ($args{InstanceId}) eq 'ARRAY') {
		my $instance_ids = delete $args{InstanceId};
		my $count = 1;
		foreach my $instance_id (@{$instance_ids}) {
			$args{"InstanceId." . $count} = $instance_id;
			$count++;
		}
	}
	
	my $xml = $self->_sign(Action  => 'DescribeInstances', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		# Lets create our custom data struct here
		my $structs;
		
		foreach my $instance (@{$xml->{reservationSet}{item}}) {
			my $struct;
			$struct->{reservationId} = $instance->{reservationId};
			$struct->{ownerId} = $instance->{ownerId};
			$struct->{instance} = $instance->{instancesSet}{item};
			
			foreach my $group_arr (@{$instance->{groupSet}{item}}) {
					push @{$struct->{groups}}, $group_arr->{groupId};
			}

			push @{$structs}, $struct;
		}
		
		return $structs;
	}
}

=head2 terminate_instances(%params)

This method shuts down instance(s) passed into it. It takes the following parameter:

=over

=item InstanceId (required)

Either a scalar or an array ref can be passed in (containing instance ids)

=back

Returns the following data struct or undef:

 [
          {
            'instanceId' => '',
            'shutdownState' => {
                               'name' => '',
                               'code' => ''
                             },
            'previousState' => {
                               'name' => '',
                               'code' => ''
                             }
          },
          ...
 ];

=cut

sub terminate_instances {
	my $self = shift;
	my %args = validate( @_, {
								InstanceId => { type => SCALAR | ARRAYREF, optional => 1 },
	});
	
	# If we have a array ref of instances lets split them out into their InstanceId.n format
	if (ref ($args{InstanceId}) eq 'ARRAY') {
		my $instance_ids = delete $args{InstanceId};
		my $count = 1;
		foreach my $instance_id (@{$instance_ids}) {
			$args{"InstanceId." . $count} = $instance_id;
			$count++;
		}
	}
	
	my $xml = $self->_sign(Action  => 'TerminateInstances', %args);	


	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		my $structs;
		
		$structs = $xml->{instancesSet}{item};

		return $structs;
	}
}

=head2 create_key_pair(%params)

Creates a new 2048 bit key pair, taking the following parameter:

=over

=item KeyName (required)

A name for this key. Should be unique.

=back

Returns a hashref containing:

=over

=item keyName 

The key name provided in the original request.

=item KeyFingerprint

A SHA-1 digest of the DER encoded private key.

=item KeyMaterial

An unencrypted PEM encoded RSA private key.

=back

or undef if the creation failed.

=cut

sub create_key_pair {
	my $self = shift;
	my %args = validate( @_, {
								KeyName => { type => SCALAR },
	});
		
	my $xml = $self->_sign(Action  => 'CreateKeyPair', %args);

	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml;
	}
}

=head2 describe_key_pairs(%params)

This method describes the keypairs available on this account. It takes the following parameter:

=over

=item KeyName (optional)

The name of the key to be described. Can be either a scalar or an array ref.

=back

Returns a data structure like: (or undef if an error occured)

 [
          {
            'keyFingerprint' => '',
            'keyName' => ''
          },
          ...
 ];


=cut

sub describe_key_pairs {
	my $self = shift;
	my %args = validate( @_, {
								KeyName => { type => SCALAR | ARRAYREF, optional => 1 },
	});
	
	# If we have a array ref of instances lets split them out into their InstanceId.n format
	if (ref ($args{KeyName}) eq 'ARRAY') {
		my $keynames = delete $args{KeyName};
		my $count = 1;
		foreach my $keyname (@{$keynames}) {
			$args{"KeyName." . $count} = $keyname;
			$count++;
		}
	}
	
	my $xml = $self->_sign(Action  => 'DescribeKeyPairs', %args);

	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {	
		my $structs;
		
		$structs = $xml->{keySet}{item};

		return $structs;
	}
}

=head2 delete_key_pair(%params)

This method deletes a keypair.  Takes the following parameter:

=over

=item KeyName (required)

The name of the key to delete.

=back

Returns 1 if the key was successfully deleted.

=cut

sub delete_key_pair {
	my $self = shift;
	my %args = validate( @_, {
								KeyName => { type => SCALAR },
	});
		
	my $xml = $self->_sign(Action  => 'DeleteKeyPair', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		if ($xml->{return} eq 'true') {
			return 1;
		}
		else {
			return undef;
		}
	}
	
}

=head2 modify_image_attribute(%params)

This method modifies attributes of an AMI on EC2.  Right now the only attribute that can be modified is to grant launch permissions.  It takes the following parameters:

=over

=item ImageId (required)

The AMI to modify the attributes of.

=item Attribute (required)

The attribute you wish to modify, right now the only attribute that exists is launchPermission.

=item OperationType (required)

The operation you wish to perform on the attribute. Right now just 'add' and 'remove' are supported.

=item UserId (required when changing the launchPermission attribute)

User Id's you wish to add/remove from the attribute (right now just for launchPermission)

=item UserGroup (required when changing the launchPermission attribute)

Groups you wish to add/remove from the attribute (right now just for launchPermission).  Currently there is only one User Group available 'all' for all Amazon EC2 customers.

=back

Returns 1 if the modification succeeds, or 0 if it does not.

=cut

sub modify_image_attribute {
	my $self = shift;
	my %args = validate( @_, {
								ImageId			=> { type => SCALAR },
								Attribute 		=> { type => SCALAR },
								OperationType	=> { type => SCALAR },
								UserId 			=> { type => SCALAR | ARRAYREF, optional => 1 },
								UserGroup 		=> { type => SCALAR | ARRAYREF, optional => 1 },
	});
	
	
	my $xml = $self->_sign(Action  => 'ModifyImageAttribute', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml->{return};
	}	
}

=head2 reset_image_attribute(%params)

This method resets an attribute for an AMI to its default state.  It takes the following parameters:

=over

=item ImageId (required)

The image id of the AMI you wish to reset the attributes on.

=item Attribute (required)

The attribute you want to reset.  Right now the only attribute which exists is launchPermission.

=back

Returns 1 if the reset succeeds, or 0 if it does not.

=cut

sub reset_image_attribute {
	my $self = shift;
	my %args = validate( @_, {
								ImageId			=> { type => SCALAR },
								Attribute 		=> { type => SCALAR },
	});
	
	my $xml = $self->_sign(Action  => 'ResetImageAttribute', %args);

	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml->{return};
	}	
}

=head2 create_security_group(%params)

This method creates a new security group.  It takes the following parameters:

=over

=item GroupName (required)

The name of the new group to create.

=item GroupDescription (required)

A short description of the new group.

=back

Returns 1 if the group creation succeeds.

=cut

sub create_security_group {
	my $self = shift;
	my %args = validate( @_, {
								GroupName				=> { type => SCALAR },
								GroupDescription 		=> { type => SCALAR },
	});
	
	
	my $xml = $self->_sign(Action  => 'CreateSecurityGroup', %args);

	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml->{return};
	}	
}

=head2 describe_security_groups(%params)

This method describes the security groups available to this account. It takes the following parameter:

=over

=item GroupName (optional)

The name of the security group(s) to be described. Can be either a scalar or an array ref.

=back

Returns the data structure: (or undef if error)

 [
	{
	'ipPermissions' => {
					   'item' => [
								 {
								   'ipRanges' => {
												 'item' => [
														   {
															 'cidrIp' => ''
														   }
														 ]
											   },
								   'toPort' => '',
								   'fromPort' => '',
								   'groups' => {},
								   'ipProtocol' => 'tcp'
								 },
								 ...
					 	],
	'groupDescription' => '',
	'groupName' => '',
	'ownerId' => ''
	},
	...
 ]

=cut

sub describe_security_groups {
	my $self = shift;
	my %args = validate( @_, {
								GroupName => { type => SCALAR | ARRAYREF, optional => 1 },
	});
	
	# If we have a array ref of instances lets split them out into their InstanceId.n format
	if (ref ($args{GroupName}) eq 'ARRAY') {
		my $groups = delete $args{GroupName};
		my $count = 1;
		foreach my $group (@{$groups}) {
			$args{"GroupName." . $count} = $group;
			$count++;
		}
	}
	
	my $xml = $self->_sign(Action  => 'DescribeSecurityGroups', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml->{securityGroupInfo}{item};
	}	
}

=head2 delete_security_group(%params)

This method deletes a security group.  It takes the following parameter:

=over

=item GroupName (required)

The name of the security group to delete.

=back

Returns 1 if the delete succeeded.

=cut

sub delete_security_group {
	my $self = shift;
	my %args = validate( @_, {
								GroupName => { type => SCALAR },
	});
	
	
	my $xml = $self->_sign(Action  => 'DeleteSecurityGroup', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml->{return};
	}
}

=head2 authorize_security_group_ingress(%params)

This method adds permissions to a security group.  It takes the following parameters:

=over

=item GroupName (required)

The name of the group to add security rules to.

=item SourceSecurityGroupName (requred when authorizing a user and group together)

Name of the group to add access for.

=item SourceSecurityGroupOwnerId (required when authorizing a user and group together)

Owner of the group to add access for.

=item IpProtocol (required when adding access for a CIDR)

IP Protocol of the rule you are adding access for (TCP, UDP, or ICMP)

=item FromPort (required when adding access for a CIDR)

Beginning of port range to add access for.

=item ToPort (required when adding access for a CIDR)

End of port range to add access for.

=item CidrIp (required when adding access for a CIDR)

The CIDR IP space we are adding access for.

=back

Adding a rule can be done in two ways: adding a source group name + source group owner id, or, by Protocol + start port + end port + CIDR IP.  The two are mutally exclusive.

Returns 1 if rule is added successfully.

=cut

sub authorize_security_group_ingress {
	my $self = shift;
	my %args = validate( @_, {
								GroupName					=> { type => SCALAR },
								SourceSecurityGroupName 	=> { 
																	type => SCALAR,
																	depends => ['SourceSecurityGroupOwnerId'],
																	optional => 1 ,
								},
								SourceSecurityGroupOwnerId	=> { type => SCALAR, optional => 1 },
								IpProtocol 					=> { 
																	type => SCALAR,
																	depends => ['FromPort', 'ToPort', 'CidrIp'],
																	optional => 1 
								},
								FromPort 					=> { type => SCALAR, optional => 1 },
								ToPort 						=> { type => SCALAR, optional => 1 },
								CidrIp						=> { type => SCALAR, optional => 1 },
	});
	
	
	my $xml = $self->_sign(Action  => 'AuthorizeSecurityGroupIngress', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml->{return};
	}
}

=head2 revoke_security_group_ingress(%params)

This method revoke permissions to a security group.  It takes the following parameters:

=over

=item GroupName (required)

The name of the group to revoke security rules from.

=item SourceSecurityGroupName (requred when authorizing a user and group together)

Name of the group to revoke access from.

=item SourceSecurityGroupOwnerId (required when authorizing a user and group together)

Owner of the group to revoke access from.

=item IpProtocol (required when revoking access from a CIDR)

IP Protocol of the rule you are revoking access from (TCP, UDP, or ICMP)

=item FromPort (required when revoking access from a CIDR)

Beginning of port range to revoke access from.

=item ToPort (required when revoking access from a CIDR)

End of port range to revoke access from.

=item CidrIp (required when revoking access from a CIDR)

The CIDR IP space we are revoking access from.

=back

Revoking a rule can be done in two ways: revoking a source group name + source group owner id, or, by Protocol + start port + end port + CIDR IP.  The two are mutally exclusive.

Returns 1 if rule is revoked successfully.

=cut

sub revoke_security_group_ingress {
	my $self = shift;
	my %args = validate( @_, {
								GroupName					=> { type => SCALAR },
								SourceSecurityGroupName 	=> { 
																	type => SCALAR,
																	depends => ['SourceSecurityGroupOwnerId'],
																	optional => 1 ,
								},
								SourceSecurityGroupOwnerId	=> { type => SCALAR, optional => 1 },
								IpProtocol 					=> { 
																	type => SCALAR,
																	depends => ['FromPort', 'ToPort', 'CidrIp'],
																	optional => 1 
								},
								FromPort 					=> { type => SCALAR, optional => 1 },
								ToPort 						=> { type => SCALAR, optional => 1 },
								CidrIp						=> { type => SCALAR, optional => 1 },
	});
	
	
	my $xml = $self->_sign(Action  => 'RevokeSecurityGroupIngress', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml->{return};
	}
}

=head2 get_console_output(%params)

This method gets the output from the virtual console for an instance.  It takes the following parameters:

=over

=item InstanceId (required)

A scalar containing a instance id.

=back

Returns the output from the console for the instance.

=cut

sub get_console_output {
	my $self = shift;
	my %args = validate( @_, {
								InstanceId					=> { type => SCALAR },
	});
	
	
	my $xml = $self->_sign(Action  => 'GetConsoleOutput', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return decode_base64($xml->{output});
	}
}

=head2 reboot_instances(%params)

This method reboots an instance.  It takes the following parameters:

=over

=item InstanceId (required)

Instance Id of the instance you wish to reboot. Can be either a scalar or array ref of instances to reboot.

=back

Returns 1 if the reboot succeeded.

=cut

sub reboot_instances {
	my $self = shift;
	my %args = validate( @_, {
								InstanceId					=> { type => SCALAR },
	});
	
	# If we have a array ref of instances lets split them out into their InstanceId.n format
	if (ref ($args{InstanceId}) eq 'ARRAY') {
		my $instance_ids = delete $args{InstanceId};
		my $count = 1;
		foreach my $instance_id (@{$instance_ids}) {
			$args{"InstanceId." . $count} = $instance_id;
			$count++;
		}
	}
	
	my $xml = $self->_sign(Action  => 'RebootInstances', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		return $xml;
	}
}

=head2 describe_image_attributes(%params)

This method pulls a list of attributes for the image id specified

=over

=item ImageId (required)

A scalar containing the image you want to get the list of attributes for.

=item Attribute (required)

A scalar containing the attribute to describe.  Currently the only possible value for this is 'launchPermission'.

=back

Returns the following data structure, or undef if a failure has occured:

 {
	'ImageId' => '',
	'launchPermission' => {
					item => [
						'group' => '',
						'user_id' => ''
					]
	}
 };

=cut

sub describe_image_attribute {
	my $self = shift;
	my %args = validate( @_, {
								ImageId => { type => SCALAR },
								Attribute => { type => SCALAR }
	});
		
	my $xml = $self->_sign(Action  => 'DescribeImageAttribute', %args);
	
	if ( grep { defined && length } $xml->{Errors} ) {
		$self->_debug("ERROR: $xml->{Errors}{Error}{Message}");
		$self->{error} = $xml->{Errors}{Error}{Message};
		$self->{errorcode} = $xml->{Errors}{Error}{Code};

		return undef;
	}
	else {
		# Lets create our custom data struct here
		my $struct = { ImageId => $xml->{ImageId}, launchPermission => $xml->{launchPermission} };
		return $struct;
	}
}

1;

__END__

=head1 TODO

=over

=item * Add more automated testing.

=item * Make the data coming back an collection of objects rather than a arrayref/hashref based structure.

=back

=head1 TESTING

Due to the nature of this module automated testing is both difficult and expensive (since starting and stopping instances and putting data on S3 all cost money). So the tests run on a make test are fairly minimal (though, this module has been tested heavily)

=head1 AUTHOR

Jeff Kim <jkim@chosec.com>

=head1 COPYRIGHT

Copyright (c) 2006-2007 Jeff Kim.  All rights reserved.  This
program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 SEE ALSO

Amazon EC2 API: L<http://docs.amazonwebservices.com/AmazonEC2/dg/2007-01-19/>
