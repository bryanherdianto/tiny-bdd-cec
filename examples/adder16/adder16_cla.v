// Structural 16-bit Carry-Lookahead Adder (4-bit CLA blocks)
module adder16(
    input  [15:0] a,
    input  [15:0] b,
    input        cin,
    output [15:0] sum,
    output       cout
);
    wire [15:0] p = a ^ b;
    wire [15:0] g = a & b;
    wire [16:0] c;
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
    // CLA Block 2: bits [11:8]
    assign c[9] = g[8] | (p[8] & c[8]);
    assign c[10] = g[9] | (p[9] & g[8]) | (p[8] & p[9] & c[8]);
    assign c[11] = g[10] | (p[10] & g[9]) | (p[9] & p[10] & g[8]) | (p[8] & p[9] & p[10] & c[8]);
    assign c[12] = g[11] | (p[11] & g[10]) | (p[10] & p[11] & g[9]) | (p[9] & p[10] & p[11] & g[8]) | (p[8] & p[9] & p[10] & p[11] & c[8]);
    // CLA Block 3: bits [15:12]
    assign c[13] = g[12] | (p[12] & c[12]);
    assign c[14] = g[13] | (p[13] & g[12]) | (p[12] & p[13] & c[12]);
    assign c[15] = g[14] | (p[14] & g[13]) | (p[13] & p[14] & g[12]) | (p[12] & p[13] & p[14] & c[12]);
    assign c[16] = g[15] | (p[15] & g[14]) | (p[14] & p[15] & g[13]) | (p[13] & p[14] & p[15] & g[12]) | (p[12] & p[13] & p[14] & p[15] & c[12]);

    // Sum bits
    xor (sum[0], p[0], c[0]);
    xor (sum[1], p[1], c[1]);
    xor (sum[2], p[2], c[2]);
    xor (sum[3], p[3], c[3]);
    xor (sum[4], p[4], c[4]);
    xor (sum[5], p[5], c[5]);
    xor (sum[6], p[6], c[6]);
    xor (sum[7], p[7], c[7]);
    xor (sum[8], p[8], c[8]);
    xor (sum[9], p[9], c[9]);
    xor (sum[10], p[10], c[10]);
    xor (sum[11], p[11], c[11]);
    xor (sum[12], p[12], c[12]);
    xor (sum[13], p[13], c[13]);
    xor (sum[14], p[14], c[14]);
    xor (sum[15], p[15], c[15]);
    assign cout = c[16];
endmodule
