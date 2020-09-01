; _MKDIR.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include direct.inc
include errno.inc
include winbase.inc

    .code

_mkdir proc directory:LPSTR

  local wpath

    .if !CreateDirectoryA(directory, 0)

        mov wpath,__allocwpath(directory)
        add eax,8
        .if !CreateDirectoryW(eax, 0)

            CreateDirectoryW(wpath, 0)
        .endif
    .endif

    .if !eax

        osmaperr()
    .else
        xor eax,eax
    .endif
    ret

_mkdir endp

    END
