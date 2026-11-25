/* Link this file LAST. It rounds .text up to a whole number of 32-bit words.
 * ELF loader 1.3.3 copies .text with memcpy(); when the size is not a
 * multiple of 4 the trailing byte stores can fault (LoadStoreError) if the
 * code is placed in instruction RAM. Never executed.
 */
    .section .text.zz_text_pad,"ax",@progbits
    .balign 4
    .global invader_text_pad
    .hidden invader_text_pad
invader_text_pad:
    .word 0
