; _MBSPBRK.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include string.inc

    .code

_mbspbrk proc uses esi edi s1:LPSTR, s2:LPSTR

    mov edi,s2
    or  ecx,-1
    xor eax,eax
    repnz scasb
    not ecx
    dec ecx

    .return .ifz

    .for esi = s1, al = [esi] : eax : esi++
        mov   edi,s2
        mov   edx,ecx
        repnz scasb
        mov   ecx,edx
        .return esi .ifz
        mov al,[esi+1]
    .endf
    xor eax,eax
    ret

_mbspbrk endp

    END
