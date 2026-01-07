#! /usr/bin/env perl

open my $fh, ">>", "job_dagman_submit_fails_post-nodeA-post.out" or die "Cannot open file: $!";
print $fh "Node A post script ($ARGV[0])\n";
close $fh;
