// Structural 8-bit Ripple-Carry Adder 
module adder8(
    input  [7:0] a,
    input  [7:0] b,
    input        cin,
    output [7:0] sum,
    output       cout
);
    wire [6:0] c;

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
    assign cout = (a[7] & b[7]) | (c[6] & (a[7] | b[7]));
endmodule
