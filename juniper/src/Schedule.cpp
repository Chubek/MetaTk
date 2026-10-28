#include "Internal.hpp"
#include <algorithm>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>

namespace juniper {
namespace {
Result<MachineFunction> schedule_function(const MachineFunction& input, const Backend& backend) {
    const auto n = input.instructions.size();
    if (n > 200000) return detail::error("JUN-E400", "machine instruction budget exceeded", input.name);
    MachineFunction out = input;
    // A reschedule starts from original IDs, not the previous issue order.
    std::sort(out.instructions.begin(), out.instructions.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    auto& instructions = out.instructions;
    std::vector<std::vector<std::size_t>> successors(n);
    std::unordered_map<std::string, std::size_t> writer;
    std::unordered_map<std::string, std::set<std::size_t>> readers;
    std::vector<std::size_t> region;
    std::optional<std::size_t> fence;
    for (std::size_t i = 0; i < n; ++i) {
        auto& inst = instructions[i];
        if (inst.id != i || !detail::identifier(inst.opcode) || inst.latency == 0 || inst.latency > 1000000 ||
            !backend.resources.contains(inst.resource) || inst.operands.size() > 200000 || inst.result.find('\0') != std::string::npos)
            return detail::error("JUN-E400", "invalid ID, opcode, latency, resource or operands", input.name);
        if (!inst.barrier && (inst.opcode == "CALL" || inst.opcode == "RET" || inst.opcode == "ASSUME" ||
            inst.opcode == "LABEL" || inst.opcode == "BR" || inst.opcode == "JUMP"))
            return detail::error("JUN-E400", "structural effect/control instruction must be a barrier", input.name);
        std::set<std::size_t> deps;
        std::set<std::string> uses;
        for (const auto& a : inst.operands) {
            if ((a.kind != Operand::Kind::reg && a.kind != Operand::Kind::immediate &&
                 a.kind != Operand::Kind::label && a.kind != Operand::Kind::symbol) ||
                (a.kind != Operand::Kind::immediate && (a.name.empty() || a.name.find('\0') != std::string::npos)))
                return detail::error("JUN-E400", "invalid machine operand", input.name);
            if (a.kind == Operand::Kind::reg) uses.insert(a.name);
        }
        for (const auto& name : uses) if (auto w = writer.find(name); w != writer.end()) deps.insert(w->second); // RAW
        if (!inst.result.empty()) {
            if (auto w = writer.find(inst.result); w != writer.end()) deps.insert(w->second); // WAW
            if (auto r = readers.find(inst.result); r != readers.end()) deps.insert(r->second.begin(), r->second.end()); // WAR
            readers[inst.result].clear(); writer[inst.result] = i;
        }
        for (const auto& name : uses) readers[name].insert(i);
        if (fence) deps.insert(*fence);
        if (inst.barrier) {
            deps.insert(region.begin(), region.end()); region.clear(); fence = i;
        } else region.push_back(i);
        deps.erase(i);
        inst.dependencies.assign(deps.begin(), deps.end()); inst.cycle.reset();
        for (auto d : deps) successors[d].push_back(i);
    }
    std::vector<std::uint64_t> priority(n), earliest(n);
    std::vector<std::size_t> remaining(n);
    for (std::size_t i = n; i-- > 0;) {
        std::uint64_t tail = 0;
        for (auto s : successors[i]) tail = std::max(tail, priority[s]);
        priority[i] = instructions[i].latency + tail;
        remaining[i] = instructions[i].dependencies.size();
    }
    // Future-ready instructions are ordered by availability; ready instructions
    // are ordered by critical path, then original ID. No per-cycle full scans.
    using Entry = std::pair<std::uint64_t, std::size_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> future;
    auto compare = [&](std::size_t a, std::size_t b) {
        return priority[a] != priority[b] ? priority[a] < priority[b] : a > b;
    };
    std::priority_queue<std::size_t, std::vector<std::size_t>, decltype(compare)> ready(compare);
    for (std::size_t i = 0; i < n; ++i) if (remaining[i] == 0) future.push({0, i});
    std::vector<Instruction> ordered; ordered.reserve(n);
    std::uint64_t cycle = 0;
    while (ordered.size() < n) {
        if (ready.empty() && !future.empty()) cycle = std::max(cycle, future.top().first);
        while (!future.empty() && future.top().first <= cycle) { ready.push(future.top().second); future.pop(); }
        if (ready.empty()) return detail::error("JUN-E401", "cyclic machine dependencies", input.name);
        std::map<std::string, std::uint32_t, std::less<>> issued;
        std::vector<std::size_t> deferred;
        std::uint32_t slots = 0;
        while (!ready.empty() && slots < backend.issue_width) {
            auto i = ready.top(); ready.pop();
            const auto& resource = instructions[i].resource;
            if (issued[resource] == backend.resources.at(resource)) { deferred.push_back(i); continue; }
            ++issued[resource]; ++slots;
            instructions[i].cycle = cycle;
            ordered.push_back(instructions[i]);
            for (auto s : successors[i]) {
                earliest[s] = std::max(earliest[s], cycle + instructions[i].latency);
                if (--remaining[s] == 0) future.push({earliest[s], s});
            }
        }
        for (auto i : deferred) ready.push(i);
        ++cycle;
    }
    out.instructions = std::move(ordered);
    return out;
}
} // namespace
Result<MachineModule> schedule(const MachineModule& input) {
    auto valid = detail::validate_backend(input.backend); if (!valid) return *valid.error();
    if (input.functions.size() > 200000) return detail::error("JUN-E400", "function budget exceeded");
    MachineModule out; out.name = input.name; out.backend = input.backend; out.scheduled = true;
    for (const auto& f : input.functions) {
        auto scheduled = schedule_function(f, input.backend);
        if (!scheduled) return *scheduled.error();
        out.functions.push_back(std::move(*scheduled.get()));
    }
    return out;
}
std::string emit_machine_json(const MachineModule& m) {
    using detail::quote;
    std::ostringstream out;
    out << "{\"schema\":1,\"module\":" << quote(m.name) << ",\"backend\":" << quote(m.backend.name)
        << ",\"scheduled\":" << (m.scheduled ? "true" : "false") << ",\"issue_width\":" << m.backend.issue_width
        << ",\"resources\":{";
    bool comma = false;
    for (const auto& [name, n] : m.backend.resources) { if (comma) out << ','; comma = true; out << quote(name) << ':' << n; }
    out << "},\"functions\":[";
    for (std::size_t f = 0; f < m.functions.size(); ++f) {
        if (f) out << ',';
        const auto& fn = m.functions[f];
        out << "\n{\"name\":" << quote(fn.name) << ",\"params\":[";
        for (std::size_t p = 0; p < fn.params.size(); ++p) { if (p) out << ','; out << quote(fn.params[p]); }
        out << "],\"instructions\":[";
        for (std::size_t i = 0; i < fn.instructions.size(); ++i) {
            if (i) out << ',';
            const auto& inst = fn.instructions[i];
            out << "\n{\"id\":" << inst.id << ",\"statement\":" << inst.statement << ",\"opcode\":" << quote(inst.opcode)
                << ",\"result\":" << quote(inst.result) << ",\"operands\":[";
            for (std::size_t a = 0; a < inst.operands.size(); ++a) {
                if (a) out << ',';
                const auto& arg = inst.operands[a];
                const char* kind = arg.kind == Operand::Kind::reg ? "reg" : arg.kind == Operand::Kind::immediate ? "imm" :
                    arg.kind == Operand::Kind::label ? "label" : "symbol";
                out << "{\"kind\":" << quote(kind) << ",\"value\":";
                if (arg.kind == Operand::Kind::immediate) out << arg.value; else out << quote(arg.name);
                out << '}';
            }
            out << "],\"latency\":" << inst.latency << ",\"resource\":" << quote(inst.resource)
                << ",\"barrier\":" << (inst.barrier ? "true" : "false") << ",\"dependencies\":[";
            for (std::size_t d = 0; d < inst.dependencies.size(); ++d) { if (d) out << ','; out << inst.dependencies[d]; }
            out << "],\"cycle\":";
            if (inst.cycle) out << *inst.cycle; else out << "null";
            out << '}';
        }
        out << "]}";
    }
    return out.str() + "]}\n";
}
} // namespace juniper
