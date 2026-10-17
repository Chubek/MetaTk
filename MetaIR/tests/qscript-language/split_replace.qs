// Splitting, replacing, repeating and slicing strings and lists.
print "a::b::::c".split("::");
print "abc".split("x");
print "".split(",");
print "aaa".replace("a", "bb");
print "abcabc".replace("bc", "");
print "ab".repeat(0).len();
print "ab".repeat(3);
print "hello".sub(5, 0).len();
print "hello".sub(1, 3);
let xs = [1, 2, 3];
print xs.slice(3, 0);
print xs.slice(1, 2);
let p = "k=v;x=y".split(";");
print p[1].split("=")[1];
return p.len();
