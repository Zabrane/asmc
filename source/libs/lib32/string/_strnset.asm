; _STRNSET.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include string.inc

    .code

_strnset proc dst:LPSTR, char:SINT, max:SIZE_T

    mov edx,dst
    mov eax,char
    mov ecx,max

    .while ecx && byte ptr [edx]

        mov [edx],al
        inc edx
        dec ecx
    .endw
    mov eax,dst
    ret

_strnset endp

    END
