use strict;
use warnings;
use Test::More;
use File::Basename qw(dirname);
use File::Spec;
use File::Temp qw(tempdir);
use IPC::Open3;
use Symbol qw(gensym);

my $generator = File::Spec->catfile(dirname(__FILE__), 'aurocks.pl');
my $dir = tempdir(CLEANUP => 1);
my $serial = 0;

sub generate {
    my ($source, @options) = @_;
    my $path = File::Spec->catfile($dir, 'grammar' . ++$serial . '.g');
    open my $file, '>', $path or die "open $path: $!";
    print {$file} $source;
    close $file or die "close $path: $!";
    my $err = gensym;
    my $pid = open3(my $in, my $out, $err, $^X, $generator, @options, $path);
    close $in;
    local $/;
    my $c = <$out> // '';
    my $diagnostic = <$err> // '';
    waitpid($pid, 0);
    return ($? >> 8, $c, $diagnostic);
}

sub compile_and_run {
    my ($source) = @_;
    my $path = File::Spec->catfile($dir, 'parser.c');
    my $binary = File::Spec->catfile($dir, 'parser');
    open my $file, '>', $path or die "open $path: $!";
    print {$file} $source;
    close $file or die "close $path: $!";
    return 0 if system('cc', '-std=c99', '-Wall', '-Wextra', '-o', $binary, $path) != 0;
    return system($binary) == 0;
}

my ($status, $c, $err) = generate(<<'GRAMMAR', '--entrypoint', 'evaluate');
%start expr
%skip /[ \t]+/
expr: expr '/' term { aurocks_free($1); $$ = $3; }
    | term { $$ = $1; }
    ;
term: /[0-9]+/ { $$ = $1; }
    | /[/]/ { $$ = $1; }
    | /[]/]/ { $$ = $1; }
    | /[[:alpha:]/]/ { $$ = $1; }
    ;
%%
int main(void) {
    char *value = (char *)evaluate("2 / 3");
    if (!value || aurocks_strcmp(value, "3") != 0 || evaluate_error()) return 1;
    aurocks_free(value);
    if (evaluate("!") || !evaluate_error()) return 2;
    value = (char *)parse_expr("/");
    if (!value || aurocks_strcmp(value, "/") != 0 || parse_expr_error()) return 3;
    aurocks_free(value);
    if (yyparse(NULL) || evaluate_error()) return 4;
    value = (char *)evaluate("]");
    if (!value || aurocks_strcmp(value, "]") != 0) return 5;
    aurocks_free(value);
    value = (char *)evaluate("q");
    if (!value || aurocks_strcmp(value, "q") != 0) return 6;
    aurocks_free(value);
    return 0;
}
GRAMMAR
is($status, 0, 'generates parser with direct left recursion and slash character class') or diag($err);
ok(compile_and_run($c), 'generated C compiles and parses, and error state resets') if $status == 0;

for my $case (
    ["%start a\na: b ;\nb: a | 'x' ;\n", qr/indirect left recursion/],
    ["%start a\na: %empty b | 'x' ;\nb: a ;\n", qr/indirect left recursion/],
    ["%start a\na: prefix a | 'x' ;\nprefix: %empty | 'y' ;\n", qr/nullable-prefix left recursion/],
    ["%start a\na: a 'x' ;\n", qr/no non-left-recursive seed/],
    ["%start a\na: missing ;\n", qr/unknown symbol/],
    ["%start a\na: 'x' /[a/ ;\n", qr/bad regular expression/],
    ["%start a\na: 'x' /[a ;\n", qr/unterminated regular expression/],
) {
    my ($exit, $output, $message) = generate($case->[0]);
    isnt($exit, 0, 'invalid grammar is rejected');
    like($message, $case->[1], 'useful diagnostic');
    is($output, '', 'invalid grammar produces no partial C');
}

($status, $c, $err) = generate("%start a\na: 'x' ;\n", '--entrypoint', 'yyparse');
isnt($status, 0, 'reserved entrypoint rejected');
is($c, '', 'reserved entrypoint produces no C');

done_testing();
