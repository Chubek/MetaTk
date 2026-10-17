// FizzBuzz, Perl-flavoured: statement modifiers, `next`, `say`.
// Prints 15 for "FizzBuzz", 3 for "Fizz", 5 for "Buzz", else the number.
for (my i = 1; i <= 15; i++) {
    say 15 if i % 15 == 0;
    next if i % 15 == 0;
    say 3 if i % 3 == 0;
    say 5 if i % 5 == 0;
    say i unless i % 3 == 0 || i % 5 == 0;
}
