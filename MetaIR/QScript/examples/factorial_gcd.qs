// Integer arithmetic examples: factorial and greatest common divisor.
let n = 5;
let factorial = 1;
while (n > 1) {
    factorial = factorial * n;
    n = n - 1;
}
print factorial;
let a = 48;
let b = 18;
while (b != 0) {
    let t = a % b;
    a = b;
    b = t;
}
print a;
return factorial;
