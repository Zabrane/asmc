; __CVTI32_Q.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include quadmath.inc

    .code

__cvti32_q proc x:ptr, l:long_t

    mov  eax,l
    test eax,eax        ; if number is negative
    .ifs
        neg eax         ; negate number
        mov edx,0xBFFF  ; set exponent
    .else
        mov edx,0x3FFF
    .endif
    .if eax
        bsr ecx,eax     ; find most significant non-zero bit
        mov ch,cl
        mov cl,32
        sub cl,ch
        .if cl == 32
            xor eax,eax
        .else
            shl eax,cl  ; shift bits into position
        .endif
        shr ecx,8       ; get shift count
        add ecx,edx     ; calculate exponent
        mov edx,eax     ; get the bits
    .else
        xor ecx,ecx     ; else zero
        xor edx,edx
    .endif
    mov eax,x
    mov [eax+10],edx
    mov [eax+14],cx
    xor edx,edx         ; zero the rest of the fraction bits
    mov [eax],edx
    mov [eax+4],edx
    mov [eax+8],dx
    ret

__cvti32_q endp

    end

