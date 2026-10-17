// A calculator written as a grammar, evaluated by walking its match tree.
grammar Arith {
    rule TOP { <sum> }
    rule sum { <product>+ % <add-op> }
    rule product { <atom>+ % <mul-op> }
    token atom { <num> | '(' ~ ')' <sum> }
    token num { \d+ }
    token add-op { '+' | '-' }
    token mul-op { '*' }
}

fn value(m: match): int {
    if (m.rule() == "TOP") { return value(m.child("sum")); }
    if (m.rule() == "num") { return m.text().to_int(); }
    if (m.rule() == "atom") {
        if (m.has("sum")) { return value(m.child("sum")); }
        return value(m.child("num"));
    }
    if (m.rule() == "product") {
        let total = 1;
        for (my a in m.children("atom")) { total *= value(a); }
        return total;
    }
    if (m.rule() == "sum") {
        let total = 0;
        let sign = 1;
        let i = 0;
        while (i < m.size()) {
            let part = m.at(i);
            if (part.rule() == "add-op") {
                sign = part.text() == "-" ? -1 : 1;
            } else {
                total += sign * value(part);
            }
            i++;
        }
        return total;
    }
    return 0;
}

fn evaluate(text: str): int {
    let m = Arith.parse(text);
    assert m.ok();
    return value(m);
}

print evaluate("2*3+4");
print evaluate("2*(3+4)");
print evaluate("10-4");
print evaluate("((7))");
print Arith.rules().len();
return evaluate("6*7");
