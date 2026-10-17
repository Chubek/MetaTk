grammar Words {
    token word { <[a..z]>+ }
    rule TOP { <word>+ % ',' }
}
fn words(g: grammar, text: str): list<str> {
    let m = g.parse(text);
    let out: list<str> = [];
    if (!m.ok()) { return out; }
    for (my w in m.children("word")) { out.push(w.text()); }
    return out;
}
let base = Words;
let loud = base.extend("token word { <[A..Z]>+ }");
print loud.parse("ABC,DEF").ok();
print base.parse("abc,def").ok();
print base.parse("ABC").ok();
print words(base, "one,two,three").join("|");
print words(loud, "ABC,DEF").len();
let dyn = compile_grammar("grammar Pair { rule TOP { <n>','<n> } token n { \\d+ } }");
print dyn.name();
print dyn.parse("3,4").text();
print dyn.parse("3;4").ok();
let rooted = Words.parse("hey,you", "word");
print rooted.ok();
print rooted.rule();
