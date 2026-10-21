#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <unordered_map>
#include <optional>
#include <stdexcept>

namespace tbc {

enum class GateKind : std::uint8_t {
    Const0,
    Const1,
    Input,
    Buf,
    Not,
    And,
    Or,
    Xor,
    Nand,
    Nor,
    Xnor,
    Mux,    // fanins: [sel, in0, in1] -> sel ? in1 : in0
    Cover   // For BLIF sum-of-products cover (Phase 2 extension)
};

using SignalID = std::uint32_t;
inline constexpr SignalID kInvalidSignal = 0xFFFFFFFF;

struct Gate {
    GateKind kind;
    std::vector<SignalID> fanins;
    std::string name;
    // For Cover gates in BLIF (SOP table)
    std::vector<std::string> sop_cubes;
};

struct Port {
    std::string bit_name; // e.g. "a[0]", "cin", "cout"
    SignalID sig = kInvalidSignal;
};

struct BusInfo {
    std::string name;     // Base name, e.g. "a"
    int msb = 0;
    int lsb = 0;
    bool is_input = true;
};

class Netlist {
public:
    std::string module_name;
    std::vector<Gate> gates;      // gates[sig] represents signal sig
    std::vector<Port> inputs;     // Primary inputs in bit-order
    std::vector<Port> outputs;    // Primary outputs in bit-order
    std::vector<BusInfo> buses;   // Discovered or declared bus structures

    SignalID add_input(const std::string& bit_name) {
        SignalID sig = static_cast<SignalID>(gates.size());
        gates.push_back(Gate{GateKind::Input, {}, bit_name, {}});
        inputs.push_back(Port{bit_name, sig});
        return sig;
    }

    SignalID add_gate(GateKind kind, std::vector<SignalID> fanins, std::string name = "") {
        SignalID sig = static_cast<SignalID>(gates.size());
        gates.push_back(Gate{kind, std::move(fanins), std::move(name), {}});
        return sig;
    }

    SignalID add_const(bool val) {
        return add_gate(val ? GateKind::Const1 : GateKind::Const0, {}, val ? "1'b1" : "1'b0");
    }

    void add_output(const std::string& bit_name, SignalID sig) {
        outputs.push_back(Port{bit_name, sig});
    }

    [[nodiscard]] std::optional<SignalID> find_input(const std::string& bit_name) const {
        for (const auto& p : inputs) {
            if (p.bit_name == bit_name) return p.sig;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<SignalID> find_output(const std::string& bit_name) const {
        for (const auto& p : outputs) {
            if (p.bit_name == bit_name) return p.sig;
        }
        return std::nullopt;
    }

    // Direct independent simulation of the netlist against an input assignment
    [[nodiscard]] std::vector<bool> simulate(const std::vector<bool>& input_values) const;
};

} // namespace tbc
