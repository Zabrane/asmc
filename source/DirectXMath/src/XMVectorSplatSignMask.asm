
include DirectXMath.inc

    .code

    option win64:rsp nosave noauto

XMVectorSplatSignMask proc XM_CALLCONV

    mov eax,0x80000000
    movd xmm0,eax
    XM_PERMUTE_PS()
    ret

XMVectorSplatSignMask endp

    end
