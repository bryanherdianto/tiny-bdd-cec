#pragma once

#include "tbc/bdd.hpp"
#include "tbc/netlist.hpp"
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace tbc {

enum class OrderMode {
    Interleave, // Interleave bus bits: cin, a[0], b[0], a[1], b[1], ...
    Decl,       // Declaration order: a[0], ..., a[7], b[0], ..., b[7], cin
    Explicit    // From custom variable order file
};

struct CecOptions {
    OrderMode order_mode = OrderMode::Interleave;
    std::string order_file;
    std::size_t max_nodes = 50000000;
    std::size_t cache_log2_size = 18;
    bool quiet = false;
};

struct OutputComparison {
    std::string bit_name;
    bool equivalent = false;
    long double failing_vector_count = 0.0;
    NodeID root_a = kFalse;
    NodeID root_b = kFalse;
    NodeID xor_diff = kFalse;
    // Counterexample input vector (if not equivalent)
    std::optional<std::vector<std::pair<std::string, bool>>> counterexample;
};

struct CecResult {
    bool equivalent = true;
    std::vector<OutputComparison> outputs;
    std::vector<std::string> var_order;
    Stats bdd_stats;
    std::chrono::microseconds elapsed{};

    [[nodiscard]] std::size_t mismatch_count() const noexcept {
        std::size_t c = 0;
        for (const auto& o : outputs) if (!o.equivalent) ++c;
        return c;
    }
};

CecResult run_cec(const Netlist& a, const Netlist& b, const CecOptions& opts = {});

// Helper to format counterexample with bus grouping and hex display
std::string format_counterexample(const OutputComparison& cmp, const Netlist& a, const Netlist& b);

// Export shared BDD DAG to Graphviz DOT
void export_dot(const Manager& mgr,
                std::span<const NodeID> roots,
                std::span<const std::string> root_names,
                const std::string& filepath);

} // namespace tbc
