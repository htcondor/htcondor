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

# Wrap the default classad library and make it a little more 
# perlish.  Perl programmers shouldn't have to worry about memory
# management, strict typing and the like.

# Basic memory management wrapper
package ClassAd::Simple::Expr;

use strict;
use ClassAd;
use base qw(ClassAd::ExprTree ClassAd::Simple);

sub new
{
    my ($class, $src) = @_;
    
    my $expr = new ClassAd::ExprTree($src);
    return undef unless $expr;
    $expr->DISOWN;
    
    bless $expr, $class;
}

sub copy
{
    my ($this) = @_;

    my $new = $this->SUPER::copy;
    return undef unless $new;
    bless $new, ref($this);

    $new->DISOWN;

    return $new;
}

package ClassAd::Simple::Value;

use strict;
use ClassAd;
use base "ClassAd::Value";

# Typeless get_value.  A couple types still unaccounted for.
sub get_value
{
    my ($this) = @_;

    my $type = $this->get_type;
    if ($type eq "boolean")
    {
	return $this->get_boolean_value;
    }
    elsif ($type eq "integer")
    {
	return $this->get_integer_value;
    }
    elsif ($type eq "real")
    {
	return $this->get_real_value;
    }
    elsif ($type eq "string")
    {
	return $this->get_string_value;
    }
    elsif ($type eq "undefined")
    {
	return undef;
    }
    elsif ($type eq "classad")
    {
	my $ad = $this->get_classad_value;
	bless $ad, "ClassAd::Simple";
	return $ad;
    }
    elsif ($type eq "error")
    {
	return undef;
    }
    else
    {
	my $type = $this->get_type;
	print "Not sure: $type\n";
	return undef;
    }
}

package ClassAd::Simple::MatchClassAd;

use strict;
use base "ClassAd::Simple";

package ClassAd::Simple;

use strict;
use ClassAd;
use FileHandle;
use base "ClassAd::ClassAd";

use overload "==" => \&equality,
             '""' => \&as_string;

sub new
{
    my ($class, $src) = @_;

    # Can be called in two ways -- with a text representation or a hash thing
    my $ad;
    if (!ref($src))
    {
	$ad = new ClassAd::ClassAd($src);
    }
    elsif ($src)
    {
	$ad = new ClassAd::ClassAd(&_struct_to_text($src));
    }
    return undef unless $ad;

    $ad->DISOWN;

    bless $ad, $class;
}

# Create a list of new classads from the output of condor_status -l
# or similar.
sub new_from_old_classads
{
    my ($class, @text) = @_;

    my $text = join "", @text;

    my @ads = split /\n\n/, $text;
    @ads = map { s/\n/;\n/g; "[ $_ ]" } @ads;
    @ads = map { new ClassAd::Simple($_) } @ads;
    @ads = grep { $_ } @ads;
    
    return @ads;
}

# Converts perl data structures to classad text
sub _struct_to_text
{
    my ($ad) = @_;

    unless (ref($ad))
    {
	if ($ad =~ /^-?\d*(\.\d*)?$/)
	{
	    return $ad;
	}
	else
	{
	    return "\"$ad\"";
	}
    }

    my $text_ad = "[";
    foreach my $key (keys %$ad)
    {
	if (!ref $ad->{$key})
	{
	    $text_ad .= "$key = $ad->{$key}; ";
	}
	elsif (ref $ad->{$key} eq "ARRAY")
	{
	    $text_ad .= "$key = { " . join(", ", map { &_struct_to_text($_) } @{$ad->{$key}}) . "}; ";
	}
	elsif (ref $ad->{$key} eq "HASH")
	{
	    $text_ad .= "$key = " . &_struct_to_text($ad->{$key}) . "; ";
	}
    }

    $text_ad .= "]";
    return $text_ad;
}

sub equality
{
    my ($ad1, $ad2) = @_;

    return $ad1->same_as($ad2);
}

sub as_string
{
    my ($this) = @_;

    my $str = $this->unparse;
    $str =~ s/\n//;
    $str =~ s/^    //gm;
    return "$str\n";
}

*ClassAd::MatchClassAd::as_string = *ClassAd::ExprTree::as_string = *ClassAd::Simple::as_string;

sub flatten
{
    my ($this, $expr) = @_;

    my $value = $this->SUPER::flatten($expr);
    $value->DISOWN;
    
    bless $value, "ClassAd::Simple::Value";

    return $value->get_value;
}

sub attributes
{
    my ($this) = @_;

    my $iterator = new ClassAd::ClassAdIterator $this;
    $iterator->DISOWN;

    my @attrs;
    while (!$iterator->is_after_last)
    {
	push @attrs, $iterator->current_attribute;
	$iterator->next_attribute;
    }

    return @attrs;
}

# Figure out a type and do an appropriate insert
sub insert
{
    my ($this, %inserts) = @_;

    # Hmm, weak typing meets strong typing
    foreach my $key (keys %inserts)
    {
	if ($key =~ /\./)
	{
	    $this->deep_insert($key => $inserts{$key});
	}
	elsif (ref($inserts{$key}) eq "ClassAd::Simple")
	{
	    $this->insert_attr_classad($key, $inserts{$key});
	}
	elsif (ref($inserts{$key}) eq "ClassAd::Simple::Expr")
	{
	    $this->SUPER::insert($key, $inserts{$key});
	}
	elsif ($inserts{$key} =~ /^-?\d+$/)
	{
	    $this->insert_attr_int($key, $inserts{$key});
	}
	elsif ($inserts{$key} =~ /^-?\d+\.\d*$/)
	{
	    $this->insert_attr_double($key, $inserts{$key});
	}
	elsif (defined($inserts{$key}))
	{
	    $this->insert_attr_string($key, $inserts{$key});
	}
	else
	{
	    $this->SUPER::insert($key, new ClassAd::Simple::Expr("UNDEFINED"));
	}
    }
}

sub deep_insert
{
    
    my ($this, %inserts) = @_;
    
    foreach my $key (keys %inserts)
    {
	my ($scope, $attr) = $key =~ /(.*)\.(.*)/;
	my $scope_expr = new ClassAd::Simple::Expr $scope;
	$this->autovivicate($key);

	if (ref($inserts{$key}) eq "ClassAd::Simple")
	{
	    $this->deep_insert_attr_classad($scope_expr, $attr, $inserts{$key});
	}
	elsif (ref($inserts{$key}) eq "ClassAd::Simple::Expr")
	{
	    $this->SUPER::deep_insert($scope_expr, $attr, $inserts{$key});
	}
	elsif ($inserts{$key} =~ /^-?\d+$/)
	{
	    $this->deep_insert_attr_int($scope_expr, $attr, $inserts{$key});
	}
	elsif ($inserts{$key} =~ /^-?\d+\.\d*$/)
	{
	    $this->deep_insert_attr_double($scope_expr, $attr, $inserts{$key});
	}
	elsif (defined($inserts{$key}))
	{
	    $this->deep_insert_attr_string($scope_expr, $attr, $inserts{$key});
	}
	else
	{
	    $this->SUPER::deep_insert($scope_expr, $attr, new ClassAd::Simple::Expr("UNDEFINED"));
	}
    }
}

# Provide auto-vivication
sub autovivicate
{
    my ($this, $path) = @_;

    my @path = split /\./, $path;
    pop @path;
    
    my $scope;
    foreach my $element (@path)
    {
	$scope .= ".$element";
	$scope =~ s/^\.//;
	
	next if defined $this->evaluate_expr($scope);

	$this->insert($scope, new ClassAd::Simple {});
    }
}

# Symettric matching
sub match
{
    my ($this, $other_ad) = @_;

    my $matcher = new ClassAd::MatchClassAd($this->copy, $other_ad->copy);
    bless $matcher, "ClassAd::Simple::MatchClassAd";
    my ($match, $err) = $matcher->evaluate_attr_bool('symmetricMatch');

    return $match && $matcher;
}

# Asymmetric (r match l) matching
sub match_asymmetric
{
    my ($this, $other_ad) = @_;

    my $matcher = new ClassAd::MatchClassAd($this->copy, $other_ad->copy);
    bless $matcher, "ClassAd::Simple::MatchClassAd";
    my ($match, $err) = $matcher->evaluate_attr_bool('rightMatchesLeft');
    
    return $match && $matcher;
}

# copy w/memory management
sub copy
{
    my ($this) = @_;

    my $new_ad = $this->SUPER::copy;
    bless $new_ad, ref($this);
    
    $new_ad->DISOWN;
    
    return $new_ad;
}

# lookup w/memory management
sub lookup
{
    my ($this, $attr) = @_;

    my $expr = $this->SUPER::lookup($attr);
    bless $expr, "ClassAd::Simple::Expr";
    
    $expr->DISOWN;

    return $expr;
}

# evaluate_attr w/memory management
sub evaluate_attr
{
    my ($this, $attr) = @_;

    my ($value) = $this->SUPER::evaluate_attr($attr);

    bless $value, "ClassAd::Simple::Value";
    $value->DISOWN;

    return $value->get_value;
}

# typeless evaluate_expr w/memory management
sub evaluate_expr
{
    my ($this, $expr) = @_;

    my $value;
    if (ref($expr))
    {
	($value) = $this->evaluate_expr_expr($expr->copy);
    }
    else
    {
	($value) = $this->evaluate_expr_string($expr);
    }

    bless $value, "ClassAd::Simple::Value";
    $value->DISOWN;

    return $value->get_value;
}

1;
