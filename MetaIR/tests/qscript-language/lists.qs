// List operations: literals, element access and update, growth, sorting and joining.
let xs = [5, 3, 9];
print xs.len();
print xs[1];
xs[1] = 30;
xs[2] += 1;
xs[0] *= 2;
print xs;
xs.push(7);
print xs.pop();
print xs.last();
print xs.contains(30);
xs.sort();
print xs;
xs.reverse();
print xs;
print xs.slice(1, 2);
let words: list<str> = ["pear", "apple", "fig"];
words.sort();
print words.join(" ");
words[0] ~= "!";
print words[0];
let grid = [[1, 2], [3, 4]];
print grid[1][0];
grid[0][1] = 20;
print grid;
let empty: list<int> = [];
print empty.len();
empty.clear();
print empty.len();
let total = 0;
for (my v in xs) { total += v; }
print total;
for (my w in words) { if (w == "fig") { break; } print w; }
let n = 0;
for (my z in [1, 2, 3, 4, 5]) { if (z % 2 == 0) { continue; } n += z; }
print n;
return total;
