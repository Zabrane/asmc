; __CVTQ_SD.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;
; __cvtq_sd() - Quad to double
;

include quadmath.inc
include errno.inc

    .code

__cvtq_sd proc frame x:ptr, q:ptr

    cmp     rdx,rcx
    mov     rax,[rdx+6]
    movzx   ecx,word ptr [rdx+14]

    .ifz
        xor r8d,r8d
        mov [rdx+8],r8
    .endif

    mov     r10,rax
    mov     r8d,ecx
    and     r8d,Q_EXPMASK
    mov     r9d,r8d
    neg     r8d
    rcr     rax,1
    mov     rdx,rax
    shr     rdx,32
    mov     r8d,eax
    shl     r8d,22

    .ifc
        .ifz
            add r8d,r8d
        .endif
        add eax,0x0800
        adc edx,0
        .ifc
            mov edx,0x80000000
            inc cx
        .endif
    .endif

    and eax,0xFFFFF800
    mov r8d,ecx
    and cx,0x7FFF
    add cx,0x03FF-0x3FFF

    .if cx < 0x07FF

        .if !cx

            shrd eax,edx,12
            shl  edx,1
            shr  edx,12
        .else
            shrd eax,edx,11
            shl  edx,1
            shrd edx,ecx,11
        .endif

        shl r8w,1
        rcr edx,1

    .else

        .if cx >= 0xC400

            .ifs cx >= -52

                mov r8d,0xFFFFF800
                sub cx,12
                neg cx

                .if cl >= 32

                    sub cl,32
                    mov r8d,eax
                    mov eax,edx
                    xor edx,edx
                .endif

                shrd r8d,eax,cl
                shrd eax,edx,cl
                shr  edx,cl
                add  r8d,r8d
                adc  eax,0
                adc  edx,0
            .else

                xor eax,eax
                xor edx,edx
                shl r8d,17
                rcr edx,1
            .endif

        .else

            shrd eax,edx,11
            shl edx,1
            shr edx,11
            shl r8w,1
            rcr edx,1
            or  edx,0x7FF00000
        .endif
    .endif

    xor ecx,ecx
    .if r9d < 0x3BCC

        .if ( r10 || r9d )

            xor eax,eax
            xor edx,edx
            mov ecx,ERANGE
        .endif

    .elseif r9d >= 0x3BCD

        mov r9d,edx
        and r9d,0x7FF00000
        mov ecx,ERANGE
        .ifnz
            .if r9d != 0x7FF00000
                xor ecx,ecx
            .endif
        .endif
    .elseif r9d >= 0x3BCC

        mov r9d,edx
        or  r9d,eax
        mov ecx,ERANGE

        .ifnz
            mov r9d,edx
            and r9d,0x7FF00000
            .ifnz
                xor ecx,ecx
            .endif
        .endif
    .endif

    shl rdx,32
    or  rdx,rax
    .if ecx

        _set_errno(ecx)
    .endif
    mov rax,x
    mov [rax],rdx
    ret

__cvtq_sd endp

    end
