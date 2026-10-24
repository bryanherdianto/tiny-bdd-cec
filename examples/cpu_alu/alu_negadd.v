// 8-bit RISC CPU ALU Operation: OP_NEG_ADD (1000)
// Direct transcription from cpu/src/alu.vhd:
// WHEN "1000" => result1 <= STD_LOGIC_VECTOR(unsigned(input1) + unsigned(NOT input2) + 1);
module alu_op(
    input  [7:0] input1,
    input  [7:0] input2,
    output [7:0] result1
);
    assign result1 = input1 + ~input2 + 1'b1;
endmodule
