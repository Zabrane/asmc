; XMVECTORFLOOR.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;
include DirectXMath.inc

    .code

    option win64:rsp nosave noauto

XMVectorFloor proc XM_CALLCONV V:FXMVECTOR

    inl_XMVectorFloor(xmm0)
    ret

XMVectorFloor endp

    end
