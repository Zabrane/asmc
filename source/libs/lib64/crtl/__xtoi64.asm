; __XTOI64.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;
; long __xtoi64(const char *);
;
; Change history:
; 2017-10-18 - created
;

    .code

__xtoi64::

    xor eax,eax
    xor edx,edx

    .while 1

        mov dl,[rcx]
        inc rcx
        and dl,0xDF

        .break .if dl < 0x10
        .break .if dl > 'F'

        .if dl > 0x19

            .break .if dl < 'A'
            sub dl,'A' - 0x1A

        .endif

        sub dl,0x10
        shl rax,4
        add rax,rdx

    .endw
    ret

    end
