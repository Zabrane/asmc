; TRESOURCE.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include twindow.inc

    .code

    assume rcx:window_t
    assume rbx:window_t
    assume rsi:robject_t

TWindow::Resource proc uses rsi rdi rbx r12 rcx p:resource_t

    lea     rsi,[rdx].TResource.dialog
    movzx   r8d,[rsi].flags
    and     r8d,W_MOVEABLE or W_SHADE or W_COLOR
    mov     rbx,[rcx].Open([rsi].rc, r8d)
    .return .if !rax

    [rbx].Load(&[rsi-2])
    movzx   edi,[rsi].count
    movzx   eax,[rsi].index
    lea     rsi,[rsi+TRObject]
    inc     eax
    mov     [rbx].Index,eax

    .for ( r12d = 1 : edi : edi--, rsi += TRObject, r12d++ )

        mov     r9w,[rsi].flags
        and     r9d,0x0F
        inc     r9d
        mov     rcx,[rbx].Child([rsi].rc, r12d, r9d)
        movzx   eax,[rsi].flags
        and     eax,0xFFF0
        shl     eax,4
        or      [rcx].Flags,eax
    .endf
    mov rax,rbx
    ret

TWindow::Resource endp

TWindow::Load proc uses rsi rdi rbx p:resource_t

    xor eax,eax
    mov rbx,rcx
    lea rsi,[rdx].TResource.dialog
    mov al,[rsi].count
    lea rsi,[rsi+rax*8+8]
    mov r10,[rbx].Color
    mov rdi,[rbx].Window
    add rdi,2
    .if ( [rbx].Flags & W_COLOR )
        UnzipAttrib()
    .else
        UnzipChar()
    .endif
    mov rdi,[rbx].Window
    UnzipChar()
    mov rcx,rbx
    ret

TWindow::Load endp

    assume rsi:nothing
    assume rcx:nothing

UnzipChar:
    movzx eax,[rbx].rc.col
    mul [rbx].rc.row
    mov ecx,eax
    xor eax,eax
    .repeat
        mov al,[rsi]
        mov dl,[rsi]
        add rsi,1
        and dl,0xF0
        .if dl == 0xF0
            mov dh,al
            mov dl,[rsi]
            mov al,[rsi+1]
            add rsi,2
            and edx,0x0FFF
            .repeat
                mov [rdi],ax
                add rdi,4
                dec edx
                .break .ifz
            .untilcxz
            .break .if !ecx
        .else
            mov [rdi],ax
            add rdi,4
        .endif
    .untilcxz
    ret

UnzipAttrib:
    movzx eax,[rbx].rc.col
    mul [rbx].rc.row
    mov ecx,eax
    xor eax,eax
    .repeat
        mov al,[rsi]
        mov r9b,al
        add rsi,1
        and r9b,0xF0
        xor edx,edx
        .if r9b == 0xF0
            mov dh,al
            mov dl,[rsi]
            mov al,[rsi+1]
            add rsi,2
            and edx,0x0FFF
        .endif
        mov r8d,eax
        and eax,0xF0
        shr eax,4
        and r8d,0x0F
        mov al,[r10+rax+16]
        or  al,[r10+r8]
        .if r9b == 0xF0
            .repeat
                mov [rdi],ax
                add rdi,4
                dec edx
                .break .ifz
            .untilcxz
            .break .if !ecx
        .else
            mov [rdi],ax
            add rdi,4
        .endif
    .untilcxz
    ret

    end
