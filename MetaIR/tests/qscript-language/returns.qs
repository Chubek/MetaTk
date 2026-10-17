// Every path of a non-integer function ends in a return, a loop that never
// exits, or an exit. Each function below is accepted by the return-path check.
fn pick(c: int): str { if (c) { return "yes"; } else { return "no"; } }
fn forever(): str { loop { return "loop"; } }
fn skipping(): str { loop { if (0) { continue; } return "skip"; } }
fn nested(): str { loop { loop { break; } return "nested"; } }
fn modifier(c: int): str { return "m" if c; return "z"; }
fn blocked(): str { { return "block"; } }
fn once(): str { do { return "once"; } while (0); }
fn spin(n: int): str { while (1) { n = n + 1; if (n > 3) { return "spun"; } } }
print pick(1), pick(0), forever(), skipping(), nested(), modifier(1), modifier(0), blocked(), once(), spin(0);
