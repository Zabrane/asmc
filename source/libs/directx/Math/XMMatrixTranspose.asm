; XMMATRIXTRANSPOSE.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;
include DirectXMath.inc

    .code

    option win64:rsp nosave noauto

XMMatrixTranspose proc XM_CALLCONV XMTHISPTR, AXMMATRIX
if _XM_VECTORCALL_
    inl_XMMatrixTranspose()
else
    assume rcx:ptr XMMATRIX
    assume rdx:ptr XMMATRIX
    inl_XMMatrixTranspose([rcx],[rdx])
    mov rax,rcx
endif
    ret

XMMatrixTranspose endp

    end
