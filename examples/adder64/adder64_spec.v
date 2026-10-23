// Behavioral 64-bit Adder Specification
module adder64(
    input  [63:0] a,
    input  [63:0] b,
    input        cin,
    output [63:0] sum,
    output       cout
);
    assign {cout, sum} = a + b + cin;
endmodule
