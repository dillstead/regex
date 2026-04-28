	@ r0 - r10 available	
        @ r0 - current location in input
        @ r1 - current char
        @ r2 - address in CLIST or XCHG to resume execution
        @ r3 - save lr
        @ r4 - r10 - scratch
        @ lr - next instruction, calling into CNODE or NNODE

        @ setup specific registers as anchors in init
        @  Max 2^11 - 1 imm12

        @ todo use r4 below
        @ initialize r1

        @ CODE can only reach 2^11 - 1 bytes
        @ Any target farther away will fail

INIT:
        mov     r3, lr
        @      ensure first char is not 0 so GETCHA
        @      runs at least once
        mov     r1, #0
        b       XCHG

GETCHA:
        cmp     r1, #0
        moveq   r0, #0
        moveq   pc, r3
        ldr     r1, [r0]
        add     r0, r0, #1
        mov     pc, lr

XCHG:
        ldr     r4, NCNT
        mov     r5, r4
        adr     r6, NLIST
        adr     r7, CLIST
        b       1f
2:
        ldr     r8, [r6, #12]
        str     r8, [r7, #12]
        #      copy from NLIST to CLIST
        add     r6, r6, #16
        add     r7, r7, #16
        add     r4, r4, #-1
1:      
        cmp     r4, #0
        bgt     2b
        @      copy XCHG to last entry in CLIST
        adr     r8, XCHG
        str     r8, [r7, #12]
        @      update list counts
        str     r4, NCNT
        str     r5, CCNT
        @      get next character from input
        bl      GETCHA
        mov     r2, pc
        b       CODE0
        b       CLIST
        
CNODE:
        @ r4 start of CLIST
        adr     r4, CLIST
        #      r5 cnt
        #      r6 NCNT
        mov     r5, #0
        ldr     r6, CCNT
        b       1f
2:
        ldr     r8, [r4, #12]
        cmp     r8, lr
        @      if value already exists, return
        beq     3f
        add     r4, r4, #16
        add     r5, r5, #1
1:
        cmp     r5, r6
        blt     2b
        @      move EXCHG up and store
        ldr     r8, [r4, #-4]
        str     lr, [r4, #-4]
        str     r8, [r4, #12]
        @ inc CCNT
        add     r6, r6, #1
        str     r7, CCNT
3:
        @ return CODE + 1
        add     lr, lr, #1
        mov     pc, lr
        
NNODE:
        @      skip over CLIST
        ldr     r8, MAXCCNT
        lsl     r8, r8, #4
        adr     r9, CLIST
        add     r4, r9, r8
        @      r4 start of NLIST
        mov     r5, #0
        ldr     r6, NCNT
        @      r5 cnt
        @      r6 NCNT
        b       1f
2:
        ldr     r8, [r4, #12]
        cmp     r8, lr
        @      if value already exists, return
        beq     3f
        add     r4, r4, #16
        add     r5, r5, #1
1:
        cmp     r5, r6
        blt     2b
        @ store
        str     lr, [r4, #12]
        @ inc NCNT
        add     r6, r6, #1
        str     r6, NCNT
3:
        @ return next inst CLIST
        mov     pc, r2

	@ compile time patch
MAXCCNT:
        .word   0x0000000

CCNT:
        .word   0x00000000
        
NCNT:
        .word   0x00000000
        
CLIST:  
        mov 	r2, pc
        add 	r2, r2, #12
        ldr 	pc, [pc, #-4]
        @ run time patch
        .word	0x00000000

        mov 	r2, pc
        add 	r2, r2, #12
        ldr 	pc, [pc, #-4]
        .word	0x00000000
        
NLIST:
        mov 	r2, pc
        add 	r2, r2, #12
        ldr 	pc, [pc, #-4]
        .word	0x00000000

        mov 	r2, pc
        add 	r2, r2, #12
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
	cmp     r1, #'c'
	movne   pc, r2
	bl	NNODE
        mov 	 r0, #1
        mov  	 pc, r3	
