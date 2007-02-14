#!/usr/bin/perl

use strict;
use List::Util "sum";

# default to taking a snapshot after every 4 actions
my $snapshot_period = 4;

# default size to build the initial tree up to is 15
my $initial_tree_size = 15;

# default number of actions to perform after tree is built is 50
my $num_actions = 50;

# default proporties of commands to generate:
#     45% - spawn
#     45% - die
#     10% - create subfamily
my @command_weights = (45, 45, 10);

# the controller will start with a single node with id 1
my @node_list = (1);

# the next node id to give out
my $next_node_id = 2;

# uniformly randomly select a member of the given list
sub uniform_random_index
{
	my $list_size = scalar(@_);
	my $index = int(rand($list_size));
	return $index;
}

# generate a random index using weights from the given list
sub weighted_random_index
{
	my @input_list = @_;
	my $rv = rand(sum(@input_list));
	my $cummulative_weight = 0;
	my $index = 0;
	foreach my $weight (@input_list) {
		$cummulative_weight += $weight;
		if ($rv < $cummulative_weight) {
			return $index;
		}
		$index += 1;
	}
}

# generate a spawn command
sub spawn
{
	# randomly select a parent
	my $parent_index = uniform_random_index(@node_list);

	# assign the child its node id
	my $child_id = $next_node_id++;
	
	# update our node list with the new ID
	push(@node_list, $child_id);

	# output the command to the controller
	print "SPAWN $node_list[$parent_index] $child_id\n";

	return $child_id
}

# generate a die command
sub die
{
	# randomly select a victim
	my $index  = uniform_random_index(@node_list);

	# remove the node from our node list
	my $node_id = splice(@node_list, $index, 1);

	# output the command to the controller
	print "DIE $node_id\n";
}

# generate a create subfamily command sequence
sub create_subfamily
{
	# first spawn
	my $child_id = spawn();

	# then write out command to register the family
	print "REGISTER_SUBFAMILY $child_id\n";
}

# generate a snapshot command
sub snapshot
{
	print "SNAPSHOT\n";
}

#
# main routine
#

# seed the RNG
srand;

# create the "initial tree"
for (my $i = 0; $i < $initial_tree_size; $i++) {
	spawn();
}

my @commands = (\&spawn, \&die, \&create_subfamily);
for (my $i = 0; $i < $num_actions; $i++) {
	if ($i % $snapshot_period == 0) {
		snapshot();
	}
	$commands[weighted_random_index(@command_weights)]();
}

# finish it off with a snapshot
snapshot();
