section .multiboot_header
align 8
header_start:
    ; magic number (Multiboot2)
    dd 0xe85250d6

    ; architecture (0 = i386)
    dd 0

    ; header length
    dd header_end - header_start

    ; checksum (so that sum of all four is zero)
    dd -(0xe85250d6 + 0 + (header_end - header_start))

    ; end tag (type = 0, size = 8)
    dw 0
    dw 0
    dd 8
header_end:
