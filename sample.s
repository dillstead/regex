	@ r0 - r10 available	
        @ r0 - current location in input
        @ r1 - current char
        @ r2 - address in CLIST or XCHG to resume execution
        @ r3 - r8 - scratch
        @ lr - next instruction, calling into CNODE or NNODE
INIT:
        push    {r4-r8, lr}
        @ ensure first char is not 0 so GETCHA runs at least once
        mov     r1, #1
        b       XCHG

ADRNLIST:
        ldr     r7, MAXCCNT
        lsl     r7, r7, #4
        adr     r8, CLIST
        add     r5, r8, r7
        mov     pc, lr

GETCHA:
	ldrb	r1, [r0]
	add 	r0, r0, #1
        mov     pc, lr

XCHG:
        ldr     r3, NCNT
        mov     r4, r3
        bl      ADRNLIST
        @ r5 NLIST
        adr     r6, CLIST
        b       1f
2:
        ldr     r7, [r5, #12]
        str     r7, [r6, #12]
        @ copy from NLIST to CLIST
        add     r5, r5, #16
        add     r6, r6, #16
        add     r3, r3, #-1
1:      
        cmp     r3, #0
        bgt     2b
        @ copy XCHG to last entry in CLIST
        adr     r7, XCHG
        str     r7, [r6, #12]
        @ update list counts
        str     r3, NCNT
        str     r4, CCNT
	@ if current char is 0 and CLIST count 0, fail
	cmp	r1, #0
	cmpeq	r4, #0
	bne	3f
	@ fail
	mov 	r0, #0
        pop     {r4-r8, lr}
        bx	lr
3:
        @ get next character from input
        bl      GETCHA
        mov     r2, pc
        @ compile time patch
        b       CODE0
        b       CLIST
        
CNODE:
        @ r3 start of CLIST
        adr     r3, CLIST
	@ r4 contains cnt
	@ r5 contains NCNT
        mov     r4, #0
        ldr     r5, CCNT
	@ search CLIST for duplicates
1:
        ldr     r7, [r3, #12]
        cmp     r7, lr
        @ if value already exists, return
        beq     2f
        add     r3, r3, #16
        add     r4, r4, #1
        cmp     r4, r5
        bls     1b
        @ move EXCHG up and store new entry
        ldr     r7, [r3, #-4]
        str     lr, [r3, #-4]
        str     r7, [r3, #12]
        str     r4, CCNT
2:
        @ return CODE + 1
        add     lr, lr, #4
        mov     pc, lr
        
NNODE:
        mov     r6, lr
        ldr     r3, NCNT
        mov     r4, #0
        bl      ADRNLIST
        @ r3 NCNT
        @ r4 cnt
        @ r5 NLIST
        @ r6 saved lr
        b       1f
2:
        ldr     r7, [r5, #12]
        cmp     r7, r6
        @ if value already exists, return
        beq     3f
        add     r5, r5, #16
        add     r4, r4, #1
1:
        cmp     r4, r3
        blt     2b
        @ store
        str     r6, [r5, #12]
        @ inc NCNT
        add     r3, r3, #1
        str     r3, NCNT
3:
        @ return next inst CLIST
        mov     pc, r2

	@ compile time patch
MAXCCNT:
        .word   0x00000000

CCNT:
        .word   0x00000000
        
NCNT:
        .word   0x00000000
        
CLIST:  
        mov 	r2, pc
        add 	r2, r2, #8
        ldr 	pc, [pc, #-4]
        @ run time patch
        .word	0x00000000

        mov 	r2, pc
        add 	r2, r2, #8
        ldr 	pc, [pc, #-4]
        .word	0x00000000
        
NLIST:
        mov 	r2, pc
        add 	r2, r2, #8
        ldr 	pc, [pc, #-4]
        .word	0x00000000

        mov 	r2, pc
        add 	r2, r2, #8
        ldr 	pc, [pc, #-4]
        .word	0x00000000

CODE0:
        @ compile time patch
        b       CODE1
CODE1:	
        @ compile time patch
        cmp     r1, #'a'
	movne   pc, r2
        @ compile time patch
        bl      NNODE
	b	CODE16
CODE5:
	cmp     r1, #'b'
        movne   pc, r2
	bl	NNODE
	b	CODE16
CODE9:
	cmp     r1, #'c'
        movne   pc, r2
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
	cmp     r1, #'d'
	movne   pc, r2
	bl	NNODE
	@ success
        mov 	r0, #1
        pop    {r4-r8, lr}
	bx	lr
