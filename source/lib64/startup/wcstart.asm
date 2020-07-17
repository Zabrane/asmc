; WCSTART.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;
; Startup module for LIBC
;
include stdlib.inc

externdef __xi_a:ptr
externdef __xi_z:ptr

wmain	proto __cdecl :dword, :ptr, :ptr
_initterm proto __cdecl :ptr, :ptr

public __ImageBase

	.code
	org -0x1050
	__ImageBase label byte
	org 0

	dd 495A440Ah
	dd 564A4A50h
	db __LIBC__ / 100 + '0','.',__LIBC__ mod 100 / 10 + '0',__LIBC__ mod 10 + '0'

wcstart::

wmainCRTStartup proc frame

	_initterm( &__xi_a, &__xi_z )
	exit( wmain( __argc, __wargv, _wenviron ) )

wmainCRTStartup endp

	end wcstart
