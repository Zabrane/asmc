; G_XMMASKX16Y16.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include DirectXMath.inc

.data
g_XMMaskX16Y16 XMVECTORU32 { { { 0x0000FFFF, 0xFFFF0000, 0x00000000, 0x00000000 } } }

 end
