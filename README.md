# Dyn-Instr-Schl-OOO
Dynamic instruction scheduling simulator for an out-of-order (OOO) superscalar processor that fetches and issues N instructions per cycle. The dynamic scheduling mechanism is modeled under the assumption of perfect branch prediction and cache design.

Design considerations:
The number of architectural registers specified in the ISA is assumed to be 67 (r0-r66). Since the number of architectural registers determines the number of entries in the Rename Map Table (RMT) and Architectural Register File (ARF), size of ARF and size of RMT is equal to 67.

The simulator takes a command line argument of the following format:
sim <ROB_SIZE> <IQ_SIZE> <WIDTH> <tracefile>
where,
<ROB_SIZE> is Reorder buffer size
<IQ_SIZE> is Issue Queue size
<WIDTH> specifies the superscalar width of all pipeline stages, in terms of the maximum number of instructions in each pipeline stage
<tracefile> is the path to trace file containing assembly code traces that are to be fed to the OOO processor.

![micro-arch-to-be-modelled](/Dyn-Instr-Schl-OOO/main/assets/microarch-to-be-modelled.png?raw=true "microarchitecture to be modelled")

The simulator reads a trace file in the following format:
<PC> <operation type> <dest reg #> <src1 reg #> <src2 reg #>
where,
<PC> is the program counter of the instruction (in hex).
<operation type> is either “0”, “1”, or “2”.
  type "0" can be ADD/SUB or any instructions that take 1 cycle in ALU to execute
  type "1" can be BEQ/BNE or any instructions that take 2 cycles in ALU to execute
  type "2" can  or any instructions that take 1 cycle in ALU to execute
<dest reg #> is the destination register of the instruction. If it is -1, then the instruction does not have a destination register (for example, a conditional branch instruction). Otherwise, it is between 0 and 66.
<src1 reg #> is the first source register of the instruction. If it is -1, then the instruction does not have a first source register. Otherwise, it is between 0 and 66.
<src2 reg #> is the second source register of the instruction. If it is -1, then the instruction does not have a second source register. Otherwise, it is between 0 and 66.
