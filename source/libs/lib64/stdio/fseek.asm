; FSEEK.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include stdio.inc
include io.inc
include errno.inc
include winbase.inc

    .code

    option win64:rsp nosave
    assume rbx:LPFILE

fseek proc uses rsi rdi rbx fp:LPFILE, off:SIZE_T, whence:SIZE_T

    mov rbx,rcx ; fp
    mov rsi,rdx ; offset
    mov rdi,r8  ; whence

    mov eax,[rbx]._flag
    .if r8b != SEEK_SET && r8b != SEEK_CUR && r8b != SEEK_END \
         && !( eax & _IOREAD or _IOWRT or _IORW )

        _set_errno(EINVAL)
        .return -1
    .endif

    and eax,not _IOEOF
    mov [rbx]._flag,eax

    .if r8b == SEEK_CUR

        add rsi,ftell(rbx)
        mov edi,SEEK_SET
    .endif

    fflush(rbx)

    mov eax,[rbx]._flag
    .if eax & _IORW

        and eax,not (_IOWRT or _IOREAD)
        mov [rbx]._flag,eax

    .elseif eax & _IOREAD && eax & _IOMYBUF && !(eax & _IOSETVBUF)

        mov [rbx]._bufsiz,_MINIOBUF
    .endif

    mov eax,[rbx]._file
    lea r8,_osfhnd
    mov rcx,[r8+rax*8]
    .ifd SetFilePointer(rcx, esi, 0, edi) == -1

        .return _dosmaperr(GetLastError())
    .endif
    xor eax,eax
    ret

fseek endp

    END
