        @gcc -Werror -std=gnu99 -fno-builtin -Wall -Wextra -nostdlib -mgeneral-regs-only -fno-diagnostics-color -fpie -O2 -o sample sample.s
        @ INIT(str, length)
        @ str must be NULL-terminated, length must include NULL-terminator
        @ r0 - str
        @ r1 - length
        @ r2 - current char
        @ r3 - address in CLIST or XCHG to resume execution
        @ r4 - r9 - scratch
        @ r10 - start position of the current thread, loaded by its list entry
        @ r11 - current position in the input, incremented by GETCHA
        @ r12 - best match
        @ lr - next instruction, calling into CNODE or NNODE
INIT:
        push    {r4-r12, lr}
        mvn     r12, #0
        mov     r11, #0
	@ NCNT might be stale from previous call, zero it
        mov     r4, #0
        str     r4, NCNT
        b       XCHG

ADRNLIST:
        ldr     r8, MAXCCNT
        lsl     r8, r8, #5
        adr     r9, CLIST
        add     r4, r9, r8
        mov     pc, lr

GETCHA:
	ldrb	r2, [r0]
	add 	r0, r0, #1
        sub 	r1, r1, #1
        add     r11, r11, #1
        mov     pc, lr

XCHG:
	@ r4 start of NLIST
	@ r5 end of NLIST
        @ r6 NCNT
	@ r7 start of CLIST
	bl      ADRNLIST
        ldr     r6, NCNT
        lsl     r5, r6, #5
        add     r5, r4, r5
	adr     r7, CLIST
	@ no more NNODES, done
	cmp	r11, #0
	beq	2f
	cmp	r6, #0
	beq	4f
	@ copy NNODES to CLIST
	b	2f
1:
        ldr     r8, [r4, #16]
        cmp     r8, r7
        beq     3f
        add     r4, r4, #32

	ldr     r8, [r4, #16]
        str     r8, [r7, #16]
        ldr     r8, [r4, #20]
        str     r8, [r7, #20]
        add     r4, r4, #32
        add     r7, r7, #32
2:
        cmp     r4, r5
        blo     1b
	@ if no match found, copy START to CLIST
	cmn	r12, #1
	bne	3
	adr     r8, DONE
        str     r8, [r7, #16]
	@ current input position
	str     r11, [r7, #20]
	add	r6, r6, #1
	add     r7, r7, #32
3:
	@ copy XCHG to CLIST
	adr     r8, XCHG
        str     r8, [r7, #16]
        @ update list counts
	str     r6, CLIDX
	mov	r6, #0
        str     r6, NCNT
	@ if end of input fail
	cmp	r1, #0
	bgt	5f
4:
	@ done, return best match (or none)
	mov 	r0, r12
        pop     {r4-r12, lr}
        bx	lr
5:
        @ get next character from input
        bl      GETCHA
        @ compile time patch
        b       CLIST

START:
	b	CODE0
        
CNODE:
        @ r4 start of CLIST
        @ r6 CLIDX
        adr     r4, CLIST
        ldr     r6, CLIDX
        add     r5, r6, #1
        lsl     r5, r5, #5
        @ r5 one beyond end of CLIST
        add     r5, r4, r5

        @ check for duplicates
1:
        ldr     r8, [r4, #16]
        cmp     r8, lr
        beq     3f
        add     r4, r4, #32
        cmp     r4, r5
        blo     1b
        @ store new entry to run next
2:
        ldr     r8, [r5, #-16]
        str     r8, [r5, #16]
        ldr     r8, [r5, #-12]
        str     r8, [r5, #20]
        sub     r5, r5, #32
        @ stop at r3 and store
        cmp     r5, r3
        bhi     2b
        str     lr, [r3, #16]
        str     r10, [r3, #20]
        @ inc CLIDX
        add     r6, r6, #1
        str     r6, CLIDX
3:
        @ return CODE + 1
        add     lr, lr, #4
        mov     pc, lr
        
NNODE:
        @ r4 start of NLIST
	@ r5 end of NLIST
        @ r6 NCNT
        @ r7 stored lr
        mov     r7, lr
        bl      ADRNLIST
        ldr     r6, NCNT
        lsl     r5, r6, #5
        add     r5, r4, r5
        @ check for duplicates
        b       2f
1:
        ldr     r8, [r4, #16]
        cmp     r8, r7
        beq     3f
        add     r4, r4, #32
2:
        cmp     r4, r5
        blo     1b
        @ store target and start
        str     r7, [r5, #16]
        str     r10, [r5, #20]
        @ inc NCNT
        add     r6, r6, #1
        str     r6, NCNT
3:
        @ return next inst CLIST
        mov     pc, r3

	@ compile time patch
MAXCCNT:
        .word   0x00000000

        @ index of last entry in CLIST
CLIDX:
        .word   0x00000000
        @ number of entries in NLIST
NCNT:
        .word   0x00000000
        
CLIST:  
        mov 	r3, pc
        add 	r3, r3, #24
        ldr     r10, [pc, #4]
        ldr 	pc, [pc, #-4]
        @ run time patch: target
        .word	0x00000000
        @ run time patch: start
        .word	0x00000000
        @ pad to 32 bytes
        .word	0x00000000
        .word	0x00000000

        mov 	r3, pc
        add 	r3, r3, #24
        ldr     r10, [pc, #4]
        ldr 	pc, [pc, #-4]
        .word	0x00000000
        .word	0x00000000
        .word	0x00000000
        .word	0x00000000
        
NLIST:
        mov 	r3, pc
        add 	r3, r3, #24
        ldr     r10, [pc, #4]
        ldr 	pc, [pc, #-4]
        .word	0x00000000
        .word	0x00000000
        .word	0x00000000
        .word	0x00000000

        mov 	r3, pc
        add 	r3, r3, #24
        ldr     r10, [pc, #4]
        ldr 	pc, [pc, #-4]
        .word	0x00000000
        .word	0x00000000
        .word	0x00000000
        .word	0x00000000

CODE0:
        @ compile time patch
        b       CODE1
CODE1:	
        @ compile time patch
        cmp     r2, #'a'
	movne   pc, r3
        @ compile time patch
        bl      NNODE
	b	CODE16
CODE5:
	cmp     r2, #'b'
        movne   pc, r3
	bl	NNODE
	b	CODE16
CODE9:
	cmp     r2, #'c'
        movne   pc, r3
	bl	NNODE
	b	CODE16
CODE13:
	bl	CNODE
	b	CODE9
	b	CODE5
CODE16:	
	bl	CNODE
	b	CODE13
	b	CODE19
CODE19:
	cmp     r2, #'d'
	movne   pc, r3
	bl	NNODE
	@ match
        cmp 	r10, r12
        movlo   r12, r10
        mov     pc, r3

