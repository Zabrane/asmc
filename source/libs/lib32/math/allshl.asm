; ALLSHL.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;
; long shift left
;
; signed and unsigned are same
;
    .386
    .model flat, c
    .code

_U8LS::             ; Watcom
_I8LS::

    mov ecx,ebx     ; edx:eax << bl

_allshl::           ; edx:eax << cl
__llshl::           ; Microsoft

    .if cl < 64

        .if cl < 32

            shld edx,eax,cl
            shl eax,cl
            ret
        .endif

        and ecx,31
        mov edx,eax
        xor eax,eax
        shl edx,cl
        ret
    .endif

    xor eax,eax
    xor edx,edx
    ret

    END
