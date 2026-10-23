// Structural 16-bit Ripple-Carry Adder 
module adder16(
    input  [15:0] a,
    input  [15:0] b,
    input        cin,
    output [15:0] sum,
    output       cout
);
    wire [14:0] c;

    // Bit 0
    xor (sum[0], a[0], b[0], cin);
    assign c[0] = (a[0] & b[0]) | (cin & (a[0] | b[0]));
    // Bit 1
    xor (sum[1], a[1], b[1], c[0]);
    assign c[1] = (a[1] & b[1]) | (c[0] & (a[1] | b[1]));
    // Bit 2
    xor (sum[2], a[2], b[2], c[1]);
    assign c[2] = (a[2] & b[2]) | (c[1] & (a[2] | b[2]));
    // Bit 3
    xor (sum[3], a[3], b[3], c[2]);
    assign c[3] = (a[3] & b[3]) | (c[2] & (a[3] | b[3]));
    // Bit 4
    xor (sum[4], a[4], b[4], c[3]);
    assign c[4] = (a[4] & b[4]) | (c[3] & (a[4] | b[4]));
    // Bit 5
    xor (sum[5], a[5], b[5], c[4]);
    assign c[5] = (a[5] & b[5]) | (c[4] & (a[5] | b[5]));
    // Bit 6
    xor (sum[6], a[6], b[6], c[5]);
    assign c[6] = (a[6] & b[6]) | (c[5] & (a[6] | b[6]));
    // Bit 7
    xor (sum[7], a[7], b[7], c[6]);
    assign c[7] = (a[7] & b[7]) | (c[6] & (a[7] | b[7]));
    // Bit 8
    xor (sum[8], a[8], b[8], c[7]);
    assign c[8] = (a[8] & b[8]) | (c[7] & (a[8] | b[8]));
    // Bit 9
    xor (sum[9], a[9], b[9], c[8]);
    assign c[9] = (a[9] & b[9]) | (c[8] & (a[9] | b[9]));
    // Bit 10
    xor (sum[10], a[10], b[10], c[9]);
    assign c[10] = (a[10] & b[10]) | (c[9] & (a[10] | b[10]));
    // Bit 11
    xor (sum[11], a[11], b[11], c[10]);
    assign c[11] = (a[11] & b[11]) | (c[10] & (a[11] | b[11]));
    // Bit 12
    xor (sum[12], a[12], b[12], c[11]);
    assign c[12] = (a[12] & b[12]) | (c[11] & (a[12] | b[12]));
    // Bit 13
    xor (sum[13], a[13], b[13], c[12]);
    assign c[13] = (a[13] & b[13]) | (c[12] & (a[13] | b[13]));
    // Bit 14
    xor (sum[14], a[14], b[14], c[13]);
    assign c[14] = (a[14] & b[14]) | (c[13] & (a[14] | b[14]));
    // Bit 15
    xor (sum[15], a[15], b[15], c[14]);
    assign cout = (a[15] & b[15]) | (c[14] & (a[15] | b[15]));
endmodule
