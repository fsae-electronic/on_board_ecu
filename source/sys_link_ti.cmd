--retain="*(.intvecs)"
--retain="*(intvecs)"
--retain="*(reset)"

MEMORY
{
    VECTORS (RX)  : origin = 0x00000000, length = 0x00000020
    FLASH0  (RX)  : origin = 0x00000020, length = 0x0013FFE0
    STACKS  (RW)  : origin = 0x08000000, length = 0x00001500
    RAM     (RW)  : origin = 0x08001500, length = 0x0002EB00
}

SECTIONS
{
    .intvecs   : {} > VECTORS
    intvecs    : {} > VECTORS
    reset      : {} > FLASH0

    .text      : {} > FLASH0
    .const     : {} > FLASH0
    .cinit     : {} > FLASH0
    .pinit     : {} > FLASH0
    .init_array: {} > FLASH0
    .binit     : {} > FLASH0
    .switch    : {} > FLASH0
    .rodata    : {} > FLASH0
    .ARM.exidx : {} > FLASH0
    .ARM.extab : {} > FLASH0

    .stack     : {} > STACKS
    .sysmem    : {} > RAM
    .cio       : {} > RAM
    .args      : {} > RAM
    .bss       : {} > RAM
    .data      : {} > RAM
    .far       : {} > RAM
    .fardata   : {} > RAM
}