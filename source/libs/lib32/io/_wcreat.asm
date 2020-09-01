; _WCREAT.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include io.inc
include fcntl.inc
include stat.inc
include errno.inc
include winbase.inc

    .code

_wcreat proc path:LPWSTR, flag:SINT

    mov edx,_A_NORMAL
    mov ecx,O_WRONLY
    mov eax,flag
    and eax,S_IREAD or S_IWRITE
    .repeat
        .if eax != S_IWRITE
            mov ecx,O_RDWR
            .if eax != S_IREAD or S_IWRITE
                .if eax == S_IREAD
                    mov ecx,O_RDONLY
                    mov edx,_A_RDONLY
                .else
                    mov errno,EINVAL
                    xor eax,eax
                    mov _doserrno,eax
                    dec eax
                    .break
                .endif
            .endif
        .endif
        xor eax,eax
        .if ecx == O_RDONLY
            mov eax,FILE_SHARE_READ
        .endif
        _osopenW(path, ecx, eax, 0, A_CREATETRUNC, edx)
    .until 1
    ret

_wcreat endp

    END
