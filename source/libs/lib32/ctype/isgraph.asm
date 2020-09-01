; ISGRAPH.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include ctype.inc

    .code

isgraph proc char:SINT

    movzx eax,BYTE PTR char
    .if eax < 0x21 || eax >= 0x7F
        xor eax,eax
    .endif
    ret

isgraph endp

    END
