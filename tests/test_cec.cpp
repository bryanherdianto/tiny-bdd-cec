#include <catch2/catch_test_macros.hpp>
#include "tbc/cec.hpp"
#include "tbc/verilog.hpp"

TEST_CASE("Combinational Equivalence Checking (CEC)", "[cec]") {
    SECTION("1-bit Full Adder Equivalence") {
        std::string spec = R"(
            module fa_spec(input a, b, cin, output sum, cout);
                assign {cout, sum} = a + b + cin;
            endmodule
        )";
        std::string gate = R"(
            module fa_gate(input a, b, cin, output sum, cout);
                wire axorb, aandb, candx;
                xor (axorb, a, b);
                xor (sum, axorb, cin);
                and (aandb, a, b);
                and (candx, cin, axorb);
                or  (cout, aandb, candx);
            endmodule
        )";

        auto nl_spec = tbc::parse_verilog_string(spec);
        auto nl_gate = tbc::parse_verilog_string(gate);

        auto res = tbc::run_cec(nl_spec, nl_gate);
        REQUIRE(res.equivalent);
        REQUIRE(res.mismatch_count() == 0);
    }

    SECTION("8-bit Adder: Spec vs Ripple-Carry Adder") {
        std::string spec = R"(
            module adder8(input [7:0] a, b, input cin, output [7:0] sum, output cout);
                assign {cout, sum} = a + b + cin;
            endmodule
        )";
        std::string rca = R"(
            module adder8(input [7:0] a, b, input cin, output [7:0] sum, output cout);
                wire c0, c1, c2, c3, c4, c5, c6;
                // Bit 0
                xor (sum[0], a[0], b[0], cin);
                assign c0 = (a[0] & b[0]) | (cin & (a[0] ^ b[0]));
                // Bit 1
                xor (sum[1], a[1], b[1], c0);
                assign c1 = (a[1] & b[1]) | (c0 & (a[1] ^ b[1]));
                // Bit 2
                xor (sum[2], a[2], b[2], c1);
                assign c2 = (a[2] & b[2]) | (c1 & (a[2] ^ b[2]));
                // Bit 3
                xor (sum[3], a[3], b[3], c2);
                assign c3 = (a[3] & b[3]) | (c2 & (a[3] ^ b[3]));
                // Bit 4
                xor (sum[4], a[4], b[4], c3);
                assign c4 = (a[4] & b[4]) | (c3 & (a[4] ^ b[4]));
                // Bit 5
                xor (sum[5], a[5], b[5], c4);
                assign c5 = (a[5] & b[5]) | (c4 & (a[5] ^ b[5]));
                // Bit 6
                xor (sum[6], a[6], b[6], c5);
                assign c6 = (a[6] & b[6]) | (c5 & (a[6] ^ b[6]));
                // Bit 7
                xor (sum[7], a[7], b[7], c6);
                assign cout = (a[7] & b[7]) | (c6 & (a[7] ^ b[7]));
            endmodule
        )";

        auto nl_spec = tbc::parse_verilog_string(spec);
        auto nl_rca  = tbc::parse_verilog_string(rca);

        auto res = tbc::run_cec(nl_spec, nl_rca);
        REQUIRE(res.equivalent);
    }

    SECTION("8-bit Adder: Alternative carry form C = A*B + C*(A+B) is mathematically equivalent") {
        std::string rca_or = R"(
            module adder8(input [7:0] a, b, input cin, output [7:0] sum, output cout);
                wire c0, c1, c2, c3, c4, c5, c6;
                xor (sum[0], a[0], b[0], cin); assign c0 = (a[0] & b[0]) | (cin & (a[0] | b[0]));
                xor (sum[1], a[1], b[1], c0);  assign c1 = (a[1] & b[1]) | (c0 & (a[1] | b[1]));
                xor (sum[2], a[2], b[2], c1);  assign c2 = (a[2] & b[2]) | (c1 & (a[2] | b[2]));
                xor (sum[3], a[3], b[3], c2);  assign c3 = (a[3] & b[3]) | (c2 & (a[3] | b[3]));
                xor (sum[4], a[4], b[4], c3);  assign c4 = (a[4] & b[4]) | (c3 & (a[4] | b[4]));
                xor (sum[5], a[5], b[5], c4);  assign c5 = (a[5] & b[5]) | (c4 & (a[5] | b[5]));
                xor (sum[6], a[6], b[6], c5);  assign c6 = (a[6] & b[6]) | (c5 & (a[6] | b[6]));
                xor (sum[7], a[7], b[7], c6);  assign cout = (a[7] & b[7]) | (c6 & (a[7] | b[7]));
            endmodule
        )";
        std::string spec = R"(
            module adder8(input [7:0] a, b, input cin, output [7:0] sum, output cout);
                assign {cout, sum} = a + b + cin;
            endmodule
        )";

        auto nl_spec = tbc::parse_verilog_string(spec);
        auto nl_or   = tbc::parse_verilog_string(rca_or);

        auto res = tbc::run_cec(nl_spec, nl_or);
        REQUIRE(res.equivalent);
    }

    SECTION("8-bit Adder: Bug Injection Detection & Counterexample Verification") {
        std::string bug = R"(
            module adder8(input [7:0] a, b, input cin, output [7:0] sum, output cout);
                wire c0, c1, c2, c3, c4, c5, c6;
                xor (sum[0], a[0], b[0], cin); assign c0 = (a[0] & b[0]) | (cin & (a[0] ^ b[0]));
                xor (sum[1], a[1], b[1], c0);  assign c1 = (a[1] & b[1]) | (c0 & (a[1] ^ b[1]));
                xor (sum[2], a[2], b[2], c1);  assign c2 = (a[2] & b[2]) | (c1 & (a[2] ^ b[2]));
                // Injected bug at bit 3: c3 is missing the (a[3] & b[3]) term!
                xor (sum[3], a[3], b[3], c2);  assign c3 = (c2 & (a[3] ^ b[3]));
                xor (sum[4], a[4], b[4], c3);  assign c4 = (a[4] & b[4]) | (c3 & (a[4] ^ b[4]));
                xor (sum[5], a[5], b[5], c4);  assign c5 = (a[5] & b[5]) | (c4 & (a[5] ^ b[5]));
                xor (sum[6], a[6], b[6], c5);  assign c6 = (a[6] & b[6]) | (c5 & (a[6] ^ b[6]));
                xor (sum[7], a[7], b[7], c6);  assign cout = (a[7] & b[7]) | (c6 & (a[7] ^ b[7]));
            endmodule
        )";
        std::string spec = R"(
            module adder8(input [7:0] a, b, input cin, output [7:0] sum, output cout);
                assign {cout, sum} = a + b + cin;
            endmodule
        )";

        auto nl_spec = tbc::parse_verilog_string(spec);
        auto nl_bug  = tbc::parse_verilog_string(bug);

        auto res = tbc::run_cec(nl_spec, nl_bug);
        REQUIRE_FALSE(res.equivalent);
        REQUIRE(res.mismatch_count() > 0);

        // Verify that sum[4] is among the mismatched outputs
        bool found_sum4_mismatch = false;
        for (const auto& out : res.outputs) {
            if (out.bit_name == "sum[4]") {
                REQUIRE_FALSE(out.equivalent);
                REQUIRE(out.counterexample.has_value());
                found_sum4_mismatch = true;
            }
        }
        REQUIRE(found_sum4_mismatch);
    }

    SECTION("CPU ALU: Proof of OP_SUB == OP_NEG_ADD") {
        // Direct transcription from cpu/src/alu.vhd:
        // OP_SUB:     result1 <= unsigned(input1) - unsigned(input2);
        // OP_NEG_ADD: result1 <= unsigned(input1) + unsigned(not input2) + 1;
        std::string alu_sub = R"(
            module alu_op(input [7:0] input1, input2, output [7:0] result1);
                assign result1 = input1 - input2;
            endmodule
        )";
        std::string alu_negadd = R"(
            module alu_op(input [7:0] input1, input2, output [7:0] result1);
                assign result1 = input1 + ~input2 + 1'b1;
            endmodule
        )";

        auto nl_sub = tbc::parse_verilog_string(alu_sub);
        auto nl_neg = tbc::parse_verilog_string(alu_negadd);

        auto res = tbc::run_cec(nl_sub, nl_neg);
        REQUIRE(res.equivalent);
    }
}
