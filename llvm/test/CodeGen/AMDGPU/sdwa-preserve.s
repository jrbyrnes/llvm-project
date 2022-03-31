	.text
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	add_f16_u32_preserve            ; -- Begin function add_f16_u32_preserve
	.p2align	2
	.type	add_f16_u32_preserve,@function
add_f16_u32_preserve:                   ; @add_f16_u32_preserve
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	flat_load_dword v2, v[2:3]
	s_nop 0
	flat_load_dword v3, v[0:1]
	s_waitcnt vmcnt(0) lgkmcnt(0)
	v_mul_f32_sdwa v4, v3, v2 dst_sel:WORD_1 dst_unused:UNUSED_PAD src0_sel:BYTE_1 src1_sel:BYTE_3
	v_add_f16_sdwa v4, v3, v2 dst_sel:BYTE_1 dst_unused:UNUSED_PRESERVE src0_sel:WORD_0 src1_sel:WORD_1
	flat_store_dword v[0:1], v4
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end0:
	.size	add_f16_u32_preserve, .Lfunc_end0-add_f16_u32_preserve
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 60
; NumSgprs: 36
; NumVgprs: 5
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	sdwa_preserve_keep              ; -- Begin function sdwa_preserve_keep
	.p2align	2
	.type	sdwa_preserve_keep,@function
sdwa_preserve_keep:                     ; @sdwa_preserve_keep
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	flat_load_dword v4, v[0:1]
	flat_load_dword v5, v[2:3]
	s_waitcnt vmcnt(0) lgkmcnt(0)
	v_and_b32_e32 v2, 0xff, v4
	v_mov_b32_sdwa v2, v5 dst_sel:WORD_1 dst_unused:UNUSED_PRESERVE src0_sel:WORD_0
	flat_store_dword v[0:1], v2
	s_endpgm
.Lfunc_end1:
	.size	sdwa_preserve_keep, .Lfunc_end1-sdwa_preserve_keep
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 52
; NumSgprs: 4
; NumVgprs: 6
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	sdwa_preserve_remove            ; -- Begin function sdwa_preserve_remove
	.p2align	2
	.type	sdwa_preserve_remove,@function
sdwa_preserve_remove:                   ; @sdwa_preserve_remove
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	flat_load_dword v2, v[2:3]
	s_nop 0
	flat_load_dword v3, v[0:1]
	s_waitcnt vmcnt(0) lgkmcnt(0)
	v_mov_b32_sdwa v3, v2 dst_sel:WORD_1 dst_unused:UNUSED_PRESERVE src0_sel:WORD_0
	flat_store_dword v[0:1], v3
	s_endpgm
.Lfunc_end2:
	.size	sdwa_preserve_remove, .Lfunc_end2-sdwa_preserve_remove
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 4
; NumVgprs: 4
; ScratchSize: 0
; MemoryBound: 0
	.section	".note.GNU-stack"
	.amd_amdgpu_isa "amdgcn-unknown-linux-gnu-gfx900"
