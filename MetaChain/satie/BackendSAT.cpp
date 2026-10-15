#include "BackendSAT.hpp"
// The bundled Satie DSLtk predates MetaChain's sibling DSLtk. Give it a
// private namespace, avoiding cross-TU ODR collisions as well as redefinitions.
#define dsl metachain_satie_dsl
#include "include/SatieCDCL.hpp"
#undef dsl

namespace metachain::sat {
Model solve(const Formula& f) {
    if (f.variables<0||f.variables>200000||f.clauses.size()>1000000)
        throw std::length_error("SAT formula size limit exceeded");
    satie::CNF cnf;
    for (const auto& clause:f.clauses) {
        for (auto lit:clause) if (!lit||lit>f.variables||lit< -f.variables) throw std::invalid_argument("invalid SAT literal");
        cnf.add_clause(clause);
    }
    satie::CDCLSolver solver(std::move(cnf));
    solver.set_conflict_budget(100000);
    auto solved=solver.solve();
    if (solved.status==satie::SolveStatus::UNKNOWN) throw std::length_error("SAT conflict budget exceeded");
    Model m; m.satisfiable=solved.satisfiable();
    m.values.resize(static_cast<std::size_t>(f.variables)+1);
    if (m.satisfiable) for (int n=1;n<=f.variables;++n) m.values[n]=solved.assignment.get_var(n)==satie::Value::TRUE;
    return m;
}
} // namespace metachain::sat
