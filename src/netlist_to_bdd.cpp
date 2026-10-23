#include "tbc/netlist_to_bdd.hpp"
#include <stdexcept>

namespace tbc {

std::vector<NodeID> netlist_to_bdd(Manager& mgr,
                                   const Netlist& netlist,
                                   const std::unordered_map<std::string, VarID>& input_var_map) {
    std::vector<NodeID> wire_node(netlist.gates.size(), kFalse);

    // Map primary inputs
    for (const auto& in_port : netlist.inputs) {
        auto it = input_var_map.find(in_port.bit_name);
        if (it == input_var_map.end()) {
            throw std::runtime_error("Input variable not mapped in BDD manager: " + in_port.bit_name);
        }
        wire_node[in_port.sig] = mgr.var(it->second);
    }

    // Evaluate gates topologically
    for (std::size_t i = 0; i < netlist.gates.size(); ++i) {
        const auto& g = netlist.gates[i];
        switch (g.kind) {
            case GateKind::Const0:
                wire_node[i] = kFalse;
                break;
            case GateKind::Const1:
                wire_node[i] = kTrue;
                break;
            case GateKind::Input:
                // Already initialized above
                break;
            case GateKind::Buf:
                wire_node[i] = g.fanins.empty() ? kFalse : wire_node[g.fanins[0]];
                break;
            case GateKind::Not:
                wire_node[i] = g.fanins.empty() ? kTrue : mgr.bnot(wire_node[g.fanins[0]]);
                break;
            case GateKind::And: {
                NodeID acc = kTrue;
                for (SignalID in : g.fanins) {
                    acc = mgr.band(acc, wire_node[in]);
                }
                wire_node[i] = acc;
                break;
            }
            case GateKind::Or: {
                NodeID acc = kFalse;
                for (SignalID in : g.fanins) {
                    acc = mgr.bor(acc, wire_node[in]);
                }
                wire_node[i] = acc;
                break;
            }
            case GateKind::Xor: {
                NodeID acc = kFalse;
                for (SignalID in : g.fanins) {
                    acc = mgr.bxor(acc, wire_node[in]);
                }
                wire_node[i] = acc;
                break;
            }
            case GateKind::Nand: {
                NodeID acc = kTrue;
                for (SignalID in : g.fanins) {
                    acc = mgr.band(acc, wire_node[in]);
                }
                wire_node[i] = mgr.bnot(acc);
                break;
            }
            case GateKind::Nor: {
                NodeID acc = kFalse;
                for (SignalID in : g.fanins) {
                    acc = mgr.bor(acc, wire_node[in]);
                }
                wire_node[i] = mgr.bnot(acc);
                break;
            }
            case GateKind::Xnor: {
                NodeID acc = kFalse;
                for (SignalID in : g.fanins) {
                    acc = mgr.bxor(acc, wire_node[in]);
                }
                wire_node[i] = mgr.bnot(acc);
                break;
            }
            case GateKind::Mux: {
                // fanins: [sel, in0, in1] -> ite(sel, in1, in0)
                NodeID sel = wire_node[g.fanins[0]];
                NodeID in0 = wire_node[g.fanins[1]];
                NodeID in1 = wire_node[g.fanins[2]];
                wire_node[i] = mgr.ite(sel, in1, in0);
                break;
            }
            case GateKind::Cover: {
                NodeID or_acc = kFalse;
                for (const auto& cube : g.sop_cubes) {
                    NodeID and_acc = kTrue;
                    for (std::size_t f = 0; f < g.fanins.size() && f < cube.size(); ++f) {
                        char c = cube[f];
                        if (c == '1') {
                            and_acc = mgr.band(and_acc, wire_node[g.fanins[f]]);
                        } else if (c == '0') {
                            and_acc = mgr.band(and_acc, mgr.bnot(wire_node[g.fanins[f]]));
                        }
                    }
                    or_acc = mgr.bor(or_acc, and_acc);
                }
                wire_node[i] = or_acc;
                break;
            }
        }
    }

    std::vector<NodeID> roots;
    roots.reserve(netlist.outputs.size());
    for (const auto& out : netlist.outputs) {
        roots.push_back(wire_node[out.sig]);
    }
    return roots;
}

} // namespace tbc
