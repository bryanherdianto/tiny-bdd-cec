// Structural 32-bit Ripple-Carry Adder 
module adder32(
    input  [31:0] a,
    input  [31:0] b,
    input        cin,
    output [31:0] sum,
    output       cout
);
    wire [30:0] c;

    // Bit 0
    xor (sum[0], a[0], b[0], cin);
    assign c[0] = (a[0] & b[0]) | (cin & (a[0] ^ b[0]));
    // Bit 1
    xor (sum[1], a[1], b[1], c[0]);
    assign c[1] = (a[1] & b[1]) | (c[0] & (a[1] ^ b[1]));
    // Bit 2
    xor (sum[2], a[2], b[2], c[1]);
    assign c[2] = (a[2] & b[2]) | (c[1] & (a[2] ^ b[2]));
    // Bit 3
    xor (sum[3], a[3], b[3], c[2]);
    assign c[3] = (a[3] & b[3]) | (c[2] & (a[3] ^ b[3]));
    // Bit 4
    xor (sum[4], a[4], b[4], c[3]);
    assign c[4] = (a[4] & b[4]) | (c[3] & (a[4] ^ b[4]));
    // Bit 5
    xor (sum[5], a[5], b[5], c[4]);
    assign c[5] = (a[5] & b[5]) | (c[4] & (a[5] ^ b[5]));
    // Bit 6
    xor (sum[6], a[6], b[6], c[5]);
    assign c[6] = (a[6] & b[6]) | (c[5] & (a[6] ^ b[6]));
    // Bit 7
    xor (sum[7], a[7], b[7], c[6]);
    assign c[7] = (a[7] & b[7]) | (c[6] & (a[7] ^ b[7]));
    // Bit 8
    xor (sum[8], a[8], b[8], c[7]);
    assign c[8] = (a[8] & b[8]) | (c[7] & (a[8] ^ b[8]));
    // Bit 9
    xor (sum[9], a[9], b[9], c[8]);
    assign c[9] = (a[9] & b[9]) | (c[8] & (a[9] ^ b[9]));
    // Bit 10
    xor (sum[10], a[10], b[10], c[9]);
    assign c[10] = (a[10] & b[10]) | (c[9] & (a[10] ^ b[10]));
    // Bit 11
    xor (sum[11], a[11], b[11], c[10]);
    assign c[11] = (a[11] & b[11]) | (c[10] & (a[11] ^ b[11]));
    // Bit 12
    xor (sum[12], a[12], b[12], c[11]);
    assign c[12] = (a[12] & b[12]) | (c[11] & (a[12] ^ b[12]));
    // Bit 13
    xor (sum[13], a[13], b[13], c[12]);
    assign c[13] = (a[13] & b[13]) | (c[12] & (a[13] ^ b[13]));
    // Bit 14
    xor (sum[14], a[14], b[14], c[13]);
    assign c[14] = (a[14] & b[14]) | (c[13] & (a[14] ^ b[14]));
    // Bit 15
    xor (sum[15], a[15], b[15], c[14]);
    assign c[15] = (a[15] & b[15]) | (c[14] & (a[15] ^ b[15]));
    // Bit 16
    xor (sum[16], a[16], b[16], c[15]);
    assign c[16] = (a[16] & b[16]) | (c[15] & (a[16] ^ b[16]));
    // Bit 17
    xor (sum[17], a[17], b[17], c[16]);
    assign c[17] = (a[17] & b[17]) | (c[16] & (a[17] ^ b[17]));
    // Bit 18
    xor (sum[18], a[18], b[18], c[17]);
    assign c[18] = (a[18] & b[18]) | (c[17] & (a[18] ^ b[18]));
    // Bit 19
    xor (sum[19], a[19], b[19], c[18]);
    assign c[19] = (a[19] & b[19]) | (c[18] & (a[19] ^ b[19]));
    // Bit 20
    xor (sum[20], a[20], b[20], c[19]);
    assign c[20] = (a[20] & b[20]) | (c[19] & (a[20] ^ b[20]));
    // Bit 21
    xor (sum[21], a[21], b[21], c[20]);
    assign c[21] = (a[21] & b[21]) | (c[20] & (a[21] ^ b[21]));
    // Bit 22
    xor (sum[22], a[22], b[22], c[21]);
    assign c[22] = (a[22] & b[22]) | (c[21] & (a[22] ^ b[22]));
    // Bit 23
    xor (sum[23], a[23], b[23], c[22]);
    assign c[23] = (a[23] & b[23]) | (c[22] & (a[23] ^ b[23]));
    // Bit 24
    xor (sum[24], a[24], b[24], c[23]);
    assign c[24] = (a[24] & b[24]) | (c[23] & (a[24] ^ b[24]));
    // Bit 25
    xor (sum[25], a[25], b[25], c[24]);
    assign c[25] = (a[25] & b[25]) | (c[24] & (a[25] ^ b[25]));
    // Bit 26
    xor (sum[26], a[26], b[26], c[25]);
    assign c[26] = (a[26] & b[26]) | (c[25] & (a[26] ^ b[26]));
    // Bit 27
    xor (sum[27], a[27], b[27], c[26]);
    assign c[27] = (a[27] & b[27]) | (c[26] & (a[27] ^ b[27]));
    // Bit 28
    xor (sum[28], a[28], b[28], c[27]);
    assign c[28] = (a[28] & b[28]) | (c[27] & (a[28] ^ b[28]));
    // Bit 29
    xor (sum[29], a[29], b[29], c[28]);
    assign c[29] = (a[29] & b[29]) | (c[28] & (a[29] ^ b[29]));
    // Bit 30
    xor (sum[30], a[30], b[30], c[29]);
    assign c[30] = (a[30] & b[30]) | (c[29] & (a[30] ^ b[30]));
    // Bit 31
    xor (sum[31], a[31], b[31], c[30]);
    assign cout = (a[31] & b[31]) | (c[30] & (a[31] ^ b[31]));
endmodule
