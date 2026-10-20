#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <optional>
#include <functional>
#include <stdexcept>
#include <limits>
#include <utility>

namespace tbc {

using NodeID = std::uint32_t;
using VarID  = std::uint32_t;

inline constexpr NodeID kFalse = 0;
inline constexpr NodeID kTrue  = 1;
inline constexpr VarID  kTerminalVar = std::numeric_limits<VarID>::max();

struct BDDNode {
    VarID  var_id;
    NodeID low;
    NodeID high;
};

enum class Op : std::uint8_t {
    And,
    Or,
    Xor,
    Xnor,
    Nand,
    Nor,
    Implies
};

class NodeLimitExceeded : public std::runtime_error {
public:
    explicit NodeLimitExceeded(const std::string& msg) : std::runtime_error(msg) {}
};

struct ManagerConfig {
    std::size_t cache_log2_size = 18;       // 262,144 entries by default
    std::size_t max_nodes       = 50000000; // 50M nodes safety cap
};

struct Stats {
    std::size_t nodes_allocated = 0;
    std::size_t unique_peak     = 0;
    std::size_t unique_hits     = 0;
    std::size_t unique_misses   = 0;
    std::size_t cache_lookups   = 0;
    std::size_t cache_hits      = 0;
    std::size_t ite_calls       = 0;

    [[nodiscard]] double cache_hit_rate() const noexcept {
        return cache_lookups == 0 ? 0.0 : (static_cast<double>(cache_hits) * 100.0 / static_cast<double>(cache_lookups));
    }
};

class Manager {
public:
    explicit Manager(ManagerConfig cfg = {});
    ~Manager();

    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) noexcept;
    Manager& operator=(Manager&&) noexcept;

    // Variable management
    VarID new_var(std::string name);
    [[nodiscard]] NodeID var(VarID v);
    [[nodiscard]] std::size_t num_vars() const noexcept { return var_names_.size(); }
    [[nodiscard]] std::string_view var_name(VarID v) const;
    [[nodiscard]] std::optional<VarID> find_var_by_name(std::string_view name) const;

    // Constant terminals
    [[nodiscard]] static constexpr NodeID bfalse() noexcept { return kFalse; }
    [[nodiscard]] static constexpr NodeID btrue() noexcept { return kTrue; }
    [[nodiscard]] static constexpr bool is_terminal(NodeID id) noexcept { return id <= 1; }

    // Core reduction constructor: enforces Rule 1 (redundancy) & Rule 2 (isomorphism)
    NodeID mk(VarID v, NodeID low, NodeID high);

    // Boolean operations
    NodeID ite(NodeID f, NodeID g, NodeID h);
    NodeID apply(Op op, NodeID f, NodeID g);

    [[nodiscard]] NodeID bnot(NodeID f);
    [[nodiscard]] NodeID band(NodeID f, NodeID g) { return apply(Op::And, f, g); }
    [[nodiscard]] NodeID bor(NodeID f, NodeID g)  { return apply(Op::Or, f, g); }
    [[nodiscard]] NodeID bxor(NodeID f, NodeID g) { return apply(Op::Xor, f, g); }

    // Decomposition & Quantifiers
    [[nodiscard]] NodeID cofactor(NodeID f, VarID v, bool value);
    [[nodiscard]] NodeID exists(NodeID f, VarID v);

    // Analysis & Verification
    [[nodiscard]] std::optional<std::vector<std::pair<VarID, bool>>> any_sat(NodeID f) const;
    [[nodiscard]] long double sat_count(NodeID f) const; // Over all declared vars
    [[nodiscard]] bool eval(NodeID f, const std::vector<bool>& assignment) const;
    [[nodiscard]] std::size_t dag_size(std::span<const NodeID> roots) const;

    template <typename Fn>
    void for_each_reachable(std::span<const NodeID> roots, Fn&& fn) const;

    [[nodiscard]] const BDDNode& node(NodeID id) const {
        return nodes_.at(id);
    }

    [[nodiscard]] const Stats& stats() const noexcept { return stats_; }
    void reset_transient_stats() noexcept {
        stats_.cache_lookups = 0;
        stats_.cache_hits = 0;
        stats_.ite_calls = 0;
    }

private:
    struct Impl;
    ManagerConfig config_;
    Stats stats_;

    std::vector<BDDNode> nodes_;
    std::vector<std::string> var_names_;

    struct CacheEntry {
        NodeID f = 0;
        NodeID g = 0;
        NodeID h = 0;
        NodeID res = 0;
    };
    std::vector<CacheEntry> cache_;
    std::size_t cache_mask_ = 0;

    struct NodeKey {
        VarID  var;
        NodeID low;
        NodeID high;
        bool operator==(const NodeKey& o) const noexcept {
            return var == o.var && low == o.low && high == o.high;
        }
    };
    struct NodeKeyHash {
        std::size_t operator()(const NodeKey& k) const noexcept;
    };

    // Forward declare unique table container
    struct UniqueTableImpl;
    UniqueTableImpl* unique_ = nullptr;

    void init_tables();
    void cleanup_tables();
};

template <typename Fn>
void Manager::for_each_reachable(std::span<const NodeID> roots, Fn&& fn) const {
    if (nodes_.empty()) return;
    std::vector<uint8_t> visited(nodes_.size(), 0);
    std::vector<NodeID> stack;
    stack.reserve(64);

    for (NodeID r : roots) {
        if (r < nodes_.size() && !visited[r]) {
            visited[r] = 1;
            stack.push_back(r);
        }
    }

    while (!stack.empty()) {
        NodeID curr = stack.back();
        stack.pop_back();

        fn(curr, nodes_[curr]);

        if (curr > 1) { // Not terminal
            NodeID lo = nodes_[curr].low;
            NodeID hi = nodes_[curr].high;
            if (lo < nodes_.size() && !visited[lo]) {
                visited[lo] = 1;
                stack.push_back(lo);
            }
            if (hi < nodes_.size() && !visited[hi]) {
                visited[hi] = 1;
                stack.push_back(hi);
            }
        }
    }
}

} // namespace tbc
