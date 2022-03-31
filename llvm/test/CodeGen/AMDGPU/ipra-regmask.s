	.text
	.amdgcn_target "amdgcn-amd-amdhsa--gfx700"
	.globl	csr                             ; -- Begin function csr
	.p2align	2
	.type	csr,@function
csr:                                    ; @csr
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_store_dword v44, off, s[0:3], s32 offset:4 ; 4-byte Folded Spill
	buffer_store_dword v45, off, s[0:3], s32 ; 4-byte Folded Spill
	;;#ASMSTART
	;;#ASMEND
	buffer_load_dword v45, off, s[0:3], s32 ; 4-byte Folded Reload
	buffer_load_dword v44, off, s[0:3], s32 offset:4 ; 4-byte Folded Reload
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end0:
	.size	csr, .Lfunc_end0-csr
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 44
; NumSgprs: 33
; NumVgprs: 46
; ScratchSize: 12
; MemoryBound: 0
	.text
	.globl	subregs_for_super               ; -- Begin function subregs_for_super
	.p2align	2
	.type	subregs_for_super,@function
subregs_for_super:                      ; @subregs_for_super
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	;;#ASMSTART
	;;#ASMEND
	s_setpc_b64 s[30:31]
.Lfunc_end1:
	.size	subregs_for_super, .Lfunc_end1-subregs_for_super
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 8
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	clobbered_reg_with_sub          ; -- Begin function clobbered_reg_with_sub
	.p2align	2
	.type	clobbered_reg_with_sub,@function
clobbered_reg_with_sub:                 ; @clobbered_reg_with_sub
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	;;#ASMSTART
	;;#ASMEND
	s_setpc_b64 s[30:31]
.Lfunc_end2:
	.size	clobbered_reg_with_sub, .Lfunc_end2-clobbered_reg_with_sub
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 8
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	nothing                         ; -- Begin function nothing
	.p2align	2
	.type	nothing,@function
nothing:                                ; @nothing
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end3:
	.size	nothing, .Lfunc_end3-nothing
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 8
; NumSgprs: 32
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	special_regs                    ; -- Begin function special_regs
	.p2align	2
	.type	special_regs,@function
special_regs:                           ; @special_regs
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	;;#ASMSTART
	;;#ASMEND
	s_setpc_b64 s[30:31]
.Lfunc_end4:
	.size	special_regs, .Lfunc_end4-special_regs
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 8
; NumSgprs: 32
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	vcc                             ; -- Begin function vcc
	.p2align	2
	.type	vcc,@function
vcc:                                    ; @vcc
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	;;#ASMSTART
	;;#ASMEND
	s_setpc_b64 s[30:31]
.Lfunc_end5:
	.size	vcc, .Lfunc_end5-vcc
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 8
; NumSgprs: 34
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.no_dead_strip	csr
	.no_dead_strip	subregs_for_super
	.no_dead_strip	clobbered_reg_with_sub
	.no_dead_strip	nothing
	.no_dead_strip	special_regs
	.no_dead_strip	vcc
	.section	".note.GNU-stack"
	.amdgpu_metadata
---
amdhsa.kernels:  []
amdhsa.target:   amdgcn-amd-amdhsa--gfx700
amdhsa.version:
  - 1
  - 1
...

	.end_amdgpu_metadata
