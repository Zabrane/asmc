; TANF.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include math.inc

    .code

    option win64:rsp nosave noauto

tanf proc x:float

    cvtss2sd xmm0,xmm0
    tan(xmm0)
    cvtsd2ss xmm0,xmm0
    ret

tanf endp

    end
