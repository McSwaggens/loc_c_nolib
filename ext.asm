section .text

global system_call
system_call:
	mov rax, rdi
	mov rdi, rsi
	mov rsi, rdx
	mov rdx, rcx
	mov r10, r8
	mov r8, r9
	mov r9, QWORD [rsp+8]
	syscall
	ret

