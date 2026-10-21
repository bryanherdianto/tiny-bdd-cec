#include "tbc/netlist.hpp"

namespace tbc {

std::vector<bool> Netlist::simulate(const std::vector<bool>& input_values) const {
    if (input_values.size() != inputs.size()) {
        throw std::invalid_argument("Input vector size (" + std::to_string(input_values.size()) +
                                    ") does not match netlist inputs count (" + std::to_string(inputs.size()) + ")");
    }

    std::vector<bool> vals(gates.size(), false);

    // Set primary input values
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        vals[inputs[i].sig] = input_values[i];
    }

    // Evaluate gates in topological order (gates are indexed sequentially)
    for (std::size_t i = 0; i < gates.size(); ++i) {
        const auto& g = gates[i];
        switch (g.kind) {
            case GateKind::Const0:
                vals[i] = false;
                break;
            case GateKind::Const1:
                vals[i] = true;
                break;
            case GateKind::Input:
                // Already initialized
                break;
            case GateKind::Buf:
                vals[i] = g.fanins.empty() ? false : vals[g.fanins[0]];
                break;
            case GateKind::Not:
                vals[i] = g.fanins.empty() ? true : !vals[g.fanins[0]];
                break;
            case GateKind::And: {
                bool acc = true;
                for (SignalID in : g.fanins) acc = acc && vals[in];
                vals[i] = acc;
                break;
            }
            case GateKind::Or: {
                bool acc = false;
                for (SignalID in : g.fanins) acc = acc || vals[in];
                vals[i] = acc;
                break;
            }
            case GateKind::Xor: {
                bool acc = false;
                for (SignalID in : g.fanins) acc = acc ^ vals[in];
                vals[i] = acc;
                break;
            }
            case GateKind::Nand: {
                bool acc = true;
                for (SignalID in : g.fanins) acc = acc && vals[in];
                vals[i] = !acc;
                break;
            }
            case GateKind::Nor: {
                bool acc = false;
                for (SignalID in : g.fanins) acc = acc || vals[in];
                vals[i] = !acc;
                break;
            }
            case GateKind::Xnor: {
                bool acc = false;
                for (SignalID in : g.fanins) acc = acc ^ vals[in];
                vals[i] = !acc;
                break;
            }
            case GateKind::Mux: {
                // fanins: [sel, in0, in1] -> sel ? in1 : in0
                bool sel = vals[g.fanins[0]];
                bool in0 = vals[g.fanins[1]];
                bool in1 = vals[g.fanins[2]];
                vals[i] = sel ? in1 : in0;
                break;
            }
            case GateKind::Cover: {
                // BLIF sum of products cover
                bool or_acc = false;
                for (const auto& cube : g.sop_cubes) {
                    bool and_acc = true;
                    for (std::size_t f = 0; f < g.fanins.size() && f < cube.size(); ++f) {
                        char c = cube[f];
                        if (c == '1') {
                            if (!vals[g.fanins[f]]) { and_acc = false; break; }
                        } else if (c == '0') {
                            if (vals[g.fanins[f]]) { and_acc = false; break; }
                        }
                    }
                    if (and_acc) { or_acc = true; break; }
                }
                vals[i] = or_acc;
                break;
            }
        }
    }

    std::vector<bool> out_vals;
    out_vals.reserve(outputs.size());
    for (const auto& out : outputs) {
        out_vals.push_back(vals[out.sig]);
    }
    return out_vals;
}

} // namespace tbc
