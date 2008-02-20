#!/usr/bin/perl

#  This software code is made available "AS IS" without warranties of any
#  kind.  You may copy, display, modify and redistribute the software
#  code either by itself or as incorporated into your code; provided that
#  you do not remove any proprietary notices.  Your use of this software
#  code is at your own risk and you waive any claim against Amazon
#  Digital Services, Inc. or its affiliates with respect to your use of
#  this software code. (c) 2006-2007 Amazon Digital Services, Inc. or its
#  affiliates.

package S3::AWSAuthConnection;

use strict;
use warnings;

use HTTP::Date;
use URI::Escape;
use Carp;

use S3 qw($DEFAULT_HOST $PORTS_BY_SECURITY $CALLING_FORMATS merge_meta urlencode);
use S3::GetResponse;
use S3::ListBucketResponse;
use S3::ListAllMyBucketsResponse;
use S3::LocationResponse;
use S3::S3Object;

# new(id, key, is_secure=1, server=DEFAULT_HOST, port=DEFAULT, calling_format=DEFAULT)
sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self  = {};
    $self->{AWS_ACCESS_KEY_ID} = shift || croak "must specify aws access key id";
    $self->{AWS_SECRET_ACCESS_KEY} = shift || croak "must specify aws secret access key";
    $self->{IS_SECURE} = shift;
    $self->{IS_SECURE} = 1 if (not defined $self->{IS_SECURE});
    $self->{PROTOCOL} = $self->{IS_SECURE} ? 'https' : 'http';
    $self->{SERVER} = shift || $DEFAULT_HOST;
    $self->{PORT} = shift || $PORTS_BY_SECURITY->{$self->{IS_SECURE}};
    $self->{CALLING_FORMAT} = shift || $CALLING_FORMATS->[0];
    $self->{AGENT} = LWP::UserAgent->new();
    bless ($self, $class);
    return $self;
}

sub set_calling_format {
  my ($self, $calling_format) = @_;
  $self->{CALLING_FORMAT} = $calling_format;
}

sub get_calling_format {
  my ($self) = @_;
  return $self->{CALLING_FORMAT};
}

sub create_bucket {
    my ($self, $bucket, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    $headers ||= {};

    return S3::Response->new($self->_make_request('PUT', $bucket, '', {}, $headers));
}

sub create_located_bucket {
    my ($self, $bucket, $location, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    $headers ||= {};

	my $data = "";
	if ($location) {
        my $data = "<CreateBucketConstraint><LocationConstraint>$location</LocationConstraint></CreateBucketConstraint>"
    }
    return S3::Response->new($self->_make_request('PUT', $bucket, '', {}, $headers, $data));
}

sub get_bucket_location {
    my ($self, $bucket) = @_;
    return S3::LocationResponse->new($self->_make_request('GET', $bucket, '', {location => undef}));
}

sub check_bucket_exists {
    my ($self, $bucket) = @_;
    return S3::Response->new($self->_make_request('HEAD', $bucket, '', {}, {}));
}

sub list_bucket {
    my ($self, $bucket, $options, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    $options ||= {};
    $headers ||= {};

    return S3::ListBucketResponse->new($self->_make_request('GET', $bucket, '', $options, $headers));
}

sub delete_bucket {
    my ($self, $bucket, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    $headers ||= {};

    return S3::Response->new($self->_make_request('DELETE', $bucket, '', {}, $headers));
}

sub put {
    my ($self, $bucket, $key, $object, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    croak 'must specify key' unless $key;
    $headers ||= {};

    $key = urlencode($key);

    if (ref($object) ne 'S3::S3Object') {
        $object = S3::S3Object->new($object);
    }

    return S3::Response->new($self->_make_request('PUT', $bucket, $key, {}, $headers, $object->data, $object->metadata));
}

sub get {
    my ($self, $bucket, $key, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    croak 'must specify key' unless $key;
    $headers ||= {};

    $key = urlencode($key);

    return S3::GetResponse->new($self->_make_request('GET', $bucket, $key, {}, $headers));
}

sub delete {
    my ($self, $bucket, $key, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    croak 'must specify key' unless $key;
    $headers ||= {};

    $key = urlencode($key);

    return S3::Response->new($self->_make_request('DELETE', $bucket, $key, {}, $headers));
}

sub get_bucket_logging {
    my ($self, $bucket, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    return S3::GetResponse->new($self->_make_request('GET', $bucket, '', {logging => undef}, $headers));
}

sub put_bucket_logging {
    my ($self, $bucket, $logging_xml_doc, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    return S3::Response->new($self->_make_request('PUT', $bucket, '', {logging => undef}, $headers, $logging_xml_doc));
}

sub get_bucket_acl {
    my ($self, $bucket, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    return $self->get_acl($bucket, "", $headers);
}

sub get_acl {
    my ($self, $bucket, $key, $headers) = @_;
    croak 'must specify bucket' unless $bucket;
    croak 'must specify key' unless defined $key;
    $headers ||= {};

    $key = urlencode($key);

    return S3::GetResponse->new($self->_make_request('GET', $bucket, $key, {acl => undef}, $headers));
}

sub put_bucket_acl {
    my ($self, $bucket, $acl_xml_doc, $headers) = @_;
    return $self->put_acl($bucket, '', $acl_xml_doc, $headers);
}

sub put_acl {
    my ($self, $bucket, $key, $acl_xml_doc, $headers) = @_;
    croak 'must specify acl xml document' unless defined $acl_xml_doc;
    croak 'must specify bucket' unless $bucket;
    croak 'must specify key' unless defined $key;
    $headers ||= {};

    $key = urlencode($key);

    return S3::Response->new(
        $self->_make_request('PUT', $bucket, $key, {acl => undef}, $headers, $acl_xml_doc));
}

sub list_all_my_buckets {
    my ($self, $headers) = @_;
    $headers ||= {};

    return S3::ListAllMyBucketsResponse->new($self->_make_request('GET', '', '', {}, $headers));
}

# parameters:
# * method - "GET","PUT", etc
# * bucket - the bucket being accessed in this request
# * path - path within the bucket that will be accessed, possibly ""
# * path_args - a hash ref giving arguments that go in the url
# * headers - a hash ref specifying request HTTP headers, may be omitted
# * data - data to upload for a PUT request (and certain other requests), may be omitted
# * metadata - hash ref specifying metadata for a PUT request
sub _make_request {
    my ($self, $method, $bucket, $path, $path_args, $headers, $data, $metadata) = @_;
    croak 'must specify method' unless $method;
    croak 'must specify bucket' unless defined $bucket;
    $path ||= '';
    $headers ||= {};
    $data ||= '';
    $metadata ||= {};

    my $http_headers = merge_meta($headers, $metadata);

    $self->_add_auth_header($http_headers, $method, $bucket, $path, $path_args);
    my $url_base = S3::build_url_base($self->{PROTOCOL}, $self->{SERVER}, $self->{PORT}, $bucket, $self->{CALLING_FORMAT});
    my $url = "$url_base/$path";

    my $arg_string = S3::path_args_hash_to_string($path_args);
    if ($arg_string) {
        $url .= "?$arg_string";
    }

    while (1) {
	my $request = HTTP::Request->new($method, $url, $http_headers);
	$request->content($data);
	my $response = $self->{AGENT}->request($request);
	# if not redirect, return
	return $response unless ($response->code >= 300 && $response->code < 400);
	# retry on returned url
	$url = $response->header('location');
	return $response unless $url;
	croak "bad redirect url: $url" unless $url;
    }
}

sub _add_auth_header {
    my ($self, $headers, $method, $bucket, $path, $path_args) = @_;

    if (not $headers->header('Date')) {
        $headers->header(Date => time2str(time));
    }
    my $canonical_string = S3::canonical_string($method, $bucket, $path, $path_args, $headers);
    my $encoded_canonical = S3::encode($self->{AWS_SECRET_ACCESS_KEY}, $canonical_string);
    $headers->header(Authorization => "AWS ".$self->{AWS_ACCESS_KEY_ID}.":$encoded_canonical");
}

1;
