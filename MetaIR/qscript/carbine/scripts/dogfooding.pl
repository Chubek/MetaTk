#!/usr/bin/env perl
# Dogfooding: builds Carbine's own tooling, runs its tests, and, when a
# QScript runtime is available, runs the carbine host on this project's
# Carbinefile.
#
#   perl scripts/dogfooding.pl [QSCRIPT_ROOT]
#
# QSCRIPT_ROOT (or the environment variable of the same name) enables the
# host. Without it only the C tooling and the engine tests run.
use strict;
use warnings;
use FindBin;
use File::Spec;

my $root = File::Spec->rel2abs(File::Spec->catdir($FindBin::Bin, File::Spec->updir));
chdir $root or die "cannot enter $root: $!\n";

my $qs = $ARGV[0] // $ENV{QSCRIPT_ROOT} // '';

sub run {
    my @cmd = @_;
    print "+ @cmd\n";
    system(@cmd) == 0 or die "dogfooding: failed: @cmd\n";
}

my @configure = ('cmake', '-S', '.', '-B', 'build');
push @configure, '-DCARBINE_WITH_QSCRIPT=ON', "-DQSCRIPT_ROOT=$qs" if length $qs;

run(@configure);
run('cmake', '--build', 'build');
run('ctest', '--test-dir', 'build', '--output-on-failure');
run('python3', 'tests/engine_test.py');

if (length $qs) {
    run('build/carbine', 'check');
    run('build/carbine', 'gen', 'ninja');
    run('ninja');
} else {
    print "dogfooding: no QScript runtime given; skipped the carbine host.\n";
}

print "dogfooding: ok\n";
