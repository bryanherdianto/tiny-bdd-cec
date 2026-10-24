// 8-bit RISC CPU Logic Operations Structural Gate-Level Implementation
// Built using De Morgan equivalent decompositions and NAND-form multiplexing
module alu_logic(
    input  [3:0] opcode,
    input  [7:0] input1,
    input  [7:0] input2,
    output [7:0] result1
);
    // De Morgan OR: A | B == ~(~A & ~B)
    wire [7:0] op_or = ~(~input1 & ~input2);

    // SOP XOR: (A & ~B) | (~A & B)
    wire [7:0] op_xor = (input1 & ~input2) | (~input1 & input2);

    wire [7:0] op_and  = input1 & input2;
    wire [7:0] op_not  = ~input1;
    wire [7:0] op_stor = input2;

    wire sel_and  = (!opcode[3]) &  opcode[2]  & (!opcode[1]) & (!opcode[0]); // 0100
    wire sel_or   = (!opcode[3]) &  opcode[2]  & (!opcode[1]) &  opcode[0];  // 0101
    wire sel_xor  = (!opcode[3]) &  opcode[2]  &  opcode[1]  & (!opcode[0]); // 0110
    wire sel_not  = (!opcode[3]) &  opcode[2]  &  opcode[1]  &  opcode[0];   // 0111
    wire sel_stor =  opcode[3]  & (!opcode[2]) & (!opcode[1]) &  opcode[0];  // 1001

    // Bit-slice multiplexing
    assign result1 = ({8{sel_and}}  & op_and)  |
                     ({8{sel_or}}   & op_or)   |
                     ({8{sel_xor}}  & op_xor)  |
                     ({8{sel_not}}  & op_not)  |
                     ({8{sel_stor}} & op_stor);
endmodule
