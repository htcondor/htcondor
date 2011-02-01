#!/usr/bin/env perl
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


use strict;
use ClassAd::Simple;
use Test::More tests => 21;

# Make ads from text and a hash, they should be the same
my $ad_text = "[ foo = 50; bar = [ foo = 1 ]; qux = { 1, 2, 3 }; ]";
my $ad_hash = { foo => 50, bar => { foo => 1 }, qux => [1, 2, 3] };
my $text_ad = new ClassAd::Simple $ad_text;
my $hash_ad = new ClassAd::Simple $ad_hash;
diag("Created from text:\n" . $hash_ad->as_string);
diag("Created from a hash:\n" . $text_ad->as_string);
ok($text_ad->same_as($hash_ad), "Creation (via text and hash) and comparison");

# Do a couple copy and make sure things are equal
my $cp = $hash_ad->copy;
diag("Created from copy:\n" . $cp->as_string);
ok($cp == $hash_ad, "copy");
$cp->copy_from($text_ad);
ok($cp == $text_ad, "copy_from");

# Try out matchmaking
my $job_ad = new ClassAd::Simple { requirements => "other.memory > 5",
			         imagesize => 50 };
my $machine_ad = new ClassAd::Simple { requirements => "other.imagesize < my.memory",
				     memory => 10 };
diag("Job ad:\n" . $job_ad->as_string);
diag("Machine ad:\n" . $machine_ad->as_string);
ok(!$job_ad->match($machine_ad), "Symmetric match");
ok($job_ad->match_asymmetric($machine_ad), "asymmetric_match");

# Insert some stuff
my $ad = new ClassAd::Simple {};
$ad->insert(string => "bar");
$ad->insert(int => 35);
$ad->insert(double => 35.5);
$ad->insert(expr => new ClassAd::Simple::Expr "int < 50");
$ad->insert(ad => new ClassAd::Simple "[ foo = string ]");
diag("Created from inserts:\n" . $ad->as_string);

is($ad->evaluate_attr("int"), 35, "insert/evaluate_attr int");
is($ad->evaluate_attr("double"), 35.5, "insert/evaluate_attr double");
is($ad->evaluate_attr("string"), "bar", "insert/evaluate_attr string");
ok($ad->evaluate_attr("expr"), "insert/evaluate_attr expr");
is($ad->evaluate_expr("ad.foo"), $ad->evaluate_attr("string"), "insert/evaluate_attr classad");

# Attribute iterator
my @attrs = $ad->attributes;
diag(join ", ", @attrs);
my %attrs = map { $_ => 1 } @attrs;
ok($attrs{int} && $attrs{double} && $attrs{string} && $attrs{expr} 
   && $attrs{ad} && @attrs == 5, "correct attributes");

# Deep inserts
$ad->insert("ad.string" => "bar");
$ad->insert("ad.int" => 35);
$ad->insert("ad.double" => 35.5);
$ad->insert("ad.expr" => new ClassAd::Simple::Expr "int < 50");
$ad->insert("ad.ad" => new ClassAd::Simple "[ foo = string ]");
diag("After deep inserts:\n" . $ad->as_string);

is($ad->evaluate_expr("ad.int"), 35, "deep insert/evaluate_attr int");
is($ad->evaluate_expr("ad.double"), 35.5, "deep insert/evaluate_attr double");
is($ad->evaluate_expr("ad.string"), "bar", "deep insert/evaluate_attr string");
ok($ad->evaluate_expr("ad.expr"), "deep insert/evaluate_attr expr");
is($ad->evaluate_expr("ad.ad.foo"), $ad->evaluate_attr("string"), "deep insert/evaluate_attr classad");

# Lookup+evaluate
my $expr = $ad->lookup("expr");
ok($expr->as_string eq "int < 50\n", "lookup");
ok($ad->evaluate_expr($expr), "evaluate_expr");

# Clearing
$ad->clear;
diag("After clear:\n" . $ad->as_string);
ok($ad == new ClassAd::Simple {}, "clear");

# Create from old classads
my $old_style = "MyType = \"Machine\"\nName = \"vail.cs.wisc.edu\"\n";
my $new_style = "[ MyType = \"Machine\"; Name = \"vail.cs.wisc.edu\" ]";
my ($old_ad) = ClassAd::Simple->new_from_old_classads($old_style);
my $new_ad = new ClassAd::Simple $new_style;
diag("Old style:\n$old_ad");
diag("New style:\n$new_ad");
ok($old_ad == $new_ad, "Creating from old classads");

# Some functions
$ad = new ClassAd::Simple "[ one = 1; foo = 50; bar = [ foo = 1 ]; qux = { 1, 2, 3 }; ]";
ok($ad->evaluate_expr("member(1, { 1, 2, 3 })"), "member()");

# Flatten
my $ad = new ClassAd::Simple "[ str = \"foo\"; foo = [ foo = 3 + 5; bar = str ] ]";
diag($ad);
my $expr = $ad->evaluate_attr("foo");
$expr = $ad->flatten($expr);
diag($expr);
