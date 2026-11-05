module metaml_backends.QBE;
import MetaML;

class QBE {
    RunReport eval(Session session, string source, string file = "<qbe>", RunMode mode = RunMode.Eval) {
        auto saved = session.interp().engine;
        scope(exit) session.interp().engine = saved;
        session.interp().engine = ExecutionEngine.QBE;
        return session.run_source(source, file, mode);
    }
}
