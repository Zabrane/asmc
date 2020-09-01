; FOPEN.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include stdio.inc
include share.inc

    .code

    option win64:rsp nosave

fopen proc fname:LPSTR, mode:LPSTR

    .if _getst()

        _openfile(rcx, rdx, SH_DENYNO, rax)
    .endif
    ret

fopen endp

    END
