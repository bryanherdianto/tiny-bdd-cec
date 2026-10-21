#include <catch2/catch_test_macros.hpp>
#include "tbc/bdd.hpp"

TEST_CASE("Computed Cache Memoization & Stress", "[cache]") {
    SECTION("Cache hits on repeated operations") {
        tbc::Manager mgr;
        auto x = mgr.var(mgr.new_var("x"));
        auto y = mgr.var(mgr.new_var("y"));
        auto z = mgr.var(mgr.new_var("z"));

        mgr.reset_transient_stats();
        auto f1 = mgr.ite(x, y, z);
        auto hits_before = mgr.stats().cache_hits;

        // Same operation should hit the computed cache
        auto f2 = mgr.ite(x, y, z);
        REQUIRE(f1 == f2);
        REQUIRE(mgr.stats().cache_hits > hits_before);
    }

    SECTION("Correctness under heavy cache thrashing (tiny cache log2=2)") {
        // Cache has only 4 entries! Heavy collision & eviction will occur.
        tbc::ManagerConfig cfg;
        cfg.cache_log2_size = 2; // 4 slots
        tbc::Manager mgr(cfg);

        std::vector<tbc::NodeID> vars;
        for (int i = 0; i < 8; ++i) {
            vars.push_back(mgr.var(mgr.new_var("v" + std::to_string(i))));
        }

        // Build complex nested functions
        auto acc = vars[0];
        for (std::size_t i = 1; i < vars.size(); ++i) {
            acc = mgr.bxor(acc, vars[i]);
        }

        // Reconstruct from scratch using different grouping: must still be equal!
        auto acc2 = mgr.bxor(
            mgr.bxor(mgr.bxor(vars[0], vars[1]), mgr.bxor(vars[2], vars[3])),
            mgr.bxor(mgr.bxor(vars[4], vars[5]), mgr.bxor(vars[6], vars[7]))
        );

        REQUIRE(acc == acc2);
    }
}
