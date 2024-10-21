
.section .bss.data
   .balign 64
   .fill 4096

.section .bss.stacks
  .global stacks_start
  .global stacks_end
  .global dummy_stack_end
    .balign 64
stacks_start:
    .fill    4096 * 32
stacks_end:
    .fill    4096
dummy_stack_end:
    .fill    4096
dummy_end:
