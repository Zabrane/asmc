; ALLMUL.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

    .code

_allmul:: ; rdx::rcx * r9::r8

    imul    rdx,r8
    mov     rax,rcx
    imul    r9,rcx
    add     r9,rdx
    mul     r8
    add     rdx,r9
    ret

    END
