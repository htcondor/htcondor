#!/usr/bin/perl

#  This software code is made available "AS IS" without warranties of any
#  kind.  You may copy, display, modify and redistribute the software
#  code either by itself or as incorporated into your code; provided that
#  you do not remove any proprietary notices.  Your use of this software
#  code is at your own risk and you waive any claim against Amazon
#  Digital Services, Inc. or its affiliates with respect to your use of
#  this software code. (c) 2006-2007 Amazon Digital Services, Inc. or its
#  affiliates.

package S3;

use base qw(Exporter);

@EXPORT_OK = qw(canonical_string encode merge_meta $DEFAULT_HOST $PORTS_BY_SECURITY $AMAZON_HEADER_PREFIX $METADATA_PREFIX $CALLING_FORMATS urlencode);

use strict;
use warnings;

use Carp;
use Digest::HMAC_SHA1;
use MIME::Base64 qw(encode_base64);
use URI::Escape;
use LWP::UserAgent;
use HTTP::Request;


our $DEFAULT_HOST = 's3.amazonaws.com';
our $PORTS_BY_SECURITY = { 0 => 80, 1 => 443 };
our $AMAZON_HEADER_PREFIX = 'x-amz-';
our $METADATA_PREFIX = 'x-amz-meta-';
our $CALLING_FORMATS = [
    "SUBDOMAIN", # http://bucket.s3.amazonaws.com/key
    "PATH",      # http://s3.amazonaws.com/bucket/key
    "VANITY",    # http://<vanity_domain>/key  -- vanity_domain resolves to s3.amazonaws.com
];

# The result of this subroutine is a string to which the resource path (sans
# bucket, including a leading '/') can be appended.  Examples include:
# - "http://s3.amazonaws.com/bucket"
# - "http://bucket.s3.amazonaws.com"
# - "http://<vanity_domain>"  -- vanity_domain resolves to s3.amazonaws.com
# 
# parameters:
# - protocol - ex: "http" or "https"
# - server   - ex: "s3.amazonaws.com"
# - port     - ex: "80"
# - bucket   - ex: "my_bucket"
# - format   - ex: "SUBDOMAIN"
sub build_url_base {
    my ($protocol, $server, $port, $bucket, $format) = @_;

    my $buf = "$protocol://";

    if ($bucket eq '') {
        $buf .= "$server:$port";
    }
    elsif ($format eq "PATH") {
        $buf .= "$server:$port/$bucket";
    }
    elsif ($format eq "SUBDOMAIN") {
        $buf .= "$bucket.$server:$port";
    }
    elsif ($format eq "VANITY") {
        $buf .= "$bucket:$port";
    }
    else {
        croak "unknown or unhandled CALLING_FORMAT";
    }

    return $buf;
}

sub trim {
    my ($value) = @_;

    $value =~ s/^\s+//;
    $value =~ s/\s+$//;
    return $value;
}

# Generate a canonical string for the given parameters.  Expires is optional and is
# only used by query string authentication.  $path is the resource NOT INCLUDING
# THE BUCKET.
sub canonical_string {
    my ($method, $bucket, $path, $path_args, $headers, $expires) = @_;
    my %interesting_headers = ();
    while (my ($key, $value) = each %$headers) {
        my $lk = lc $key;
        if (
            $lk eq 'content-md5' or
            $lk eq 'content-type' or
            $lk eq 'date' or
            $lk =~ /^$AMAZON_HEADER_PREFIX/
        ) {
            $interesting_headers{$lk} = trim($value);
        }
    }

    # these keys get empty strings if they don't exist
    $interesting_headers{'content-type'} ||= '';
    $interesting_headers{'content-md5'} ||= '';

    # just in case someone used this.  it's not necessary in this lib.
    $interesting_headers{'date'} = '' if $interesting_headers{'x-amz-date'};

    # if you're using expires for query string auth, then it trumps date
    # (and x-amz-date)
    $interesting_headers{'date'} = $expires if $expires;

    my $buf = "$method\n";
    foreach my $key (sort keys %interesting_headers) {
        if ($key =~ /^$AMAZON_HEADER_PREFIX/) {
            $buf .= "$key:$interesting_headers{$key}\n";
        } else {
            $buf .= "$interesting_headers{$key}\n";
        }
    }

    if ($bucket) {
        $buf .= "/$bucket"
    }
    $buf .= "/$path";

    my @special_arg_list = qw/acl location logging torrent/;
    my @found_special_args = grep { exists $path_args->{$_} } @special_arg_list;
    if (@found_special_args > 1) {
        croak "more than one special query-string argument found: " . join(",", @found_special_args);
    }
    elsif (@found_special_args == 1) {
        $buf .= "?$found_special_args[0]";
    }
    
    return $buf;
}

# finds the hmac-sha1 hash of the canonical string and the aws secret access key and then
# base64 encodes the result (optionally urlencoding after that).
sub encode {
    my ($aws_secret_access_key, $str, $urlencode) = @_;
    my $hmac = Digest::HMAC_SHA1->new($aws_secret_access_key);
    $hmac->add($str);
    my $b64 = encode_base64($hmac->digest, '');
    if ($urlencode) {
        return urlencode($b64);
    } else {
        return $b64;
    }
}

sub urlencode {
    my ($unencoded) = @_;
    return uri_escape($unencoded, '^A-Za-z0-9_-');
}

# generates an HTTP::Headers objects given one hash that represents http
# headers to set and another hash that represents an object's metadata.
sub merge_meta {
    my ($headers, $metadata) = @_;
    $headers ||= {};
    $metadata ||= {};

    my $http_header = HTTP::Headers->new;
    while (my ($k, $v) = each %$headers) {
        $http_header->header($k => $v);
    }
    while (my ($k, $v) = each %$metadata) {
        $http_header->header("$METADATA_PREFIX$k" => $v);
    }

    return $http_header;
}

# Build a URL arguments string from the path_args hash ref.
# Returns a string like: "key1=val1&key2=val2&key3=val3"
# This function handles url escaping keys and values.
sub path_args_hash_to_string {
  my ($path_args) = @_;

  my $arg_string = '';
  foreach my $key (keys %$path_args) {
    my $value = $path_args->{$key};
    $arg_string .= $key;
    if ($value) {
      $arg_string .= "=".S3::urlencode($value);
    }
    $arg_string .= '&';
  }
  chop $arg_string; # remove trailing '&'
  return $arg_string
}


1;
