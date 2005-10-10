#!/usr/bin/env perl

use strict;
use ClassAd::Simple;
use Test::More tests => 14;

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
diag("Created from inserts:\n" . $ad->as_string);

is($ad->evaluate_attr("int"), 35, "insert/evaluate_attr int");
is($ad->evaluate_attr("double"), 35.5, "insert/evaluate_attr double");
cmp_ok($ad->evaluate_attr("string"), 'eq', "bar", "insert/evaluate_attr string");
ok($ad->evaluate_attr("expr"), "insert/evaluate_attr expr");

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
