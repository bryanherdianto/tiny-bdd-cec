// 8-bit RISC CPU Logic Operations Specification (cpu/src/alu.vhd)
// Implements OP_AND (0100), OP_OR (0101), OP_XOR (0110), OP_NOT (0111), OP_STOR (1001)
module alu_logic(
    input  [3:0] opcode,
    input  [7:0] input1,
    input  [7:0] input2,
    output [7:0] result1
);
    wire [7:0] op_and  = input1 & input2;
    wire [7:0] op_or   = input1 | input2;
    wire [7:0] op_xor  = input1 ^ input2;
    wire [7:0] op_not  = ~input1;
    wire [7:0] op_stor = input2;

    assign result1 = (opcode == 4'b0100) ? op_and :
                     (opcode == 4'b0101) ? op_or :
                     (opcode == 4'b0110) ? op_xor :
                     (opcode == 4'b0111) ? op_not :
                     (opcode == 4'b1001) ? op_stor : 8'b00000000;
endmodule
