// Structural 8-bit Carry-Lookahead Adder (4-bit CLA blocks)
module adder8(
    input  [7:0] a,
    input  [7:0] b,
    input        cin,
    output [7:0] sum,
    output       cout
);
    wire [7:0] p = a ^ b;
    wire [7:0] g = a & b;
    wire [8:0] c;
    assign c[0] = cin;

    // CLA Block 0: bits [3:0]
    assign c[1] = g[0] | (p[0] & c[0]);
    assign c[2] = g[1] | (p[1] & g[0]) | (p[0] & p[1] & c[0]);
    assign c[3] = g[2] | (p[2] & g[1]) | (p[1] & p[2] & g[0]) | (p[0] & p[1] & p[2] & c[0]);
    assign c[4] = g[3] | (p[3] & g[2]) | (p[2] & p[3] & g[1]) | (p[1] & p[2] & p[3] & g[0]) | (p[0] & p[1] & p[2] & p[3] & c[0]);
    // CLA Block 1: bits [7:4]
    assign c[5] = g[4] | (p[4] & c[4]);
    assign c[6] = g[5] | (p[5] & g[4]) | (p[4] & p[5] & c[4]);
    assign c[7] = g[6] | (p[6] & g[5]) | (p[5] & p[6] & g[4]) | (p[4] & p[5] & p[6] & c[4]);
    assign c[8] = g[7] | (p[7] & g[6]) | (p[6] & p[7] & g[5]) | (p[5] & p[6] & p[7] & g[4]) | (p[4] & p[5] & p[6] & p[7] & c[4]);

    // Sum bits
    xor (sum[0], p[0], c[0]);
    xor (sum[1], p[1], c[1]);
    xor (sum[2], p[2], c[2]);
    xor (sum[3], p[3], c[3]);
    xor (sum[4], p[4], c[4]);
    xor (sum[5], p[5], c[5]);
    xor (sum[6], p[6], c[6]);
    xor (sum[7], p[7], c[7]);
    assign cout = c[8];
endmodule
