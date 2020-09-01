; STRINGVPRINTFWORKERW.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include stdio.inc
include strsafe.inc

.code

StringVPrintfWorkerW proc pszDest:STRSAFE_LPWSTR, cchDest:size_t, pcchNewDestLength:ptr size_t,
         pszFormat:STRSAFE_LPCWSTR, argList:va_list

  local hr:HRESULT
  local retval:SINT
  local cchMax:size_t
  local cchNewDestLength:size_t

    mov hr,S_OK
    mov cchNewDestLength,0

    ;; leave the last space for the null terminator
    lea rax,[rdx-2]
    mov cchMax,rax

if (STRSAFE_USE_SECURE_CRT eq 1) and not defined(STRSAFE_LIB_IMPL)
    mov retval,_vsnwprintf_s(pszDest, cchDest, cchMax, pszFormat, argList)
else
    mov retval,_vsnwprintf(pszDest, cchMax, pszFormat, argList)
endif
    ;; ASSERT((iRet < 0) || (((size_t)iRet) <= cchMax));

    .if ((retval < 0) || (rax > cchMax))

        ;; need to null terminate the string
        mov rcx,pszDest
        mov rax,cchMax
        mov word ptr [rcx+rax*2],0

        mov cchNewDestLength,rax

        ;; we have truncated pszDest
        mov hr,STRSAFE_E_INSUFFICIENT_BUFFER

    .elseif (rax == cchMax)

        ;; need to null terminate the string
        mov rcx,pszDest
        mov rax,cchMax
        mov word ptr [rcx+rax*2],0

        mov cchNewDestLength,rax

    .else

        mov cchNewDestLength,rax
    .endif

    mov rcx,pcchNewDestLength
    mov rax,cchNewDestLength
    .if (rcx)

        mov [rcx],rax
    .endif

    mov eax,hr
    ret

StringVPrintfWorkerW endp

    end
