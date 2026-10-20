#include <catch2/catch_test_macros.hpp>
#include "tbc/bdd.hpp"

TEST_CASE("ROBDD Reduction Rules", "[reduction]") {
    tbc::Manager mgr;
    auto x0 = mgr.new_var("x0");
    auto x1 = mgr.new_var("x1");

    SECTION("Pre-allocated terminal nodes") {
        REQUIRE(tbc::Manager::bfalse() == 0);
        REQUIRE(tbc::Manager::btrue() == 1);
        REQUIRE(tbc::Manager::is_terminal(0));
        REQUIRE(tbc::Manager::is_terminal(1));
        REQUIRE_FALSE(tbc::Manager::is_terminal(2));
    }

    SECTION("Rule 1: Redundant node elimination (low == high)") {
        // A node whose low and high edges point to the same child is eliminated
        auto lo = mgr.var(x1);
        auto hi = lo;
        auto redundant = mgr.mk(x0, lo, hi);
        REQUIRE(redundant == lo);
    }

    SECTION("Rule 2: Isomorphism elimination (unique table)") {
        // Constructing the same triple (var, low, high) must return the identical NodeID
        auto f1 = mgr.mk(x0, tbc::kFalse, tbc::kTrue);
        auto f2 = mgr.mk(x0, tbc::kFalse, tbc::kTrue);
        REQUIRE(f1 == f2);
        REQUIRE(mgr.stats().unique_hits >= 1);
    }

    SECTION("Projection function x_v") {
        auto vx0 = mgr.var(x0);
        REQUIRE(vx0 > 1);
        const auto& n = mgr.node(vx0);
        REQUIRE(n.var_id == x0);
        REQUIRE(n.low == tbc::kFalse);
        REQUIRE(n.high == tbc::kTrue);
    }
}
