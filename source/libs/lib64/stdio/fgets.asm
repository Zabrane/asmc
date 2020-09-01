; FGETS.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include stdio.inc

    .code

    assume rbx:ptr _iobuf

fgets proc uses rsi rdi rbx buf:LPSTR, count:SINT, fp:LPFILE

    mov rdi,rcx
    mov rsi,rdx
    mov rbx,r8

    xor eax,eax
    cmp rsi,rax
    .return .ifng

    dec rsi
    .ifnz
        .repeat
            dec [rbx]._cnt
            .ifl
                .ifd _filbuf(rbx) == -1
                    .break .if rdi != buf
                    .return 0
                .endif
            .else
                mov rcx,[rbx]._ptr
                inc [rbx]._ptr
                mov al,[rcx]
            .endif
            stosb
            .break .if al == 10
            dec rsi
        .untilz
    .endif
    mov byte ptr [rdi],0
    mov rax,buf
    ret

fgets endp

    END
