#!/usr/bin/perl

#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.



use warnings;
use strict;
use Getopt::Std;
use Fcntl qw(SEEK_SET SEEK_CUR SEEK_END O_RDONLY); #SEEK_SET=0 SEEK_CUR=1 ...
use Text::Diff;
use Tie::File;
use Algorithm::Diff qw( diff );
use Data::Dumper;
use v5.10;   # uses specific code for version 5.10: "$ref->[$_] //= '' for 0 .. $#$ref;"

#-------- constants --------
my $default_mem_cache = (2*1024*1024*1024); #2GB
#------- /constants --------

my $beverbose;

sub printerr
{
  print STDERR "@_";
}

sub printverb
{
    printerr @_ if defined $beverbose and $beverbose != 0;
}

sub usage {
  printerr <<EOL;
usage:
lfdiff [-h] [-v] [-f] [-o OUTPUT] [-m MEM] INPUT1 INPUT2
\t-f: force overwriting
\t-o: write output to OUTFILE instead of stdout
\t-m: use MEM bytes for file caching (default:$default_mem_cache)
\t-v: be verbose
\t-h: print usage
EOL
}

sub errorexit {
  printerr @_;
  usage;
  exit 1;
}


# return the maximum line number which is below the given cumulative bytes
sub get_max_line_for_bytes {
	my $arr_ref = shift;
	my $start_line = shift;
	my $max_bytes = shift;


	return ($max_bytes / 200); # each line got 200 bytes on average
}



my %args;
#$Getopt::Std::STANDARD_HELP_VERSION = 1;
if (getopts( 'ho:vfm:', \%args )) {
#  errorexit "error parsing commandline\n";
}

if (defined $args{h} and $args{h} ne '') {
    usage();
    exit 0;
}


$beverbose = $args{v};
my $mem_cache = $args{m} || $default_mem_cache;
my $useforce = $args{f};
my $filename = $args{o};

if ($#ARGV+1 < 2) {
    errorexit "error: missing input files\n";
}

my $fileA = $ARGV[0];
my $fileB = $ARGV[1];

if ($fileA eq $fileB) {
    errorexit "error: input files are same\n";
}


if (defined $filename) {
    # redirect STDOUT to file
    if (not open (STDOUT, ">", $filename)) {
        if (defined $useforce) {
            unlink $filename or die "Can not remove '$filename': $!";
            open (STDOUT, ">", $filename) or die "Unable to open file '$filename': $!";
        }
        else {
            die "Unable to open file '$filename': $!";
        }
    }
}


my @array_a;
my @array_b;
tie @array_a, 'Tie::File', $fileA, autochomp => 0, mode => O_RDONLY, memory => $mem_cache or die "error: could not open file '$fileA': $!";
tie @array_b, 'Tie::File', $fileB, autochomp => 0, mode => O_RDONLY, memory => $mem_cache or die "error: could not open file '$fileB': $!";
#, autochomp => 0
#, mode => O_RDONLY
#, memory => 20_000_000

my @part_array_a;
my @part_array_b;

printverb "diffing '$fileA' and '$fileB'".(defined $filename?" into '$filename'":'')."\n";
my @virtual_file_a;
my @virtual_file_b;
my @diff;
my $maxdiffline = 0;
my $lineoffset = 0;


sub diff_discard_a {
    my $index = shift;
    $virtual_file_a[$index+$lineoffset] = $part_array_a[$index];
}

sub diff_discard_b {
    my $index = shift;
    $virtual_file_b[$index+$lineoffset] = $part_array_b[$index];
}



print "diffing from 0..999\n";
@part_array_a = @array_a[0..999];
@part_array_b = @array_b[0..999];
#@diff = Algorithm::Diff::diff( \@part_array_a, \@part_array_b );
#print Dumper(\@diff);
#Algorithm::Diff::traverse_sequences(
#        \@part_array_a, \@part_array_b,
#        {   #MATCH => diff_discard_a,
#            DISCARD_A => \&diff_discard_a,
#            DISCARD_B => \&diff_discard_b,
#        }
#    );
Algorithm::Diff::traverse_balanced(
        \@part_array_a, \@part_array_b,
        {   #MATCH => diff_discard_a,
            DISCARD_A => \&diff_discard_a,
            DISCARD_B => \&diff_discard_b,
        }
    );
#print "virtual file a:\n", Dumper(\@virtual_file_a);
#print "virtual file b:\n", Dumper(\@virtual_file_b);
#exit;

#@part_array_a = @array_a[1001..2000];
#@part_array_b = @array_b[1001..2000];
#push @diff, Algorithm::Diff::diff( \@part_array_a, \@part_array_b );
#


# Dumper(\@diff) :
# $VAR1 = [             # @diff
#          [            # $splitdiff, @diffblock
#            [          # $diffsplit, @diffsplit
#              '-',     # $diffsplit[0], $prefix
#              638,     # $diffsplit[1], $line
#              '                         bandbreite_neu = 100
#'                      # $diffsplit[2], $value
#            ],
#            [
#              '+',
#              639,
#              '                         bandbreite_neu_vectoring = 100
#'
#            ]
#          ],
#         ]

#foreach my $splitdiff (@diff) {
#    my @diffblock = @{$splitdiff};
#    foreach my $diffsplit (@diffblock) {
#        my @diffsplit = @{$diffsplit};
#        my ($prefix, $line, $value) = @diffsplit;
#        $line += $lineoffset;
#        if ($prefix eq '-') {
#            $virtual_file_a[$line] = $value;
#        }
#        elsif ($prefix eq '+') {
#            $virtual_file_b[$line] = $value;
#        }
#        else {
#            errorexit "unknown prefix in diff data array found: '$prefix'\n";
#        }
#        $maxdiffline = $line if $line > $maxdiffline;
#    }
#}



print "diffing from 1000..1999\n";
@part_array_a = @array_a[1000..1999];
@part_array_b = @array_b[1000..1999];
#@diff = Algorithm::Diff::diff( \@part_array_a, \@part_array_b );
#print Dumper(\@diff);

$lineoffset = 1000;
#Algorithm::Diff::traverse_sequences(
#        \@part_array_a, \@part_array_b,
#        {   #MATCH => diff_discard_a,
#            DISCARD_A => \&diff_discard_a,
#            DISCARD_B => \&diff_discard_b,
#        }
#    );
Algorithm::Diff::traverse_balanced(
        \@part_array_a, \@part_array_b,
        {   #MATCH => diff_discard_a,
            DISCARD_A => \&diff_discard_a,
            DISCARD_B => \&diff_discard_b,
        }
    );
print "virtual file a:\n", Dumper(\@virtual_file_a);
print "virtual file b:\n", Dumper(\@virtual_file_b);
exit;


#foreach my $splitdiff (@diff) {
#    my @diffblock = @{$splitdiff};
#    foreach my $diffsplit (@diffblock) {
#        my @diffsplit = @{$diffsplit};
#        my ($prefix, $line, $value) = @diffsplit;
#        $line += $lineoffset;
#        if ($prefix eq '-') {
#            $virtual_file_a[$line] = $value;
#        }
#        elsif ($prefix eq '+') {
#            $virtual_file_b[$line] = $value;
#        }
#        else {
#            errorexit "unknown prefix in diff data array found: '$prefix'\n";
#        }
#        $maxdiffline = $line if $line > $maxdiffline;
#    }
#}



print "diffing from 2000..2999\n";
@part_array_a = @array_a[2000..2999];
@part_array_b = @array_b[2000..2999];
@diff = Algorithm::Diff::diff( \@part_array_a, \@part_array_b );
print Dumper(\@diff);


$lineoffset = 2000;
foreach my $splitdiff (@diff) {
    my @diffblock = @{$splitdiff};
    foreach my $diffsplit (@diffblock) {
        my @diffsplit = @{$diffsplit};
        my ($prefix, $line, $value) = @diffsplit;
        $line += $lineoffset;
        if ($prefix eq '-') {
            $virtual_file_a[$line] = $value;
        }
        elsif ($prefix eq '+') {
            $virtual_file_b[$line] = $value;
        }
        else {
            errorexit "unknown prefix in diff data array found: '$prefix'\n";
        }
        $maxdiffline = $line if $line > $maxdiffline;
    }
}





# assign empty values to the undef array slots
# make both arrays same size with $maxdiffline
for my $ref (\@virtual_file_a, \@virtual_file_b) {
#  $ref->[$_] //= "\n" for 0 .. $#$ref;
#  $ref->[$_] //= "" for 0 .. $#$ref;
#  $ref->[$_] //= "\n" for 0 .. $maxdiffline;
#  $ref->[$_] //= "" for 0 .. $maxdiffline;
  $ref->[$_] //= "void\n" for 0 .. $maxdiffline;
}


#open FILE, ">", "dump_mwgeo-201609210500.sql.split10000.zz" or die "$!";
#foreach my $line (@virtual_file_a) {
#    print FILE $line;
#}
#close FILE;
#open FILE, ">", "dump_mwgeo-201609220500.sql.split10000.zz" or die "$!";
#foreach my $line (@virtual_file_b) {
#    print FILE $line;
#}
#close FILE;



#my $diff = 
Text::Diff::diff \@virtual_file_a,  \@virtual_file_b, { STYLE => "OldStyle", OUTPUT => \*STDOUT };


