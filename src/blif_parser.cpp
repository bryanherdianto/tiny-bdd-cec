#include "tbc/blif.hpp"
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

namespace tbc {

namespace {

struct BlifCover {
    std::string out_wire;
    std::vector<std::string> in_wires;
    std::vector<std::string> cubes;
    bool is_offset = false; // true if cube ends in '0'
};

struct BlifLatch {
    std::string input;
    std::string output;
};

class BlifElaborator {
public:
    BlifElaborator(const BlifOptions& opts) : opts_(opts) {}

    Netlist parse(std::string_view source);

private:
    BlifOptions opts_;
    Netlist nl_;

    std::unordered_map<std::string, SignalID> wire_signals_;
    std::unordered_map<std::string, std::size_t> wire_to_driver_;
    std::unordered_map<std::string, std::string> wire_alias_;
    std::vector<BlifCover> covers_;
    std::unordered_set<std::string> evaluating_wires_;

    SignalID resolve_wire(const std::string& name);
};

SignalID BlifElaborator::resolve_wire(const std::string& name) {
    auto it = wire_signals_.find(name);
    if (it != wire_signals_.end()) {
        return it->second;
    }

    auto al_it = wire_alias_.find(name);
    if (al_it != wire_alias_.end()) {
        if (evaluating_wires_.contains(name)) {
            throw BlifParseError("Combinational loop detected in wire alias: " + name);
        }
        evaluating_wires_.insert(name);
        SignalID s = resolve_wire(al_it->second);
        evaluating_wires_.erase(name);
        wire_signals_[name] = s;
        return s;
    }

    if (evaluating_wires_.contains(name)) {
        throw BlifParseError("Combinational loop detected involving wire: " + name);
    }
    evaluating_wires_.insert(name);

    auto d_it = wire_to_driver_.find(name);
    if (d_it == wire_to_driver_.end()) {
        // Unconnected wire defaults to 0
        SignalID zero = nl_.add_const(false);
        wire_signals_[name] = zero;
        evaluating_wires_.erase(name);
        return zero;
    }

    const auto& cov = covers_[d_it->second];

    // Case 1: Constant
    if (cov.in_wires.empty()) {
        bool val = false;
        if (!cov.cubes.empty() && cov.cubes[0] == "1") val = true;
        SignalID sig = nl_.add_const(val);
        wire_signals_[name] = sig;
        evaluating_wires_.erase(name);
        return sig;
    }

    // Resolve fanin signals
    std::vector<SignalID> fanins;
    for (const auto& in_name : cov.in_wires) {
        fanins.push_back(resolve_wire(in_name));
    }

    // Case 2: Buffer / Inverter
    if (cov.in_wires.size() == 1 && cov.cubes.size() == 1) {
        if (cov.cubes[0] == "1") {
            SignalID s = nl_.add_gate(cov.is_offset ? GateKind::Not : GateKind::Buf, fanins, name);
            wire_signals_[name] = s;
            evaluating_wires_.erase(name);
            return s;
        } else if (cov.cubes[0] == "0") {
            SignalID s = nl_.add_gate(cov.is_offset ? GateKind::Buf : GateKind::Not, fanins, name);
            wire_signals_[name] = s;
            evaluating_wires_.erase(name);
            return s;
        }
    }

    // Case 3: Standard single AND gate
    if (cov.cubes.size() == 1 && !cov.is_offset) {
        const auto& c = cov.cubes[0];
        bool all_ones = true;
        for (char ch : c) if (ch != '1') { all_ones = false; break; }
        if (all_ones && c.size() == fanins.size()) {
            SignalID s = nl_.add_gate(GateKind::And, fanins, name);
            wire_signals_[name] = s;
            evaluating_wires_.erase(name);
            return s;
        }
    }

    // Case 4: General Sum-Of-Products Cover
    SignalID cov_sig = nl_.add_gate(GateKind::Cover, fanins, name);
    nl_.gates.back().sop_cubes = cov.cubes;

    if (cov.is_offset) {
        cov_sig = nl_.add_gate(GateKind::Not, {cov_sig}, name + "_inv");
    }

    wire_signals_[name] = cov_sig;
    evaluating_wires_.erase(name);
    return cov_sig;
}

Netlist BlifElaborator::parse(std::string_view source) {
    std::vector<std::string> lines;
    std::string line;
    std::istringstream stream((std::string(source)));

    // Handle line continuation with backslash character
    std::string accumulator;
    while (std::getline(stream, line)) {
        // Strip comment
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim right
        auto end_pos = line.find_last_not_of(" \t\r\n");
        if (end_pos == std::string::npos) {
            if (!accumulator.empty()) {
                lines.push_back(std::move(accumulator));
                accumulator.clear();
            }
            continue;
        }
        line = line.substr(0, end_pos + 1);

        if (line.back() == '\\') {
            line.pop_back();
            accumulator += line + " ";
        } else {
            accumulator += line;
            lines.push_back(std::move(accumulator));
            accumulator.clear();
        }
    }
    if (!accumulator.empty()) {
        lines.push_back(std::move(accumulator));
    }

    std::vector<std::string> declared_inputs;
    std::vector<std::string> declared_outputs;
    std::vector<BlifLatch> latches;

    BlifCover current_cover;
    bool in_names = false;

    for (const auto& l : lines) {
        std::istringstream ls(l);
        std::string token;
        if (!(ls >> token)) continue;

        if (token == ".model") {
            ls >> nl_.module_name;
        } else if (token == ".inputs") {
            if (in_names) {
                wire_to_driver_[current_cover.out_wire] = covers_.size();
                covers_.push_back(std::move(current_cover));
                in_names = false;
            }
            std::string in_wire;
            while (ls >> in_wire) {
                declared_inputs.push_back(in_wire);
            }
        } else if (token == ".outputs") {
            if (in_names) {
                wire_to_driver_[current_cover.out_wire] = covers_.size();
                covers_.push_back(std::move(current_cover));
                in_names = false;
            }
            std::string out_wire;
            while (ls >> out_wire) {
                declared_outputs.push_back(out_wire);
            }
        } else if (token == ".conn") {
            if (in_names) {
                wire_to_driver_[current_cover.out_wire] = covers_.size();
                covers_.push_back(std::move(current_cover));
                in_names = false;
            }
            std::string src_w, dst_w;
            if (ls >> src_w >> dst_w) {
                wire_alias_[dst_w] = src_w;
            }
        } else if (token == ".latch") {
            if (in_names) {
                wire_to_driver_[current_cover.out_wire] = covers_.size();
                covers_.push_back(std::move(current_cover));
                in_names = false;
            }
            std::string lin, lout;
            if (ls >> lin >> lout) {
                latches.push_back(BlifLatch{lin, lout});
            }
        } else if (token == ".subckt") {
            if (in_names) {
                wire_to_driver_[current_cover.out_wire] = covers_.size();
                covers_.push_back(std::move(current_cover));
                in_names = false;
            }
            std::string cell_type;
            ls >> cell_type;
            std::unordered_map<std::string, std::string> conns;
            std::string pair;
            while (ls >> pair) {
                auto eq_pos = pair.find('=');
                if (eq_pos != std::string::npos) {
                    conns[pair.substr(0, eq_pos)] = pair.substr(eq_pos + 1);
                }
            }

            if (conns.contains("D") && conns.contains("Q")) {
                std::string d_net = conns["D"];
                // Handle synchronous reset if present
                if (conns.contains("R") && cell_type.find("SDFF") != std::string::npos) {
                    auto pos = cell_type.find("SDFF_");
                    if (pos != std::string::npos && pos + 7 < cell_type.size()) {
                        char rst_pol = cell_type[pos + 6];
                        char rst_val = cell_type[pos + 7];
                        std::string r_net = conns["R"];
                        std::string eff_d = conns["Q"] + "$eff_d";
                        BlifCover c;
                        c.out_wire = eff_d;
                        c.in_wires = {r_net, d_net};
                        if (rst_pol == 'P' && rst_val == '0') {
                            // rst=1 forces 0: ~R & D
                            c.cubes = {"01"};
                        } else if (rst_pol == 'N' && rst_val == '0') {
                            // rst=0 forces 0: R & D
                            c.cubes = {"11"};
                        } else if (rst_pol == 'P' && rst_val == '1') {
                            // rst=1 forces 1: R | D
                            c.cubes = {"1-", "-1"};
                        } else if (rst_pol == 'N' && rst_val == '1') {
                            // rst=0 forces 1: ~R | D
                            c.cubes = {"0-", "-1"};
                        }
                        wire_to_driver_[eff_d] = covers_.size();
                        covers_.push_back(std::move(c));
                        d_net = eff_d;
                    }
                }
                latches.push_back(BlifLatch{d_net, conns["Q"]});
            } else if (cell_type == "$_AND_" || cell_type == "$_OR_" || cell_type == "$_XOR_") {
                if (conns.contains("A") && conns.contains("B") && conns.contains("Y")) {
                    BlifCover c;
                    c.out_wire = conns["Y"];
                    c.in_wires = {conns["A"], conns["B"]};
                    if (cell_type == "$_AND_") c.cubes = {"11"};
                    else if (cell_type == "$_OR_") c.cubes = {"1-", "-1"};
                    else if (cell_type == "$_XOR_") c.cubes = {"10", "01"};
                    wire_to_driver_[c.out_wire] = covers_.size();
                    covers_.push_back(std::move(c));
                }
            } else if (cell_type == "$_NOT_") {
                if (conns.contains("A") && conns.contains("Y")) {
                    BlifCover c;
                    c.out_wire = conns["Y"];
                    c.in_wires = {conns["A"]};
                    c.cubes = {"0"};
                    wire_to_driver_[c.out_wire] = covers_.size();
                    covers_.push_back(std::move(c));
                }
            } else if (cell_type == "$_MUX_") {
                if (conns.contains("A") && conns.contains("B") && conns.contains("S") && conns.contains("Y")) {
                    BlifCover c;
                    c.out_wire = conns["Y"];
                    c.in_wires = {conns["S"], conns["A"], conns["B"]};
                    c.cubes = {"01-", "1-1"};
                    wire_to_driver_[c.out_wire] = covers_.size();
                    covers_.push_back(std::move(c));
                }
            }
        } else if (token == ".names") {
            if (in_names) {
                wire_to_driver_[current_cover.out_wire] = covers_.size();
                covers_.push_back(std::move(current_cover));
            }
            in_names = true;
            current_cover = BlifCover{};
            std::vector<std::string> wires;
            std::string w;
            while (ls >> w) {
                wires.push_back(w);
            }
            if (wires.empty()) {
                throw BlifParseError("Empty .names command");
            }
            current_cover.out_wire = wires.back();
            wires.pop_back();
            current_cover.in_wires = std::move(wires);
        } else if (token == ".end") {
            if (in_names) {
                wire_to_driver_[current_cover.out_wire] = covers_.size();
                covers_.push_back(std::move(current_cover));
                in_names = false;
            }
            break;
        } else if (in_names) {
            // Truth table line for current .names
            // format: <in_cube> <out_val> or just <out_val>
            std::string out_val;
            if (ls >> out_val) {
                current_cover.cubes.push_back(token);
                current_cover.is_offset = (out_val == "0");
            } else {
                current_cover.cubes.push_back(token);
                current_cover.is_offset = (token == "0");
            }
        }
    }

    if (in_names) {
        wire_to_driver_[current_cover.out_wire] = covers_.size();
        covers_.push_back(std::move(current_cover));
    }

    // Register primary inputs
    for (const auto& in_name : declared_inputs) {
        SignalID s = nl_.add_input(in_name);
        wire_signals_[in_name] = s;
    }

    // Register cutting and output alias for latches / DFFs
    std::unordered_set<std::string> out_set(declared_outputs.begin(), declared_outputs.end());
    if (opts_.cut_latches) {
        for (const auto& lat : latches) {
            if (out_set.contains(lat.output)) {
                // The flip-flop Q drives a primary output: connect the output directly to D
                wire_alias_[lat.output] = lat.input;
            } else {
                // Internal state register: Q becomes pseudo-input, D becomes pseudo-output
                SignalID q_sig = nl_.add_input(lat.output);
                wire_signals_[lat.output] = q_sig;
                declared_outputs.push_back(lat.input);
            }
        }
    }

    // Resolve primary outputs in topological order
    for (const auto& out_name : declared_outputs) {
        SignalID s = resolve_wire(out_name);
        nl_.add_output(out_name, s);
    }

    return nl_;
}

} // namespace

Netlist parse_blif_string(const std::string& source, const BlifOptions& opts) {
    BlifElaborator elab(opts);
    return elab.parse(source);
}

Netlist parse_blif_file(const std::string& filepath, const BlifOptions& opts) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open BLIF file: " + filepath);
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return parse_blif_string(ss.str(), opts);
}

} // namespace tbc
