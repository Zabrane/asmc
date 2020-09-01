; TOWLOWER.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;
include ctype.inc
include winnls.inc

    .code

towlower proc char:wchar_t

    mov eax,ecx
    .return .if ( ax < 'A' )
    .if ( ax <= 'Z' )
        add ax,'a'-'A'
        .return
    .endif
    .return .if ( ax >= 'a' && ax <= 'z' )

if WINVER GE 0x0600
    LCMapStringEx(LOCALE_NAME_USER_DEFAULT,
        LCMAP_LOWERCASE, &char, 1, &char, 1, 0, 0, 0)
    movzx eax,char
endif
    ret

towlower endp

    end

