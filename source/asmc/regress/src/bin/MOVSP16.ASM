
	.286
	.model small
	.586p

	.code

	mov eax, cr0
	mov eax, cr2
	mov eax, cr3
	mov eax, cr4
	mov ecx, cr0
	mov cr0, eax
	mov cr2, eax
	mov cr3, eax
	mov cr4, eax
	mov cr0, ecx

	mov eax, dr0
	mov eax, dr1
	mov eax, dr2
	mov eax, dr3
	mov eax, dr6
	mov eax, dr7
	mov ecx, dr0
	mov dr0, eax
	mov dr1, eax
	mov dr2, eax
	mov dr3, eax
	mov dr6, eax
	mov dr7, eax
	mov dr0, ecx

	end

