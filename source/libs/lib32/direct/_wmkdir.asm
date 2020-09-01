; _WMKDIR.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include direct.inc
include errno.inc
include winbase.inc

    .code

_wmkdir proc directory:LPWSTR

    .if CreateDirectoryW(directory, 0)

        xor eax,eax

    .else
        osmaperr()
    .endif
    ret

_wmkdir endp

    END
