#include <catch2/catch_test_macros.hpp>
#include "tbc/blif.hpp"
#include "tbc/bdd.hpp"
#include "tbc/netlist_to_bdd.hpp"
#include "tbc/cec.hpp"
#include "tbc/verilog.hpp"

using namespace tbc;

TEST_CASE("BLIF parser basic gates", "[blif]") {
    std::string blif_src = R"(
.model simple_gate
.inputs a b c
.outputs y_and y_or y_xor y_inv y_const1
.names a b y_and
11 1
.names a b y_or
1- 1
-1 1
.names a b y_xor
10 1
01 1
.names c y_inv
0 1
.names y_const1
1
.end
)";

    Netlist nl = parse_blif_string(blif_src);
    REQUIRE(nl.module_name == "simple_gate");
    REQUIRE(nl.inputs.size() == 3);
    REQUIRE(nl.outputs.size() == 5);

    Manager mgr;
    std::unordered_map<std::string, VarID> var_map;
    var_map["a"] = mgr.new_var("a");
    var_map["b"] = mgr.new_var("b");
    var_map["c"] = mgr.new_var("c");

    auto roots = netlist_to_bdd(mgr, nl, var_map);
    REQUIRE(roots.size() == 5);

    NodeID a = mgr.var(var_map["a"]);
    NodeID b = mgr.var(var_map["b"]);
    NodeID c = mgr.var(var_map["c"]);

    // y_and == a & b
    CHECK(roots[0] == mgr.band(a, b));
    // y_or == a | b
    CHECK(roots[1] == mgr.bor(a, b));
    // y_xor == a ^ b
    CHECK(roots[2] == mgr.bxor(a, b));
    // y_inv == ~c
    CHECK(roots[3] == mgr.bnot(c));
    // y_const1 == 1
    CHECK(roots[4] == kTrue);
}

TEST_CASE("BLIF vs Verilog Equivalence via CEC", "[blif][cec]") {
    std::string verilog_src = R"(
module alu_ref (
    input [3:0] a,
    input [3:0] b,
    output [3:0] sum,
    output cout
);
    assign {cout, sum} = a + b;
endmodule
)";

    // Equivalent structural BLIF for 4-bit ripple-carry adder
    std::string blif_src = R"(
.model alu_ref
.inputs a[0] a[1] a[2] a[3] b[0] b[1] b[2] b[3]
.outputs sum[0] sum[1] sum[2] sum[3] cout

# Bit 0
.names a[0] b[0] sum[0]
10 1
01 1
.names a[0] b[0] c0
11 1

# Bit 1
.names a[1] b[1] p1
10 1
01 1
.names p1 c0 sum[1]
10 1
01 1
.names a[1] b[1] c0 c1
11- 1
1-1 1
-11 1

# Bit 2
.names a[2] b[2] p2
10 1
01 1
.names p2 c1 sum[2]
10 1
01 1
.names a[2] b[2] c1 c2
11- 1
1-1 1
-11 1

# Bit 3
.names a[3] b[3] p3
10 1
01 1
.names p3 c2 sum[3]
10 1
01 1
.names a[3] b[3] c2 cout
11- 1
1-1 1
-11 1

.end
)";

    Netlist v_nl = parse_verilog_string(verilog_src);
    Netlist b_nl = parse_blif_string(blif_src);

    CecOptions opts;
    opts.quiet = true;
    CecResult res = run_cec(v_nl, b_nl, opts);

    CHECK(res.equivalent == true);
    for (const auto& out : res.outputs) {
        CHECK(out.equivalent == true);
    }
}

TEST_CASE("BLIF parser subckt and DFF synchronous reset", "[blif]") {
    std::string blif_src = R"(
.model dff_test
.inputs clk en d
.outputs q_pp q_pn
.subckt $_SDFF_PP0_ C=clk D=d Q=q_pp R=en
.subckt $_SDFF_PN0_ C=clk D=d Q=q_pn R=en
.end
)";

    Netlist nl = parse_blif_string(blif_src);
    REQUIRE(nl.inputs.size() == 3);
    REQUIRE(nl.outputs.size() == 2);

    Manager mgr;
    std::unordered_map<std::string, VarID> var_map;
    var_map["clk"] = mgr.new_var("clk");
    var_map["en"]  = mgr.new_var("en");
    var_map["d"]   = mgr.new_var("d");

    auto roots = netlist_to_bdd(mgr, nl, var_map);
    NodeID en = mgr.var(var_map["en"]);
    NodeID d  = mgr.var(var_map["d"]);

    // q_pp has active-high reset to 0: ~en & d
    CHECK(roots[0] == mgr.band(mgr.bnot(en), d));
    // q_pn has active-low reset to 0: en & d
    CHECK(roots[1] == mgr.band(en, d));
}

TEST_CASE("Yosys Synthesized CPU Decoder Formal Verification", "[eda][cec]") {
    Netlist v_nl = parse_verilog_file("examples/cpu_decoder_spec.v");
    Netlist b_nl = parse_blif_file("out/cpu_decoder_synth.blif");

    CecOptions opts;
    opts.quiet = true;
    CecResult res = run_cec(v_nl, b_nl, opts);

    CHECK(res.equivalent == true);
    CHECK(res.outputs.size() == 16);
    for (const auto& out : res.outputs) {
        CHECK(out.equivalent == true);
    }
}

TEST_CASE("Yosys Synthesized L1 Cache Datapath Formal Verification", "[eda][cec]") {
    Netlist v_nl = parse_verilog_file("examples/l1_cache_datapath_spec.v");
    Netlist b_nl = parse_blif_file("out/l1_cache_datapath_synth.blif");

    CecOptions opts;
    opts.quiet = true;
    CecResult res = run_cec(v_nl, b_nl, opts);

    CHECK(res.equivalent == true);
    CHECK(res.outputs.size() == 35);
    for (const auto& out : res.outputs) {
        CHECK(out.equivalent == true);
    }
}

TEST_CASE("Yosys Synthesized Saturating Clamp Formal Verification", "[eda][cec]") {
    Netlist v_nl = parse_verilog_file("examples/clamp_byte_opt.v");
    Netlist b_nl = parse_blif_file("out/clamp_byte_synth.blif");

    CecOptions opts;
    opts.quiet = true;
    CecResult res = run_cec(v_nl, b_nl, opts);

    CHECK(res.equivalent == true);
    CHECK(res.outputs.size() == 8);
    for (const auto& out : res.outputs) {
        CHECK(out.equivalent == true);
    }
}

TEST_CASE("CPU ALU Subtraction vs Negation-Addition Equivalence", "[eda][cec]") {
    Netlist a_nl = parse_verilog_file("examples/alu_sub.v");
    Netlist b_nl = parse_verilog_file("examples/alu_neg_add.v");

    CecOptions opts;
    opts.quiet = true;
    CecResult res = run_cec(a_nl, b_nl, opts);

    CHECK(res.equivalent == true);
    CHECK(res.outputs.size() == 8);
    for (const auto& out : res.outputs) {
        CHECK(out.equivalent == true);
    }
}
