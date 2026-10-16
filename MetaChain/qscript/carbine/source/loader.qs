// Loading the Carbinefile. The host parses it (grammar/carbine.grammar),
// resolves dependency names and expands source globs. It prints the reason
// and traps when the file is missing or malformed, so a return here means
// the model is ready. The result is the number of targets.

fn load_spec() {
    return carbine::spec_load();
}
