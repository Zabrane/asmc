; FREAD.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include stdio.inc
include io.inc
include errno.inc
include string.inc

    .code

    assume ebx:ptr _iobuf

fread proc uses esi edi ebx buf:LPSTR, rsize:SINT, num:SINT, fp:LPFILE

  local total:SIZE_T, bufsize:SIZE_T, nbytes:SIZE_T

    mov edi,buf
    mov eax,rsize
    mov ecx,num
    mov ebx,fp
    mul ecx
    mov total,eax
    mov esi,eax

    test eax,eax
    jz  toend

    mov edx,_MAXIOBUF
    .if [ebx]._flag & _IOMYBUF or _IONBF or _IOYOURBUF

        mov edx,[ebx]._bufsiz   ; already has buffer, use its size
    .endif
    mov bufsize,edx

    .while esi

        mov edx,[ebx]._cnt
        .if [ebx]._flag & _IOMYBUF or _IOYOURBUF && edx

            .if esi < edx

                mov edx,esi
            .endif

            mov eax,esi
            mov ecx,edx
            mov esi,[ebx]._ptr
            rep movsb
            mov [ebx]._ptr,esi
            mov esi,eax
            sub esi,edx
            sub [ebx]._cnt,edx

        .elseif esi >= bufsize

            mov eax,esi
            mov ecx,bufsize
            .if ecx

                xor edx,edx
                div ecx
                mov eax,esi
                sub eax,edx
            .endif
            mov nbytes,eax

            .if !_read( [ebx]._file, edi, eax )

                jmp error

            .elseif eax == -1

                jmp error
            .endif

            sub esi,eax
            add edi,eax

        .else
            .if _filbuf(ebx) == -1

                jmp break
            .endif

            mov [edi],al
            inc edi
            dec esi
            mov eax,[ebx]._bufsiz
            mov bufsize,eax
        .endif
    .endw
    mov eax,num
toend:
    ret
error:
    or  [ebx]._flag,_IOEOF
break:
    mov eax,total
    sub eax,esi
    xor edx,edx
    div rsize
    jmp toend
fread endp

    END
