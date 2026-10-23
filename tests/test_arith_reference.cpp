#include <catch2/catch_test_macros.hpp>
#include "tbc/verilog.hpp"
#include <random>

TEST_CASE("Exhaustive Arithmetic Reference Verification", "[arith]") {
    SECTION("8-bit Adder exhaustive verification against native C++ addition (2^17 vectors)") {
        std::string src = R"(
            module adder8(input [7:0] a, b, input cin, output [7:0] sum, output cout);
                assign {cout, sum} = a + b + cin;
            endmodule
        )";
        auto nl = tbc::parse_verilog_string(src);
        REQUIRE(nl.inputs.size() == 17);
        REQUIRE(nl.outputs.size() == 9);

        // Input port order: a[0..7], b[0..7], cin
        std::vector<bool> in_vec(17, false);

        for (int a = 0; a < 256; ++a) {
            for (int b = 0; b < 256; ++b) {
                for (int cin = 0; cin < 2; ++cin) {
                    for (int i = 0; i < 8; ++i) {
                        in_vec[static_cast<std::size_t>(i)]     = ((a >> i) & 1) != 0;
                        in_vec[static_cast<std::size_t>(8 + i)] = ((b >> i) & 1) != 0;
                    }
                    in_vec[16] = (cin != 0);

                    auto out = nl.simulate(in_vec);

                    // Output port order: sum[0..7], cout
                    int sim_sum = 0;
                    for (int i = 0; i < 8; ++i) {
                        if (out[static_cast<std::size_t>(i)]) sim_sum |= (1 << i);
                    }
                    int sim_cout = out[8] ? 1 : 0;
                    int sim_total = (sim_cout << 8) | sim_sum;

                    int expected_total = a + b + cin;
                    if (sim_total != expected_total) {
                        FAIL_CHECK("Mismatch for a=" << a << " b=" << b << " cin=" << cin
                                   << ": sim=" << sim_total << " expected=" << expected_total);
                    }
                }
            }
        }
    }

    SECTION("8-bit Subtraction exhaustive verification against native C++ subtraction (2^16 vectors)") {
        std::string src = R"(
            module sub8(input [7:0] a, b, output [7:0] diff);
                assign diff = a - b;
            endmodule
        )";
        auto nl = tbc::parse_verilog_string(src);
        std::vector<bool> in_vec(16, false);

        for (int a = 0; a < 256; ++a) {
            for (int b = 0; b < 256; ++b) {
                for (int i = 0; i < 8; ++i) {
                    in_vec[static_cast<std::size_t>(i)]     = ((a >> i) & 1) != 0;
                    in_vec[static_cast<std::size_t>(8 + i)] = ((b >> i) & 1) != 0;
                }
                auto out = nl.simulate(in_vec);
                int sim_diff = 0;
                for (int i = 0; i < 8; ++i) {
                    if (out[static_cast<std::size_t>(i)]) sim_diff |= (1 << i);
                }
                int expected_diff = (a - b) & 0xFF;
                if (sim_diff != expected_diff) {
                    FAIL_CHECK("Sub mismatch for a=" << a << " b=" << b
                               << ": sim=" << sim_diff << " expected=" << expected_diff);
                }
            }
        }
    }

    SECTION("16-bit Adder random verification against native C++ addition (1,000 vectors)") {
        std::string src = R"(
            module adder16(input [15:0] a, b, input cin, output [15:0] sum, output cout);
                assign {cout, sum} = a + b + cin;
            endmodule
        )";
        auto nl = tbc::parse_verilog_string(src);
        std::mt19937_64 rng(1337);
        std::uniform_int_distribution<uint32_t> dist16(0, 0xFFFF);
        std::uniform_int_distribution<uint32_t> dist1(0, 1);

        std::vector<bool> in_vec(33, false);
        for (int iter = 0; iter < 1000; ++iter) {
            uint32_t a = dist16(rng);
            uint32_t b = dist16(rng);
            uint32_t cin = dist1(rng);

            for (int i = 0; i < 16; ++i) {
                in_vec[static_cast<std::size_t>(i)]      = ((a >> i) & 1) != 0;
                in_vec[static_cast<std::size_t>(16 + i)] = ((b >> i) & 1) != 0;
            }
            in_vec[32] = (cin != 0);

            auto out = nl.simulate(in_vec);
            uint32_t sim_sum = 0;
            for (int i = 0; i < 16; ++i) {
                if (out[static_cast<std::size_t>(i)]) sim_sum |= (1U << i);
            }
            uint32_t sim_cout = out[16] ? 1U : 0U;
            uint32_t sim_total = (sim_cout << 16) | sim_sum;
            uint32_t expected = a + b + cin;

            REQUIRE(sim_total == expected);
        }
    }
}
