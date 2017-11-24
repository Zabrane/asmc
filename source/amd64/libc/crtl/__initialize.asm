include crtl.inc

    .code

    option win64:rsp nosave

__initialize proc uses rsi rdi rbx offset_start:PVOID, offset_end:PVOID

    mov rax,rdx
    mov rdx,rcx
    sub rax,rdx

    .ifnz

        mov rsi,rdx ; start of segment
        mov rdi,rdx
        add rdi,rax
        .while  1
            xor rax,rax
            mov rcx,256
            mov rbx,rsi
            mov rdx,rdi
            .while rdi != rbx
                .if [rbx] != rax && rcx >= [rbx+8]

                    mov rcx,[rbx+8]
                    mov rdx,rbx
                .endif
                add rbx,16
            .endw
            .break .if rdx == rdi
            mov rbx,[rdx]
            mov [rdx],rax
            rbx()
        .endw
    .endif
    ret

__initialize endp

    end
