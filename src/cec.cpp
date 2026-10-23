#include "tbc/cec.hpp"
#include "tbc/netlist_to_bdd.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <iomanip>
#include <algorithm>
#include <regex>

namespace tbc {

namespace {

struct BusMatch {
    std::string prefix;
    int index = 0;
};

std::optional<BusMatch> parse_bus_bit(const std::string& name) {
    auto lbrack = name.find('[');
    auto rbrack = name.find(']');
    if (lbrack != std::string::npos && rbrack != std::string::npos && rbrack > lbrack + 1) {
        std::string prefix = name.substr(0, lbrack);
        try {
            int idx = std::stoi(name.substr(lbrack + 1, rbrack - lbrack - 1));
            return BusMatch{prefix, idx};
        } catch (...) {}
    }
    return std::nullopt;
}

std::vector<std::string> compute_variable_order(const Netlist& a, const Netlist& b, const CecOptions& opts) {
    if (opts.order_mode == OrderMode::Explicit && !opts.order_file.empty()) {
        std::ifstream in(opts.order_file);
        if (!in.is_open()) {
            throw std::runtime_error("Could not open variable order file: " + opts.order_file);
        }
        std::vector<std::string> order;
        std::string line;
        while (std::getline(in, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (!line.empty() && line[0] != '#') {
                order.push_back(line);
            }
        }
        return order;
    }

    // Collect all unique input bit names
    std::vector<std::string> all_inputs;
    std::unordered_set<std::string> seen;
    for (const auto& p : a.inputs) {
        if (!seen.contains(p.bit_name)) {
            seen.insert(p.bit_name);
            all_inputs.push_back(p.bit_name);
        }
    }
    for (const auto& p : b.inputs) {
        if (!seen.contains(p.bit_name)) {
            seen.insert(p.bit_name);
            all_inputs.push_back(p.bit_name);
        }
    }

    if (opts.order_mode == OrderMode::Decl) {
        return all_inputs;
    }

    // OrderMode::Interleave
    // Separate into scalar inputs and bus inputs
    std::vector<std::string> scalars;
    std::map<int, std::vector<std::string>> bit_groups; // bit_index -> list of bus bits across all buses
    std::vector<std::string> bus_names;
    std::unordered_set<std::string> seen_bus_names;

    for (const auto& in_name : all_inputs) {
        auto bm = parse_bus_bit(in_name);
        if (bm) {
            bit_groups[bm->index].push_back(in_name);
            if (!seen_bus_names.contains(bm->prefix)) {
                seen_bus_names.insert(bm->prefix);
                bus_names.push_back(bm->prefix);
            }
        } else {
            scalars.push_back(in_name);
        }
    }

    std::vector<std::string> order;
    // Scalars first (e.g. cin, clk, rst)
    for (const auto& s : scalars) {
        order.push_back(s);
    }

    // Sort bus names alphabetically for deterministic interleaving: a[0], b[0], a[1], b[1]...
    std::sort(bus_names.begin(), bus_names.end());

    // Interleave bit by bit: bit 0 of each bus, bit 1 of each bus, etc.
    for (const auto& [idx, _] : bit_groups) {
        for (const auto& prefix : bus_names) {
            std::string bit_name = prefix + "[" + std::to_string(idx) + "]";
            if (seen.contains(bit_name)) {
                order.push_back(bit_name);
            }
        }
    }

    // Any remaining
    std::unordered_set<std::string> in_order(order.begin(), order.end());
    for (const auto& in_name : all_inputs) {
        if (!in_order.contains(in_name)) {
            order.push_back(in_name);
        }
    }

    return order;
}

} // namespace

CecResult run_cec(const Netlist& a, const Netlist& b, const CecOptions& opts) {
    auto start_time = std::chrono::steady_clock::now();

    CecResult result;
    result.var_order = compute_variable_order(a, b, opts);

    ManagerConfig mcfg;
    mcfg.max_nodes = opts.max_nodes;
    mcfg.cache_log2_size = opts.cache_log2_size;
    Manager mgr(mcfg);

    std::unordered_map<std::string, VarID> var_map;
    for (const auto& var_name : result.var_order) {
        VarID vid = mgr.new_var(var_name);
        var_map[var_name] = vid;
    }

    // Compile both netlists into BDD roots within the same shared Manager
    std::vector<NodeID> roots_a = netlist_to_bdd(mgr, a, var_map);
    std::vector<NodeID> roots_b = netlist_to_bdd(mgr, b, var_map);

    // Map output port bit names to roots
    std::unordered_map<std::string, NodeID> a_out_map;
    for (std::size_t i = 0; i < a.outputs.size(); ++i) {
        a_out_map[a.outputs[i].bit_name] = roots_a[i];
    }

    std::unordered_map<std::string, NodeID> b_out_map;
    for (std::size_t i = 0; i < b.outputs.size(); ++i) {
        b_out_map[b.outputs[i].bit_name] = roots_b[i];
    }

    // Check all outputs
    std::unordered_set<std::string> all_out_names;
    for (const auto& p : a.outputs) all_out_names.insert(p.bit_name);
    for (const auto& p : b.outputs) all_out_names.insert(p.bit_name);

    // Sort output names naturally (bus bit index order)
    std::vector<std::string> sorted_out_names(all_out_names.begin(), all_out_names.end());
    std::sort(sorted_out_names.begin(), sorted_out_names.end(), [](const std::string& x, const std::string& y) {
        auto bx = parse_bus_bit(x);
        auto by = parse_bus_bit(y);
        if (bx && by && bx->prefix == by->prefix) {
            return bx->index < by->index;
        }
        return x < y;
    });

    result.equivalent = true;

    for (const auto& out_name : sorted_out_names) {
        OutputComparison cmp;
        cmp.bit_name = out_name;

        auto it_a = a_out_map.find(out_name);
        auto it_b = b_out_map.find(out_name);

        if (it_a == a_out_map.end() || it_b == b_out_map.end()) {
            cmp.equivalent = false;
            result.equivalent = false;
            result.outputs.push_back(std::move(cmp));
            continue;
        }

        cmp.root_a = it_a->second;
        cmp.root_b = it_b->second;

        // O(1) canonicity check: exact pointer / index match!
        if (cmp.root_a == cmp.root_b) {
            cmp.equivalent = true;
        } else {
            cmp.equivalent = false;
            result.equivalent = false;

            // Compute difference BDD: XOR
            cmp.xor_diff = mgr.bxor(cmp.root_a, cmp.root_b);
            cmp.failing_vector_count = mgr.sat_count(cmp.xor_diff);

            // Extract satisfying counterexample
            auto sat_assign = mgr.any_sat(cmp.xor_diff);
            if (sat_assign) {
                std::unordered_map<VarID, bool> partial;
                for (const auto& [vid, val] : *sat_assign) {
                    partial[vid] = val;
                }

                // Construct full input vector for both circuits
                std::vector<std::pair<std::string, bool>> full_cex;
                for (std::size_t v = 0; v < result.var_order.size(); ++v) {
                    VarID vid = static_cast<VarID>(v);
                    bool val = partial.contains(vid) ? partial[vid] : false;
                    full_cex.emplace_back(result.var_order[v], val);
                }
                cmp.counterexample = std::move(full_cex);
            }
        }
        result.outputs.push_back(std::move(cmp));
    }

    result.bdd_stats = mgr.stats();
    result.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_time);

    return result;
}

std::string format_counterexample(const OutputComparison& cmp, const Netlist& a, const Netlist& b) {
    if (!cmp.counterexample) return "No counterexample available.";

    std::unordered_map<std::string, bool> val_map;
    for (const auto& [name, val] : *cmp.counterexample) {
        val_map[name] = val;
    }

    // Run independent netlist simulation to verify counterexample
    std::vector<bool> a_in(a.inputs.size(), false);
    for (std::size_t i = 0; i < a.inputs.size(); ++i) {
        auto it = val_map.find(a.inputs[i].bit_name);
        if (it != val_map.end()) a_in[i] = it->second;
    }
    std::vector<bool> b_in(b.inputs.size(), false);
    for (std::size_t i = 0; i < b.inputs.size(); ++i) {
        auto it = val_map.find(b.inputs[i].bit_name);
        if (it != val_map.end()) b_in[i] = it->second;
    }

    auto a_sim_out = a.simulate(a_in);
    auto b_sim_out = b.simulate(b_in);

    // Group input assignments by bus
    std::map<std::string, std::map<int, bool>> buses;
    std::vector<std::pair<std::string, bool>> scalars;

    for (const auto& [name, val] : *cmp.counterexample) {
        auto bm = parse_bus_bit(name);
        if (bm) {
            buses[bm->prefix][bm->index] = val;
        } else {
            scalars.emplace_back(name, val);
        }
    }

    std::ostringstream ss;
    ss << "Counterexample inputs for mismatch at " << cmp.bit_name << ":\n  ";

    for (const auto& [bus_name, bits] : buses) {
        unsigned long long hex_val = 0;
        int max_idx = 0;
        for (const auto& [idx, val] : bits) {
            if (val) hex_val |= (1ULL << idx);
            if (idx > max_idx) max_idx = idx;
        }
        int hex_digits = (max_idx + 4) / 4;
        ss << bus_name << " = 0x" << std::hex << std::setfill('0') << std::setw(hex_digits) << hex_val << std::dec << "  ";
    }
    for (const auto& [sname, sval] : scalars) {
        ss << sname << " = " << (sval ? 1 : 0) << "  ";
    }
    ss << "\n";

    auto get_sim_out_str = [](const Netlist& nl, const std::vector<bool>& sim_out, const std::string& bit_name) -> std::string {
        for (std::size_t i = 0; i < nl.outputs.size(); ++i) {
            if (nl.outputs[i].bit_name == bit_name) {
                return sim_out[i] ? "1" : "0";
            }
        }
        return "N/A";
    };

    ss << "  [Sim A] " << cmp.bit_name << " = " << get_sim_out_str(a, a_sim_out, cmp.bit_name)
       << "  vs  [Sim B] " << cmp.bit_name << " = " << get_sim_out_str(b, b_sim_out, cmp.bit_name);

    return ss.str();
}

} // namespace tbc
