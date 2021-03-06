; XMMATRIXROTATIONZ.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;
include DirectXMath.inc

    .code

    option win64:rsp nosave noauto

XMMatrixRotationZ proc XM_CALLCONV XMTHISPTR, Scale:float
if _XM_VECTORCALL_ eq 0
    _mm_store_ps(xmm0, xmm1)
endif
    inl_XMScalarSinCos(xmm0)
    _mm_unpacklo_ps(xmm1, xmm0)
    _mm_store_ps(xmm0, xmm1)
    XM_PERMUTE_PS(xmm1, _MM_SHUFFLE(3,2,0,1))
    _mm_mul_ps(xmm1, g_XMNegateX)
    _mm_store_ps(xmm2, g_XMIdentityR2)
    _mm_store_ps(xmm3, g_XMIdentityR3)
if _XM_VECTORCALL_ eq 0
    _mm_store_ps([rcx+0*16], xmm0)
    _mm_store_ps([rcx+1*16], xmm1)
    _mm_store_ps([rcx+2*16], xmm2)
    _mm_store_ps([rcx+3*16], xmm3)
    mov rax,rcx
endif
    ret

XMMatrixRotationZ endp

    end
