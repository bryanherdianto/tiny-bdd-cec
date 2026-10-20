#include "tbc/bdd.hpp"
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace tbc {

namespace {

inline std::uint64_t splitmix64(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

inline std::size_t combine3(std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept {
    std::uint64_t v = (static_cast<std::uint64_t>(a) << 32) | static_cast<std::uint64_t>(b);
    std::uint64_t h = splitmix64(v) ^ splitmix64(static_cast<std::uint64_t>(c) + 0x517cc1b727220a95ULL);
    return static_cast<std::size_t>(splitmix64(h));
}

} // namespace

std::size_t Manager::NodeKeyHash::operator()(const NodeKey& k) const noexcept {
    return combine3(k.var, k.low, k.high);
}

struct Manager::UniqueTableImpl {
    std::unordered_map<NodeKey, NodeID, NodeKeyHash> table;
};

void Manager::init_tables() {
    unique_ = new UniqueTableImpl();

    // Terminals at indices 0 and 1
    // kFalse = 0
    nodes_.push_back(BDDNode{kTerminalVar, kFalse, kFalse});
    // kTrue = 1
    nodes_.push_back(BDDNode{kTerminalVar, kTrue, kTrue});

    stats_.nodes_allocated = 2;

    std::size_t cache_size = (1ULL << config_.cache_log2_size);
    cache_.assign(cache_size, CacheEntry{});
    cache_mask_ = cache_size - 1;
}

void Manager::cleanup_tables() {
    delete unique_;
    unique_ = nullptr;
}

Manager::Manager(ManagerConfig cfg) : config_(cfg) {
    init_tables();
}

Manager::~Manager() {
    cleanup_tables();
}

Manager::Manager(Manager&& o) noexcept
    : config_(o.config_),
      stats_(o.stats_),
      nodes_(std::move(o.nodes_)),
      var_names_(std::move(o.var_names_)),
      cache_(std::move(o.cache_)),
      cache_mask_(o.cache_mask_),
      unique_(o.unique_) {
    o.unique_ = nullptr;
}

Manager& Manager::operator=(Manager&& o) noexcept {
    if (this != &o) {
        cleanup_tables();
        config_ = o.config_;
        stats_ = o.stats_;
        nodes_ = std::move(o.nodes_);
        var_names_ = std::move(o.var_names_);
        cache_ = std::move(o.cache_);
        cache_mask_ = o.cache_mask_;
        unique_ = o.unique_;
        o.unique_ = nullptr;
    }
    return *this;
}

VarID Manager::new_var(std::string name) {
    VarID id = static_cast<VarID>(var_names_.size());
    var_names_.push_back(std::move(name));
    return id;
}

NodeID Manager::var(VarID v) {
    if (v >= var_names_.size()) {
        throw std::out_of_range("VarID out of range: " + std::to_string(v));
    }
    // Projection function x_v: when x_v=0 -> 0 (low), when x_v=1 -> 1 (high)
    return mk(v, kFalse, kTrue);
}

std::string_view Manager::var_name(VarID v) const {
    if (v >= var_names_.size()) {
        throw std::out_of_range("VarID out of range: " + std::to_string(v));
    }
    return var_names_[v];
}

std::optional<VarID> Manager::find_var_by_name(std::string_view name) const {
    for (std::size_t i = 0; i < var_names_.size(); ++i) {
        if (var_names_[i] == name) return static_cast<VarID>(i);
    }
    return std::nullopt;
}

NodeID Manager::mk(VarID v, NodeID low, NodeID high) {
    // Rule 1: Redundant node elimination
    if (low == high) {
        return low;
    }

    NodeKey key{v, low, high};
    auto it = unique_->table.find(key);
    if (it != unique_->table.end()) {
        ++stats_.unique_hits;
        return it->second;
    }

    // Rule 2: Isomorphism elimination (new canonical node)
    ++stats_.unique_misses;
    if (nodes_.size() >= config_.max_nodes) {
        throw NodeLimitExceeded("BDD Manager exceeded max_nodes limit: " + std::to_string(config_.max_nodes));
    }

    NodeID new_id = static_cast<NodeID>(nodes_.size());
    nodes_.push_back(BDDNode{v, low, high});
    unique_->table.emplace(key, new_id);

    ++stats_.nodes_allocated;
    if (unique_->table.size() > stats_.unique_peak) {
        stats_.unique_peak = unique_->table.size();
    }

    return new_id;
}

NodeID Manager::bnot(NodeID f) {
    return ite(f, kFalse, kTrue);
}

NodeID Manager::ite(NodeID f, NodeID g, NodeID h) {
    ++stats_.ite_calls;

    // Terminal Cases
    if (f == kTrue)  return g;
    if (f == kFalse) return h;
    if (g == h)      return g;
    if (g == kTrue && h == kFalse) return f;

    if (f == g) g = kTrue;  // ite(f, f, h) == ite(f, 1, h)
    if (f == h) h = kFalse; // ite(f, g, f) == ite(f, g, 0)

    // Canonical representation for symmetric operations to maximize cache hit rate
    // e.g. for OR: ite(f, 1, g) == ite(g, 1, f)
    if (g == kTrue && f > h) {
        std::swap(f, h);
    }
    // for AND: ite(f, g, 0) == ite(g, f, 0)
    if (h == kFalse && f > g) {
        std::swap(f, g);
    }

    // Computed Cache Lookup
    ++stats_.cache_lookups;
    std::size_t slot = combine3(f, g, h) & cache_mask_;
    const auto& entry = cache_[slot];
    if (entry.f == f && entry.g == g && entry.h == h && entry.res != 0) {
        ++stats_.cache_hits;
        return entry.res;
    }

    // Determine top variable: x = min(var(f), var(g), var(h))
    VarID vf = (f <= 1) ? kTerminalVar : nodes_[f].var_id;
    VarID vg = (g <= 1) ? kTerminalVar : nodes_[g].var_id;
    VarID vh = (h <= 1) ? kTerminalVar : nodes_[h].var_id;
    VarID top_var = std::min({vf, vg, vh});

    // Shannon expansion cofactors
    NodeID f0 = (vf == top_var) ? nodes_[f].low  : f;
    NodeID f1 = (vf == top_var) ? nodes_[f].high : f;

    NodeID g0 = (vg == top_var) ? nodes_[g].low  : g;
    NodeID g1 = (vg == top_var) ? nodes_[g].high : g;

    NodeID h0 = (vh == top_var) ? nodes_[h].low  : h;
    NodeID h1 = (vh == top_var) ? nodes_[h].high : h;

    // Recursive subproblems
    NodeID res0 = ite(f0, g0, h0);
    NodeID res1 = ite(f1, g1, h1);

    NodeID result = mk(top_var, res0, res1);

    // Cache update (lossy direct-mapped)
    cache_[slot] = CacheEntry{f, g, h, result};

    return result;
}

NodeID Manager::apply(Op op, NodeID f, NodeID g) {
    switch (op) {
        case Op::And:     return ite(f, g, kFalse);
        case Op::Or:      return ite(f, kTrue, g);
        case Op::Xor:     return ite(f, bnot(g), g);
        case Op::Xnor:    return ite(f, g, bnot(g));
        case Op::Nand:    return bnot(ite(f, g, kFalse));
        case Op::Nor:     return bnot(ite(f, kTrue, g));
        case Op::Implies: return ite(f, g, kTrue);
    }
    return kFalse;
}

NodeID Manager::cofactor(NodeID f, VarID v, bool value) {
    if (f <= 1) return f;
    const auto& n = nodes_[f];
    if (n.var_id > v) {
        // Since variables are strictly ordered, variable v does not appear in f
        return f;
    }
    if (n.var_id == v) {
        return value ? n.high : n.low;
    }
    // n.var_id < v
    NodeID lo = cofactor(n.low, v, value);
    NodeID hi = cofactor(n.high, v, value);
    return mk(n.var_id, lo, hi);
}

NodeID Manager::exists(NodeID f, VarID v) {
    return bor(cofactor(f, v, false), cofactor(f, v, true));
}

std::optional<std::vector<std::pair<VarID, bool>>> Manager::any_sat(NodeID f) const {
    if (f == kFalse) return std::nullopt;
    std::vector<std::pair<VarID, bool>> assignment;
    NodeID curr = f;
    while (curr > 1) {
        const auto& n = nodes_[curr];
        if (n.low != kFalse) {
            // Take low edge (var = 0)
            assignment.emplace_back(n.var_id, false);
            curr = n.low;
        } else {
            // Take high edge (var = 1)
            assignment.emplace_back(n.var_id, true);
            curr = n.high;
        }
    }
    return assignment;
}

long double Manager::sat_count(NodeID f) const {
    if (f == kFalse) return 0.0;
    if (num_vars() == 0) return (f == kTrue) ? 1.0 : 0.0;

    std::vector<long double> memo(nodes_.size(), -1.0);
    memo[kFalse] = 0.0;
    memo[kTrue]  = 1.0;

    auto total_vars = static_cast<VarID>(num_vars());

    std::function<long double(NodeID)> count_rec = [&](NodeID u) -> long double {
        if (memo[u] >= 0.0) return memo[u];
        const auto& n = nodes_[u];

        VarID var_lo = (n.low <= 1) ? total_vars : nodes_[n.low].var_id;
        VarID var_hi = (n.high <= 1) ? total_vars : nodes_[n.high].var_id;

        long double skip_lo = static_cast<long double>(var_lo - n.var_id - 1);
        long double skip_hi = static_cast<long double>(var_hi - n.var_id - 1);

        long double cnt_lo = count_rec(n.low) * std::pow(2.0L, skip_lo);
        long double cnt_hi = count_rec(n.high) * std::pow(2.0L, skip_hi);

        memo[u] = cnt_lo + cnt_hi;
        return memo[u];
    };

    long double root_cnt = count_rec(f);
    if (f > 1) {
        long double skip_root = static_cast<long double>(nodes_[f].var_id);
        root_cnt *= std::pow(2.0L, skip_root);
    } else if (f == kTrue) {
        root_cnt = std::pow(2.0L, static_cast<long double>(total_vars));
    }

    return root_cnt;
}

bool Manager::eval(NodeID f, const std::vector<bool>& assignment) const {
    NodeID curr = f;
    while (curr > 1) {
        const auto& n = nodes_[curr];
        if (n.var_id >= assignment.size()) {
            throw std::out_of_range("Assignment span size (" + std::to_string(assignment.size()) +
                                    ") smaller than variable index (" + std::to_string(n.var_id) + ")");
        }
        curr = assignment[n.var_id] ? n.high : n.low;
    }
    return curr == kTrue;
}

std::size_t Manager::dag_size(std::span<const NodeID> roots) const {
    std::size_t count = 0;
    for_each_reachable(roots, [&](NodeID, const BDDNode&) {
        ++count;
    });
    return count;
}

} // namespace tbc
