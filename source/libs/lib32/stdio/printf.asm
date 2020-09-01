; PRINTF.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include stdio.inc

    .code

printf proc c format:LPSTR, argptr:VARARG

    _stbuf(&stdout)
    push eax
    _output(&stdout, format, &argptr)
    pop  edx
    push eax
    _ftbuf(edx, &stdout)
    pop  eax
    ret

printf endp

    END
