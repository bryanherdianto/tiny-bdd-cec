#include <catch2/catch_test_macros.hpp>
#include "tbc/bdd.hpp"

TEST_CASE("ITE, Cofactors, and Quantifiers", "[ite]") {
    tbc::Manager mgr;
    auto x0 = mgr.new_var("x0");
    auto x1 = mgr.new_var("x1");
    auto x2 = mgr.new_var("x2");

    auto vx0 = mgr.var(x0);
    auto vx1 = mgr.var(x1);
    auto vx2 = mgr.var(x2);

    SECTION("ITE terminal rules") {
        REQUIRE(mgr.ite(tbc::kTrue, vx0, vx1) == vx0);
        REQUIRE(mgr.ite(tbc::kFalse, vx0, vx1) == vx1);
        REQUIRE(mgr.ite(vx0, vx1, vx1) == vx1);
        REQUIRE(mgr.ite(vx0, tbc::kTrue, tbc::kFalse) == vx0);
        REQUIRE(mgr.ite(vx0, tbc::kFalse, tbc::kTrue) == mgr.bnot(vx0));
    }

    SECTION("Shannon expansion theorem") {
        // f = (x0 & x1) | (~x0 & x2)
        auto f = mgr.bor(mgr.band(vx0, vx1), mgr.band(mgr.bnot(vx0), vx2));

        auto f_x0_0 = mgr.cofactor(f, x0, false);
        auto f_x0_1 = mgr.cofactor(f, x0, true);

        REQUIRE(f_x0_0 == vx2);
        REQUIRE(f_x0_1 == vx1);

        // Shannon reconstruction: f == x0 * f_1 + ~x0 * f_0
        auto recon = mgr.ite(vx0, f_x0_1, f_x0_0);
        REQUIRE(recon == f);
    }

    SECTION("Existential Quantification") {
        // exists(x0, (x0 & x1) | (~x0 & x2)) == (x1 | x2)
        auto f = mgr.bor(mgr.band(vx0, vx1), mgr.band(mgr.bnot(vx0), vx2));
        auto ex = mgr.exists(f, x0);
        REQUIRE(ex == mgr.bor(vx1, vx2));
    }

    SECTION("SAT count and any_sat") {
        // f = x0 & x1. Out of 3 variables, exactly 2 assignments make it true:
        // (1, 1, 0) and (1, 1, 1)
        auto f = mgr.band(vx0, vx1);
        REQUIRE(mgr.sat_count(f) == 2.0L);

        auto cex = mgr.any_sat(f);
        REQUIRE(cex.has_value());
        // Verify assignment with eval
        std::vector<bool> assign(3, false);
        for (const auto& [vid, val] : *cex) {
            assign[vid] = val;
        }
        REQUIRE(mgr.eval(f, assign) == true);
    }
}
