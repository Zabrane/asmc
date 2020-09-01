; FABSF.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include math.inc

    .code

    option win64:rsp nosave noauto

fabsf proc x:float

    pcmpeqw xmm1,xmm1
    psrld   xmm1,1
    andps   xmm0,xmm1
    ret

fabsf endp

    end
