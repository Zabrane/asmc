; _STRREV.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

    .code

_strrev::

    .for ( rax=rcx, rdx=rcx : byte ptr [rdx] : rdx++ )

    .endf

    .for ( rdx-- : rdx > rcx : rdx--, rcx++ )

        mov r8b,[rcx]
        mov r9b,[rdx]
        mov [rcx],r9b
        mov [rdx],r8b
    .endf
    ret

    end
