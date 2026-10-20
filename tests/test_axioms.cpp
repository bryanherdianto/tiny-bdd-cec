#include <catch2/catch_test_macros.hpp>
#include "tbc/bdd.hpp"

TEST_CASE("Boolean Algebra Axioms", "[axioms]") {
    tbc::Manager mgr;
    auto x_id = mgr.new_var("x");
    auto y_id = mgr.new_var("y");
    auto z_id = mgr.new_var("z");

    auto x = mgr.var(x_id);
    auto y = mgr.var(y_id);
    auto z = mgr.var(z_id);

    SECTION("Idempotence") {
        REQUIRE(mgr.band(x, x) == x);
        REQUIRE(mgr.bor(x, x) == x);
    }

    SECTION("Commutativity") {
        REQUIRE(mgr.band(x, y) == mgr.band(y, x));
        REQUIRE(mgr.bor(x, y) == mgr.bor(y, x));
        REQUIRE(mgr.bxor(x, y) == mgr.bxor(y, x));
    }

    SECTION("Associativity") {
        auto xy_z = mgr.band(mgr.band(x, y), z);
        auto x_yz = mgr.band(x, mgr.band(y, z));
        REQUIRE(xy_z == x_yz);

        auto oxy_z = mgr.bor(mgr.bor(x, y), z);
        auto ox_yz = mgr.bor(x, mgr.bor(y, z));
        REQUIRE(oxy_z == ox_yz);

        auto xxy_z = mgr.bxor(mgr.bxor(x, y), z);
        auto xx_yz = mgr.bxor(x, mgr.bxor(y, z));
        REQUIRE(xxy_z == xx_yz);
    }

    SECTION("Absorption") {
        // x & (x | y) == x
        REQUIRE(mgr.band(x, mgr.bor(x, y)) == x);
        // x | (x & y) == x
        REQUIRE(mgr.bor(x, mgr.band(x, y)) == x);
    }

    SECTION("Distributivity") {
        // x & (y | z) == (x & y) | (x & z)
        auto lhs = mgr.band(x, mgr.bor(y, z));
        auto rhs = mgr.bor(mgr.band(x, y), mgr.band(x, z));
        REQUIRE(lhs == rhs);

        // x | (y & z) == (x | y) & (x | z)
        auto lhs2 = mgr.bor(x, mgr.band(y, z));
        auto rhs2 = mgr.band(mgr.bor(x, y), mgr.bor(x, z));
        REQUIRE(lhs2 == rhs2);
    }

    SECTION("De Morgan's Laws") {
        // ~(x & y) == ~x | ~y
        auto not_and = mgr.bnot(mgr.band(x, y));
        auto or_not  = mgr.bor(mgr.bnot(x), mgr.bnot(y));
        REQUIRE(not_and == or_not);

        // ~(x | y) == ~x & ~y
        auto not_or  = mgr.bnot(mgr.bor(x, y));
        auto and_not = mgr.band(mgr.bnot(x), mgr.bnot(y));
        REQUIRE(not_or == and_not);
    }

    SECTION("Double Negation and Inverses") {
        REQUIRE(mgr.bnot(mgr.bnot(x)) == x);
        REQUIRE(mgr.band(x, mgr.bnot(x)) == tbc::kFalse);
        REQUIRE(mgr.bor(x, mgr.bnot(x)) == tbc::kTrue);
        REQUIRE(mgr.bxor(x, x) == tbc::kFalse);
        REQUIRE(mgr.bxor(x, tbc::kFalse) == x);
        REQUIRE(mgr.bxor(x, tbc::kTrue) == mgr.bnot(x));
    }
}
