#include <catch2/catch_test_macros.hpp>
#include "tbc/bdd.hpp"

TEST_CASE("ROBDD Canonicity Properties", "[canonicity]") {
    tbc::Manager mgr;
    auto a_id = mgr.new_var("a");
    auto b_id = mgr.new_var("b");
    auto c_id = mgr.new_var("c");

    auto a = mgr.var(a_id);
    auto b = mgr.var(b_id);
    auto c = mgr.var(c_id);

    SECTION("Majority function built in two different ways") {
        // M1 = (a & b) | (b & c) | (a & c)
        auto m1 = mgr.bor(mgr.bor(mgr.band(a, b), mgr.band(b, c)), mgr.band(a, c));

        // M2 = (a & b) | (c & (a ^ b))
        auto m2 = mgr.bor(mgr.band(a, b), mgr.band(c, mgr.bxor(a, b)));

        // Under fixed variable order, both must have the exact same NodeID
        REQUIRE(m1 == m2);
    }

    SECTION("Parity function canonical node count") {
        // 4-variable XOR: a ^ b ^ c ^ d
        auto d_id = mgr.new_var("d");
        auto d = mgr.var(d_id);

        auto p1 = mgr.bxor(mgr.bxor(mgr.bxor(a, b), c), d);
        auto p2 = mgr.bxor(mgr.bxor(a, c), mgr.bxor(b, d));
        REQUIRE(p1 == p2);

        // For n-variable parity, total internal nodes is 2n - 1
        // For n=4, 2*4 - 1 = 7 internal nodes + 2 terminals = 9 nodes in DAG
        std::vector<tbc::NodeID> roots = {p1};
        std::size_t total_dag_nodes = mgr.dag_size(roots);
        REQUIRE(total_dag_nodes == 9);
    }
}
