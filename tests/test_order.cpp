#include <catch2/catch_test_macros.hpp>
#include "tbc/cec.hpp"
#include "tbc/verilog.hpp"

TEST_CASE("Variable Ordering Impact on BDD Size", "[order]") {
    std::string spec = R"(
        module adder8(input [7:0] a, b, input cin, output [7:0] sum, output cout);
            assign {cout, sum} = a + b + cin;
        endmodule
    )";

    auto nl_a = tbc::parse_verilog_string(spec);
    auto nl_b = tbc::parse_verilog_string(spec);

    tbc::CecOptions opt_interleave;
    opt_interleave.order_mode = tbc::OrderMode::Interleave;
    auto res_interleave = tbc::run_cec(nl_a, nl_b, opt_interleave);

    tbc::CecOptions opt_decl;
    opt_decl.order_mode = tbc::OrderMode::Decl;
    auto res_decl = tbc::run_cec(nl_a, nl_b, opt_decl);

    REQUIRE(res_interleave.equivalent);
    REQUIRE(res_decl.equivalent);

    // Interleaved variable order produces far fewer nodes than declaration order
    // for adder carry chains
    REQUIRE(res_interleave.bdd_stats.nodes_allocated < res_decl.bdd_stats.nodes_allocated);
}
