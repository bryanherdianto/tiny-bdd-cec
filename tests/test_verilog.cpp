#include <catch2/catch_test_macros.hpp>
#include "tbc/verilog.hpp"

TEST_CASE("Verilog Parser & Elaborator", "[verilog]") {
    SECTION("Basic continuous assignment with gates") {
        std::string src = R"(
            module test_gates(input a, b, c, output y1, y2);
                assign y1 = a & b | ~c;
                assign y2 = a ^ b ^ c;
            endmodule
        )";
        auto nl = tbc::parse_verilog_string(src);
        REQUIRE(nl.module_name == "test_gates");
        REQUIRE(nl.inputs.size() == 3);
        REQUIRE(nl.outputs.size() == 2);

        // Simulate: a=1, b=1, c=1 -> y1 = 1&1 | 0 = 1, y2 = 1^1^1 = 1
        std::vector<bool> in = {true, true, true};
        auto out = nl.simulate(in);
        REQUIRE(out.size() == 2);
        REQUIRE(out[0] == true);
        REQUIRE(out[1] == true);
    }

    SECTION("Bus declarations and part selects") {
        std::string src = R"(
            module bus_test(input [3:0] in, output [1:0] out_low, output [1:0] out_high);
                assign out_low = in[1:0];
                assign out_high = in[3:2];
            endmodule
        )";
        auto nl = tbc::parse_verilog_string(src);
        REQUIRE(nl.inputs.size() == 4);
        REQUIRE(nl.outputs.size() == 4);

        // in = 4'b1011 (in[0]=1, in[1]=1, in[2]=0, in[3]=1)
        // out_low should be 2'b11, out_high should be 2'b10
        std::vector<bool> in = {true, true, false, true};
        auto out = nl.simulate(in);
        REQUIRE(out[0] == true);  // out_low[0]
        REQUIRE(out[1] == true);  // out_low[1]
        REQUIRE(out[2] == false); // out_high[2]
        REQUIRE(out[3] == true);  // out_high[3]
    }

    SECTION("Gate primitives") {
        std::string src = R"(
            module prim_test(input a, b, output o_and, o_xor);
                and g1 (o_and, a, b);
                xor g2 (o_xor, a, b);
            endmodule
        )";
        auto nl = tbc::parse_verilog_string(src);
        REQUIRE(nl.inputs.size() == 2);
        REQUIRE(nl.outputs.size() == 2);

        // a=1, b=0 -> o_and=0, o_xor=1
        auto out = nl.simulate({true, false});
        REQUIRE(out[0] == false);
        REQUIRE(out[1] == true);
    }

    SECTION("Ternary multiplexer") {
        std::string src = R"(
            module mux_test(input sel, input [3:0] a, b, output [3:0] y);
                assign y = sel ? b : a;
            endmodule
        )";
        auto nl = tbc::parse_verilog_string(src);
        // inputs: sel, a[0..3], b[0..3] -> 9 inputs
        REQUIRE(nl.inputs.size() == 9);
        REQUIRE(nl.outputs.size() == 4);
    }

    SECTION("Syntax error handling") {
        std::string bad_src = "module foo(input a; output b); endmodule";
        REQUIRE_THROWS_AS(tbc::parse_verilog_string(bad_src), tbc::VerilogParseError);
    }
}
