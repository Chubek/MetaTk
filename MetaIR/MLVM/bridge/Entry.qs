use pipeline, control_flow, compute, optimization, serialize, dataflow, metair;

fn bridge_parse(text: str): list<list<int>> { return pipeline::compile(text, 0); }
fn bridge_import(bytes: str): list<list<int>> { return metair::import_mcil(bytes); }
fn bridge_export(code: list<list<int>>): str { return metair::export_mcil(code); }
fn bridge_optimize(code: list<list<int>>, mode): list<list<int>> {
    if (mode == 2) return optimization::propagate(code);
    if (mode == 1) return optimization::fold(code);
    if (mode != 0) die("MLVM bridge: invalid optimization mode");
    control_flow::verify(code);
    return code;
}
fn bridge_run(code: list<list<int>>, steps) { return compute::run(code, steps); }
fn bridge_analyze(code: list<list<int>>): str { return dataflow::report(code); }
fn bridge_text(code: list<list<int>>): str { return serialize::text(code); }
