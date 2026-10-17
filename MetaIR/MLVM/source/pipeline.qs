use parser, control_flow, optimization, compute;

fn compile(text: str, optimize): list<list<int>> {
    let code = parser::parse(text);
    control_flow::verify(code);
    if (optimize == 2) return optimization::propagate(code);
    if (optimize) return optimization::fold(code);
    return code;
}
fn run(text: str, max_steps, optimize) {
    return compute::run(compile(text, optimize), max_steps);
}
