	.arch armv8-a
	.file	"c_template.c"
	.text
	.align	2
	.global	add_two_nums
	.type	add_two_nums, %function
add_two_nums:
.LFB0:
	.cfi_startproc
	add	x0, x0, x1
	ret
	.cfi_endproc
.LFE0:
	.size	add_two_nums, .-add_two_nums
	.align	2
	.global	start
	.type	start, %function
start:
.LFB1:
	.cfi_startproc
	stp	x29, x30, [sp, -16]!
	.cfi_def_cfa_offset 16
	.cfi_offset 29, -16
	.cfi_offset 30, -8
	mov	x29, sp
	mov	x1, 1000
	mov	x0, 100
	bl	add_two_nums
	mov	x1, -1
	str	x0, [x1]
	ldp	x29, x30, [sp], 16
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE1:
	.size	start, .-start
	.ident	"GCC: (Ubuntu 13.2.0-23ubuntu4) 13.2.0"
	.section	.note.GNU-stack,"",@progbits
