// Structural 32-bit Carry-Lookahead Adder (4-bit CLA blocks)
module adder32(
    input  [31:0] a,
    input  [31:0] b,
    input        cin,
    output [31:0] sum,
    output       cout
);
    wire [31:0] p = a ^ b;
    wire [31:0] g = a & b;
    wire [32:0] c;
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
    // CLA Block 4: bits [19:16]
    assign c[17] = g[16] | (p[16] & c[16]);
    assign c[18] = g[17] | (p[17] & g[16]) | (p[16] & p[17] & c[16]);
    assign c[19] = g[18] | (p[18] & g[17]) | (p[17] & p[18] & g[16]) | (p[16] & p[17] & p[18] & c[16]);
    assign c[20] = g[19] | (p[19] & g[18]) | (p[18] & p[19] & g[17]) | (p[17] & p[18] & p[19] & g[16]) | (p[16] & p[17] & p[18] & p[19] & c[16]);
    // CLA Block 5: bits [23:20]
    assign c[21] = g[20] | (p[20] & c[20]);
    assign c[22] = g[21] | (p[21] & g[20]) | (p[20] & p[21] & c[20]);
    assign c[23] = g[22] | (p[22] & g[21]) | (p[21] & p[22] & g[20]) | (p[20] & p[21] & p[22] & c[20]);
    assign c[24] = g[23] | (p[23] & g[22]) | (p[22] & p[23] & g[21]) | (p[21] & p[22] & p[23] & g[20]) | (p[20] & p[21] & p[22] & p[23] & c[20]);
    // CLA Block 6: bits [27:24]
    assign c[25] = g[24] | (p[24] & c[24]);
    assign c[26] = g[25] | (p[25] & g[24]) | (p[24] & p[25] & c[24]);
    assign c[27] = g[26] | (p[26] & g[25]) | (p[25] & p[26] & g[24]) | (p[24] & p[25] & p[26] & c[24]);
    assign c[28] = g[27] | (p[27] & g[26]) | (p[26] & p[27] & g[25]) | (p[25] & p[26] & p[27] & g[24]) | (p[24] & p[25] & p[26] & p[27] & c[24]);
    // CLA Block 7: bits [31:28]
    assign c[29] = g[28] | (p[28] & c[28]);
    assign c[30] = g[29] | (p[29] & g[28]) | (p[28] & p[29] & c[28]);
    assign c[31] = g[30] | (p[30] & g[29]) | (p[29] & p[30] & g[28]) | (p[28] & p[29] & p[30] & c[28]);
    assign c[32] = g[31] | (p[31] & g[30]) | (p[30] & p[31] & g[29]) | (p[29] & p[30] & p[31] & g[28]) | (p[28] & p[29] & p[30] & p[31] & c[28]);

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
    xor (sum[16], p[16], c[16]);
    xor (sum[17], p[17], c[17]);
    xor (sum[18], p[18], c[18]);
    xor (sum[19], p[19], c[19]);
    xor (sum[20], p[20], c[20]);
    xor (sum[21], p[21], c[21]);
    xor (sum[22], p[22], c[22]);
    xor (sum[23], p[23], c[23]);
    xor (sum[24], p[24], c[24]);
    xor (sum[25], p[25], c[25]);
    xor (sum[26], p[26], c[26]);
    xor (sum[27], p[27], c[27]);
    xor (sum[28], p[28], c[28]);
    xor (sum[29], p[29], c[29]);
    xor (sum[30], p[30], c[30]);
    xor (sum[31], p[31], c[31]);
    assign cout = c[32];
endmodule
