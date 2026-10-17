// First-class grammars: declarations, inheritance, runtime extension,
// dynamic compilation, values passed to functions, and match trees.
grammar Base {
    token num { \d+ }
    token ws { \s* }
}
grammar Calc is Base {
    rule TOP { <expr> }
    rule expr { <term>+ % '+' }
    token term { <num> | '(' ~ ')' <expr> }
}
grammar Words {
    token word { <[a..z]>+ }
    rule TOP { <word>+ % ',' }
}

fn words(g: grammar, text: str): list<str> {
    let found: list<str> = [];
    let m = g.parse(text);
    if (!m.ok()) { return found; }
    for (my w in m.find_all("word")) { found.push(w.text()); }
    return found;
}

let calc = Calc;
print calc.name();
print calc.has_rule("term");
print calc.rules().join(" ");
let m = calc.parse("1+2+(3+4)");
print m.ok();
print m.rule();
print m.text();
print m.find_all("num").len();
print m.children("expr").len();
print calc.parse("1+").ok();
print calc.parse("1+").error();
let loud = Words.extend("token word { <[A..Z]>+ }");
print words(Words, "one,two,three").join("|");
print words(loud, "ABC,DEF").len();
print words(Words, "ABC").len();
let dyn = compile_grammar("grammar Pair { rule TOP { <n> ',' <n> } token n { \\d+ } }");
print dyn.name();
print dyn.parse("3,4").text();
print dyn.parse("3;4").ok();
print Words.parse("hey,you", "word").ok();
print m.at(0).rule();
let total = 0;
for (my item in m.children("expr")) { total += item.text().len(); }
print total;
return m.find_all("num").len();
