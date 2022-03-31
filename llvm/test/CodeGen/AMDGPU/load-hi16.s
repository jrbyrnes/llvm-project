	.text
	.amdgcn_target "amdgcn-amd-amdhsa--gfx700"
	.globl	load_local_lo_hi_v2i16_multi_use_lo ; -- Begin function load_local_lo_hi_v2i16_multi_use_lo
	.p2align	2
	.type	load_local_lo_hi_v2i16_multi_use_lo,@function
load_local_lo_hi_v2i16_multi_use_lo:    ; @load_local_lo_hi_v2i16_multi_use_lo
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v2, v0
	ds_read_u16 v1, v0 offset:16
	v_mov_b32_e32 v0, 0
	s_waitcnt lgkmcnt(1)
	ds_write_b16 v0, v2
	v_mov_b32_e32 v0, v2
	s_waitcnt lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end0:
	.size	load_local_lo_hi_v2i16_multi_use_lo, .Lfunc_end0-load_local_lo_hi_v2i16_multi_use_lo
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 52
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_lo_hi_v2i16_multi_use_hi ; -- Begin function load_local_lo_hi_v2i16_multi_use_hi
	.p2align	2
	.type	load_local_lo_hi_v2i16_multi_use_hi,@function
load_local_lo_hi_v2i16_multi_use_hi:    ; @load_local_lo_hi_v2i16_multi_use_hi
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v1, v0 offset:16
	ds_read_u16 v0, v0
	v_mov_b32_e32 v2, 0
	s_waitcnt lgkmcnt(1)
	ds_write_b16 v2, v1
	s_waitcnt lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end1:
	.size	load_local_lo_hi_v2i16_multi_use_hi, .Lfunc_end1-load_local_lo_hi_v2i16_multi_use_hi
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_lo_hi_v2i16_multi_use_lohi ; -- Begin function load_local_lo_hi_v2i16_multi_use_lohi
	.p2align	2
	.type	load_local_lo_hi_v2i16_multi_use_lohi,@function
load_local_lo_hi_v2i16_multi_use_lohi:  ; @load_local_lo_hi_v2i16_multi_use_lohi
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v3, v0
	ds_read_u16 v4, v0 offset:16
	s_waitcnt lgkmcnt(1)
	ds_write_b16 v1, v3
	s_waitcnt lgkmcnt(1)
	ds_write_b16 v2, v4
	v_mov_b32_e32 v0, v3
	v_mov_b32_e32 v1, v4
	s_waitcnt lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end2:
	.size	load_local_lo_hi_v2i16_multi_use_lohi, .Lfunc_end2-load_local_lo_hi_v2i16_multi_use_lohi
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 64
; NumSgprs: 32
; NumVgprs: 5
; ScratchSize: 0
; MemoryBound: 1
	.text
	.globl	load_local_hi_v2i16_undeflo     ; -- Begin function load_local_hi_v2i16_undeflo
	.p2align	2
	.type	load_local_hi_v2i16_undeflo,@function
load_local_hi_v2i16_undeflo:            ; @load_local_hi_v2i16_undeflo
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v1, v0
	s_waitcnt lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end3:
	.size	load_local_hi_v2i16_undeflo, .Lfunc_end3-load_local_hi_v2i16_undeflo
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 24
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2i16_reglo       ; -- Begin function load_local_hi_v2i16_reglo
	.p2align	2
	.type	load_local_hi_v2i16_reglo,@function
load_local_hi_v2i16_reglo:              ; @load_local_hi_v2i16_reglo
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v2, v0
	v_mov_b32_e32 v0, v1
	s_waitcnt lgkmcnt(0)
	v_mov_b32_e32 v1, v2
	s_setpc_b64 s[30:31]
.Lfunc_end4:
	.size	load_local_hi_v2i16_reglo, .Lfunc_end4-load_local_hi_v2i16_reglo
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 32
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2i16_reglo_vreg  ; -- Begin function load_local_hi_v2i16_reglo_vreg
	.p2align	2
	.type	load_local_hi_v2i16_reglo_vreg,@function
load_local_hi_v2i16_reglo_vreg:         ; @load_local_hi_v2i16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v0, v0
	v_and_b32_e32 v1, 0xffff, v1
	s_waitcnt lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end5:
	.size	load_local_hi_v2i16_reglo_vreg, .Lfunc_end5-load_local_hi_v2i16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 52
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2i16_zerolo      ; -- Begin function load_local_hi_v2i16_zerolo
	.p2align	2
	.type	load_local_hi_v2i16_zerolo,@function
load_local_hi_v2i16_zerolo:             ; @load_local_hi_v2i16_zerolo
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v1, v0
	v_mov_b32_e32 v0, 0
	s_waitcnt lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end6:
	.size	load_local_hi_v2i16_zerolo, .Lfunc_end6-load_local_hi_v2i16_zerolo
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 28
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2i16_zerolo_shift ; -- Begin function load_local_hi_v2i16_zerolo_shift
	.p2align	2
	.type	load_local_hi_v2i16_zerolo_shift,@function
load_local_hi_v2i16_zerolo_shift:       ; @load_local_hi_v2i16_zerolo_shift
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v0, v0
	s_waitcnt lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	s_setpc_b64 s[30:31]
.Lfunc_end7:
	.size	load_local_hi_v2i16_zerolo_shift, .Lfunc_end7-load_local_hi_v2i16_zerolo_shift
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 28
; NumSgprs: 32
; NumVgprs: 1
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2f16_reglo_vreg  ; -- Begin function load_local_hi_v2f16_reglo_vreg
	.p2align	2
	.type	load_local_hi_v2f16_reglo_vreg,@function
load_local_hi_v2f16_reglo_vreg:         ; @load_local_hi_v2f16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v0, v0
	v_cvt_f16_f32_e32 v1, v1
	s_waitcnt lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end8:
	.size	load_local_hi_v2f16_reglo_vreg, .Lfunc_end8-load_local_hi_v2f16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2i16_reglo_vreg_zexti8 ; -- Begin function load_local_hi_v2i16_reglo_vreg_zexti8
	.p2align	2
	.type	load_local_hi_v2i16_reglo_vreg_zexti8,@function
load_local_hi_v2i16_reglo_vreg_zexti8:  ; @load_local_hi_v2i16_reglo_vreg_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u8 v0, v0
	v_and_b32_e32 v1, 0xffff, v1
	s_waitcnt lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end9:
	.size	load_local_hi_v2i16_reglo_vreg_zexti8, .Lfunc_end9-load_local_hi_v2i16_reglo_vreg_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 52
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2i16_reglo_vreg_sexti8 ; -- Begin function load_local_hi_v2i16_reglo_vreg_sexti8
	.p2align	2
	.type	load_local_hi_v2i16_reglo_vreg_sexti8,@function
load_local_hi_v2i16_reglo_vreg_sexti8:  ; @load_local_hi_v2i16_reglo_vreg_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_i8 v0, v0
	v_and_b32_e32 v1, 0xffff, v1
	s_waitcnt lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end10:
	.size	load_local_hi_v2i16_reglo_vreg_sexti8, .Lfunc_end10-load_local_hi_v2i16_reglo_vreg_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 52
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2f16_reglo_vreg_zexti8 ; -- Begin function load_local_hi_v2f16_reglo_vreg_zexti8
	.p2align	2
	.type	load_local_hi_v2f16_reglo_vreg_zexti8,@function
load_local_hi_v2f16_reglo_vreg_zexti8:  ; @load_local_hi_v2f16_reglo_vreg_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u8 v0, v0
	v_cvt_f16_f32_e32 v1, v1
	s_waitcnt lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end11:
	.size	load_local_hi_v2f16_reglo_vreg_zexti8, .Lfunc_end11-load_local_hi_v2f16_reglo_vreg_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2f16_reglo_vreg_sexti8 ; -- Begin function load_local_hi_v2f16_reglo_vreg_sexti8
	.p2align	2
	.type	load_local_hi_v2f16_reglo_vreg_sexti8,@function
load_local_hi_v2f16_reglo_vreg_sexti8:  ; @load_local_hi_v2f16_reglo_vreg_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_i8 v0, v0
	v_cvt_f16_f32_e32 v1, v1
	s_waitcnt lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end12:
	.size	load_local_hi_v2f16_reglo_vreg_sexti8, .Lfunc_end12-load_local_hi_v2f16_reglo_vreg_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_global_hi_v2i16_reglo_vreg ; -- Begin function load_global_hi_v2i16_reglo_vreg
	.p2align	2
	.type	load_global_hi_v2i16_reglo_vreg,@function
load_global_hi_v2i16_reglo_vreg:        ; @load_global_hi_v2i16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff002, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_ushort v0, v[0:1]
	v_and_b32_e32 v1, 0xffff, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end13:
	.size	load_global_hi_v2i16_reglo_vreg, .Lfunc_end13-load_global_hi_v2i16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 60
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_global_hi_v2f16_reglo_vreg ; -- Begin function load_global_hi_v2f16_reglo_vreg
	.p2align	2
	.type	load_global_hi_v2f16_reglo_vreg,@function
load_global_hi_v2f16_reglo_vreg:        ; @load_global_hi_v2f16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff002, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_ushort v0, v[0:1]
	v_cvt_f16_f32_e32 v1, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end14:
	.size	load_global_hi_v2f16_reglo_vreg, .Lfunc_end14-load_global_hi_v2f16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 56
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_global_hi_v2i16_reglo_vreg_zexti8 ; -- Begin function load_global_hi_v2i16_reglo_vreg_zexti8
	.p2align	2
	.type	load_global_hi_v2i16_reglo_vreg_zexti8,@function
load_global_hi_v2i16_reglo_vreg_zexti8: ; @load_global_hi_v2i16_reglo_vreg_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff001, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_ubyte v0, v[0:1]
	v_and_b32_e32 v1, 0xffff, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end15:
	.size	load_global_hi_v2i16_reglo_vreg_zexti8, .Lfunc_end15-load_global_hi_v2i16_reglo_vreg_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 60
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_global_hi_v2i16_reglo_vreg_sexti8 ; -- Begin function load_global_hi_v2i16_reglo_vreg_sexti8
	.p2align	2
	.type	load_global_hi_v2i16_reglo_vreg_sexti8,@function
load_global_hi_v2i16_reglo_vreg_sexti8: ; @load_global_hi_v2i16_reglo_vreg_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff001, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_sbyte v0, v[0:1]
	v_and_b32_e32 v1, 0xffff, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end16:
	.size	load_global_hi_v2i16_reglo_vreg_sexti8, .Lfunc_end16-load_global_hi_v2i16_reglo_vreg_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 60
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_global_hi_v2f16_reglo_vreg_sexti8 ; -- Begin function load_global_hi_v2f16_reglo_vreg_sexti8
	.p2align	2
	.type	load_global_hi_v2f16_reglo_vreg_sexti8,@function
load_global_hi_v2f16_reglo_vreg_sexti8: ; @load_global_hi_v2f16_reglo_vreg_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff001, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_sbyte v0, v[0:1]
	v_cvt_f16_f32_e32 v1, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end17:
	.size	load_global_hi_v2f16_reglo_vreg_sexti8, .Lfunc_end17-load_global_hi_v2f16_reglo_vreg_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 56
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_global_hi_v2f16_reglo_vreg_zexti8 ; -- Begin function load_global_hi_v2f16_reglo_vreg_zexti8
	.p2align	2
	.type	load_global_hi_v2f16_reglo_vreg_zexti8,@function
load_global_hi_v2f16_reglo_vreg_zexti8: ; @load_global_hi_v2f16_reglo_vreg_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff001, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_ubyte v0, v[0:1]
	v_cvt_f16_f32_e32 v1, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end18:
	.size	load_global_hi_v2f16_reglo_vreg_zexti8, .Lfunc_end18-load_global_hi_v2f16_reglo_vreg_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 56
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_flat_hi_v2i16_reglo_vreg   ; -- Begin function load_flat_hi_v2i16_reglo_vreg
	.p2align	2
	.type	load_flat_hi_v2i16_reglo_vreg,@function
load_flat_hi_v2i16_reglo_vreg:          ; @load_flat_hi_v2i16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	flat_load_ushort v0, v[0:1]
	v_and_b32_e32 v1, 0xffff, v2
	s_waitcnt vmcnt(0) lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end19:
	.size	load_flat_hi_v2i16_reglo_vreg, .Lfunc_end19-load_flat_hi_v2i16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_flat_hi_v2f16_reglo_vreg   ; -- Begin function load_flat_hi_v2f16_reglo_vreg
	.p2align	2
	.type	load_flat_hi_v2f16_reglo_vreg,@function
load_flat_hi_v2f16_reglo_vreg:          ; @load_flat_hi_v2f16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	flat_load_ushort v0, v[0:1]
	v_cvt_f16_f32_e32 v1, v2
	s_waitcnt vmcnt(0) lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end20:
	.size	load_flat_hi_v2f16_reglo_vreg, .Lfunc_end20-load_flat_hi_v2f16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 44
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_flat_hi_v2i16_reglo_vreg_zexti8 ; -- Begin function load_flat_hi_v2i16_reglo_vreg_zexti8
	.p2align	2
	.type	load_flat_hi_v2i16_reglo_vreg_zexti8,@function
load_flat_hi_v2i16_reglo_vreg_zexti8:   ; @load_flat_hi_v2i16_reglo_vreg_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	flat_load_ubyte v0, v[0:1]
	v_and_b32_e32 v1, 0xffff, v2
	s_waitcnt vmcnt(0) lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end21:
	.size	load_flat_hi_v2i16_reglo_vreg_zexti8, .Lfunc_end21-load_flat_hi_v2i16_reglo_vreg_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_flat_hi_v2i16_reglo_vreg_sexti8 ; -- Begin function load_flat_hi_v2i16_reglo_vreg_sexti8
	.p2align	2
	.type	load_flat_hi_v2i16_reglo_vreg_sexti8,@function
load_flat_hi_v2i16_reglo_vreg_sexti8:   ; @load_flat_hi_v2i16_reglo_vreg_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	flat_load_sbyte v0, v[0:1]
	v_and_b32_e32 v1, 0xffff, v2
	s_waitcnt vmcnt(0) lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end22:
	.size	load_flat_hi_v2i16_reglo_vreg_sexti8, .Lfunc_end22-load_flat_hi_v2i16_reglo_vreg_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_flat_hi_v2f16_reglo_vreg_zexti8 ; -- Begin function load_flat_hi_v2f16_reglo_vreg_zexti8
	.p2align	2
	.type	load_flat_hi_v2f16_reglo_vreg_zexti8,@function
load_flat_hi_v2f16_reglo_vreg_zexti8:   ; @load_flat_hi_v2f16_reglo_vreg_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	flat_load_ubyte v0, v[0:1]
	v_cvt_f16_f32_e32 v1, v2
	s_waitcnt vmcnt(0) lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end23:
	.size	load_flat_hi_v2f16_reglo_vreg_zexti8, .Lfunc_end23-load_flat_hi_v2f16_reglo_vreg_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 44
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_flat_hi_v2f16_reglo_vreg_sexti8 ; -- Begin function load_flat_hi_v2f16_reglo_vreg_sexti8
	.p2align	2
	.type	load_flat_hi_v2f16_reglo_vreg_sexti8,@function
load_flat_hi_v2f16_reglo_vreg_sexti8:   ; @load_flat_hi_v2f16_reglo_vreg_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	flat_load_sbyte v0, v[0:1]
	v_cvt_f16_f32_e32 v1, v2
	s_waitcnt vmcnt(0) lgkmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end24:
	.size	load_flat_hi_v2f16_reglo_vreg_sexti8, .Lfunc_end24-load_flat_hi_v2f16_reglo_vreg_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 44
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2i16_reglo_vreg ; -- Begin function load_private_hi_v2i16_reglo_vreg
	.p2align	2
	.type	load_private_hi_v2i16_reglo_vreg,@function
load_private_hi_v2i16_reglo_vreg:       ; @load_private_hi_v2i16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_ushort v1, off, s[0:3], s32 offset:4094
	v_and_b32_e32 v0, 0xffff, v0
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v1, 16, v1
	v_or_b32_e32 v0, v0, v1
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end25:
	.size	load_private_hi_v2i16_reglo_vreg, .Lfunc_end25-load_private_hi_v2i16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2f16_reglo_vreg ; -- Begin function load_private_hi_v2f16_reglo_vreg
	.p2align	2
	.type	load_private_hi_v2f16_reglo_vreg,@function
load_private_hi_v2f16_reglo_vreg:       ; @load_private_hi_v2f16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_ushort v1, off, s[0:3], s32 offset:4094
	v_cvt_f16_f32_e32 v0, v0
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v1, 16, v1
	v_or_b32_e32 v0, v0, v1
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end26:
	.size	load_private_hi_v2f16_reglo_vreg, .Lfunc_end26-load_private_hi_v2f16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 44
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2i16_reglo_vreg_nooff ; -- Begin function load_private_hi_v2i16_reglo_vreg_nooff
	.p2align	2
	.type	load_private_hi_v2i16_reglo_vreg_nooff,@function
load_private_hi_v2i16_reglo_vreg_nooff: ; @load_private_hi_v2i16_reglo_vreg_nooff
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_ushort v1, off, s[0:3], 0 offset:4094 glc
	s_waitcnt vmcnt(0)
	v_and_b32_e32 v0, 0xffff, v0
	v_lshlrev_b32_e32 v1, 16, v1
	v_or_b32_e32 v0, v0, v1
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end27:
	.size	load_private_hi_v2i16_reglo_vreg_nooff, .Lfunc_end27-load_private_hi_v2i16_reglo_vreg_nooff
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2f16_reglo_vreg_nooff ; -- Begin function load_private_hi_v2f16_reglo_vreg_nooff
	.p2align	2
	.type	load_private_hi_v2f16_reglo_vreg_nooff,@function
load_private_hi_v2f16_reglo_vreg_nooff: ; @load_private_hi_v2f16_reglo_vreg_nooff
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_ushort v0, off, s[0:3], 0 offset:4094 glc
	s_waitcnt vmcnt(0)
	v_cvt_f16_f32_e32 v1, v1
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end28:
	.size	load_private_hi_v2f16_reglo_vreg_nooff, .Lfunc_end28-load_private_hi_v2f16_reglo_vreg_nooff
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 44
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2i16_reglo_vreg_zexti8 ; -- Begin function load_private_hi_v2i16_reglo_vreg_zexti8
	.p2align	2
	.type	load_private_hi_v2i16_reglo_vreg_zexti8,@function
load_private_hi_v2i16_reglo_vreg_zexti8: ; @load_private_hi_v2i16_reglo_vreg_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_ubyte v1, off, s[0:3], s32 offset:4095
	v_and_b32_e32 v0, 0xffff, v0
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v1, 16, v1
	v_or_b32_e32 v0, v0, v1
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end29:
	.size	load_private_hi_v2i16_reglo_vreg_zexti8, .Lfunc_end29-load_private_hi_v2i16_reglo_vreg_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2f16_reglo_vreg_zexti8 ; -- Begin function load_private_hi_v2f16_reglo_vreg_zexti8
	.p2align	2
	.type	load_private_hi_v2f16_reglo_vreg_zexti8,@function
load_private_hi_v2f16_reglo_vreg_zexti8: ; @load_private_hi_v2f16_reglo_vreg_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_ubyte v1, off, s[0:3], s32 offset:4095
	v_cvt_f16_f32_e32 v0, v0
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v1, 16, v1
	v_or_b32_e32 v0, v0, v1
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end30:
	.size	load_private_hi_v2f16_reglo_vreg_zexti8, .Lfunc_end30-load_private_hi_v2f16_reglo_vreg_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 44
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2f16_reglo_vreg_sexti8 ; -- Begin function load_private_hi_v2f16_reglo_vreg_sexti8
	.p2align	2
	.type	load_private_hi_v2f16_reglo_vreg_sexti8,@function
load_private_hi_v2f16_reglo_vreg_sexti8: ; @load_private_hi_v2f16_reglo_vreg_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_sbyte v1, off, s[0:3], s32 offset:4095
	v_cvt_f16_f32_e32 v0, v0
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v1, 16, v1
	v_or_b32_e32 v0, v0, v1
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end31:
	.size	load_private_hi_v2f16_reglo_vreg_sexti8, .Lfunc_end31-load_private_hi_v2f16_reglo_vreg_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 44
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2i16_reglo_vreg_sexti8 ; -- Begin function load_private_hi_v2i16_reglo_vreg_sexti8
	.p2align	2
	.type	load_private_hi_v2i16_reglo_vreg_sexti8,@function
load_private_hi_v2i16_reglo_vreg_sexti8: ; @load_private_hi_v2i16_reglo_vreg_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_sbyte v1, off, s[0:3], s32 offset:4095
	v_and_b32_e32 v0, 0xffff, v0
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v1, 16, v1
	v_or_b32_e32 v0, v0, v1
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end32:
	.size	load_private_hi_v2i16_reglo_vreg_sexti8, .Lfunc_end32-load_private_hi_v2i16_reglo_vreg_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2i16_reglo_vreg_nooff_zexti8 ; -- Begin function load_private_hi_v2i16_reglo_vreg_nooff_zexti8
	.p2align	2
	.type	load_private_hi_v2i16_reglo_vreg_nooff_zexti8,@function
load_private_hi_v2i16_reglo_vreg_nooff_zexti8: ; @load_private_hi_v2i16_reglo_vreg_nooff_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_ubyte v0, off, s[0:3], 0 offset:4094 glc
	s_waitcnt vmcnt(0)
	v_and_b32_e32 v1, 0xffff, v1
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end33:
	.size	load_private_hi_v2i16_reglo_vreg_nooff_zexti8, .Lfunc_end33-load_private_hi_v2i16_reglo_vreg_nooff_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2i16_reglo_vreg_nooff_sexti8 ; -- Begin function load_private_hi_v2i16_reglo_vreg_nooff_sexti8
	.p2align	2
	.type	load_private_hi_v2i16_reglo_vreg_nooff_sexti8,@function
load_private_hi_v2i16_reglo_vreg_nooff_sexti8: ; @load_private_hi_v2i16_reglo_vreg_nooff_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_sbyte v0, off, s[0:3], 0 offset:4094 glc
	s_waitcnt vmcnt(0)
	v_and_b32_e32 v1, 0xffff, v1
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end34:
	.size	load_private_hi_v2i16_reglo_vreg_nooff_sexti8, .Lfunc_end34-load_private_hi_v2i16_reglo_vreg_nooff_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2f16_reglo_vreg_nooff_zexti8 ; -- Begin function load_private_hi_v2f16_reglo_vreg_nooff_zexti8
	.p2align	2
	.type	load_private_hi_v2f16_reglo_vreg_nooff_zexti8,@function
load_private_hi_v2f16_reglo_vreg_nooff_zexti8: ; @load_private_hi_v2f16_reglo_vreg_nooff_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_ubyte v0, off, s[0:3], 0 offset:4094 glc
	s_waitcnt vmcnt(0)
	v_cvt_f16_f32_e32 v1, v1
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end35:
	.size	load_private_hi_v2f16_reglo_vreg_nooff_zexti8, .Lfunc_end35-load_private_hi_v2f16_reglo_vreg_nooff_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 44
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_constant_hi_v2i16_reglo_vreg ; -- Begin function load_constant_hi_v2i16_reglo_vreg
	.p2align	2
	.type	load_constant_hi_v2i16_reglo_vreg,@function
load_constant_hi_v2i16_reglo_vreg:      ; @load_constant_hi_v2i16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff002, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_ushort v0, v[0:1]
	v_and_b32_e32 v1, 0xffff, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end36:
	.size	load_constant_hi_v2i16_reglo_vreg, .Lfunc_end36-load_constant_hi_v2i16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 60
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_constant_hi_v2f16_reglo_vreg ; -- Begin function load_constant_hi_v2f16_reglo_vreg
	.p2align	2
	.type	load_constant_hi_v2f16_reglo_vreg,@function
load_constant_hi_v2f16_reglo_vreg:      ; @load_constant_hi_v2f16_reglo_vreg
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff002, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_ushort v0, v[0:1]
	v_cvt_f16_f32_e32 v1, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end37:
	.size	load_constant_hi_v2f16_reglo_vreg, .Lfunc_end37-load_constant_hi_v2f16_reglo_vreg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 56
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_constant_hi_v2f16_reglo_vreg_sexti8 ; -- Begin function load_constant_hi_v2f16_reglo_vreg_sexti8
	.p2align	2
	.type	load_constant_hi_v2f16_reglo_vreg_sexti8,@function
load_constant_hi_v2f16_reglo_vreg_sexti8: ; @load_constant_hi_v2f16_reglo_vreg_sexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff001, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_sbyte v0, v[0:1]
	v_cvt_f16_f32_e32 v1, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end38:
	.size	load_constant_hi_v2f16_reglo_vreg_sexti8, .Lfunc_end38-load_constant_hi_v2f16_reglo_vreg_sexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 56
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_constant_hi_v2f16_reglo_vreg_zexti8 ; -- Begin function load_constant_hi_v2f16_reglo_vreg_zexti8
	.p2align	2
	.type	load_constant_hi_v2f16_reglo_vreg_zexti8,@function
load_constant_hi_v2f16_reglo_vreg_zexti8: ; @load_constant_hi_v2f16_reglo_vreg_zexti8
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v0, vcc, 0xfffff001, v0
	v_addc_u32_e32 v1, vcc, -1, v1, vcc
	flat_load_ubyte v0, v[0:1]
	v_cvt_f16_f32_e32 v1, v2
	s_waitcnt vmcnt(0)
	v_lshlrev_b32_e32 v0, 16, v0
	v_or_b32_e32 v0, v1, v0
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end39:
	.size	load_constant_hi_v2f16_reglo_vreg_zexti8, .Lfunc_end39-load_constant_hi_v2f16_reglo_vreg_zexti8
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 56
; NumSgprs: 34
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2i16_reglo_vreg_to_offset ; -- Begin function load_private_hi_v2i16_reglo_vreg_to_offset
	.p2align	2
	.type	load_private_hi_v2i16_reglo_vreg_to_offset,@function
load_private_hi_v2i16_reglo_vreg_to_offset: ; @load_private_hi_v2i16_reglo_vreg_to_offset
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_mov_b32_e32 v1, 0x7b
	buffer_store_dword v1, off, s[0:3], s32
	s_waitcnt vmcnt(0)
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end40:
	.size	load_private_hi_v2i16_reglo_vreg_to_offset, .Lfunc_end40-load_private_hi_v2i16_reglo_vreg_to_offset
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 40
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2i16_reglo_vreg_sexti8_to_offset ; -- Begin function load_private_hi_v2i16_reglo_vreg_sexti8_to_offset
	.p2align	2
	.type	load_private_hi_v2i16_reglo_vreg_sexti8_to_offset,@function
load_private_hi_v2i16_reglo_vreg_sexti8_to_offset: ; @load_private_hi_v2i16_reglo_vreg_sexti8_to_offset
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_and_b32_e32 v0, 0xffff, v0
	v_mov_b32_e32 v1, 0x7b
	buffer_store_dword v1, off, s[0:3], s32
	s_waitcnt vmcnt(0)
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end41:
	.size	load_private_hi_v2i16_reglo_vreg_sexti8_to_offset, .Lfunc_end41-load_private_hi_v2i16_reglo_vreg_sexti8_to_offset
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_private_hi_v2i16_reglo_vreg_zexti8_to_offset ; -- Begin function load_private_hi_v2i16_reglo_vreg_zexti8_to_offset
	.p2align	2
	.type	load_private_hi_v2i16_reglo_vreg_zexti8_to_offset,@function
load_private_hi_v2i16_reglo_vreg_zexti8_to_offset: ; @load_private_hi_v2i16_reglo_vreg_zexti8_to_offset
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_and_b32_e32 v0, 0xffff, v0
	v_mov_b32_e32 v1, 0x7b
	buffer_store_dword v1, off, s[0:3], s32
	s_waitcnt vmcnt(0)
	flat_store_dword v[0:1], v0
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end42:
	.size	load_private_hi_v2i16_reglo_vreg_zexti8_to_offset, .Lfunc_end42-load_private_hi_v2i16_reglo_vreg_zexti8_to_offset
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 48
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_local_v2i16_split_multi_chain ; -- Begin function load_local_v2i16_split_multi_chain
	.p2align	2
	.type	load_local_v2i16_split_multi_chain,@function
load_local_v2i16_split_multi_chain:     ; @load_local_v2i16_split_multi_chain
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v2, v0
	ds_read_u16 v1, v0 offset:2
	s_waitcnt lgkmcnt(1)
	v_mov_b32_e32 v0, v2
	s_waitcnt lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end43:
	.size	load_local_v2i16_split_multi_chain, .Lfunc_end43-load_local_v2i16_split_multi_chain
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 40
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_lo_hi_v2i16_samechain ; -- Begin function load_local_lo_hi_v2i16_samechain
	.p2align	2
	.type	load_local_lo_hi_v2i16_samechain,@function
load_local_lo_hi_v2i16_samechain:       ; @load_local_lo_hi_v2i16_samechain
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v2, v0
	ds_read_u16 v1, v0 offset:16
	s_waitcnt lgkmcnt(1)
	v_mov_b32_e32 v0, v2
	s_waitcnt lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end44:
	.size	load_local_lo_hi_v2i16_samechain, .Lfunc_end44-load_local_lo_hi_v2i16_samechain
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 40
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_v2i16_broadcast      ; -- Begin function load_local_v2i16_broadcast
	.p2align	2
	.type	load_local_v2i16_broadcast,@function
load_local_v2i16_broadcast:             ; @load_local_v2i16_broadcast
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v0, v0
	s_waitcnt lgkmcnt(0)
	v_mov_b32_e32 v1, v0
	s_setpc_b64 s[30:31]
.Lfunc_end45:
	.size	load_local_v2i16_broadcast, .Lfunc_end45-load_local_v2i16_broadcast
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 28
; NumSgprs: 32
; NumVgprs: 2
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_local_lo_hi_v2i16_side_effect ; -- Begin function load_local_lo_hi_v2i16_side_effect
	.p2align	2
	.type	load_local_lo_hi_v2i16_side_effect,@function
load_local_lo_hi_v2i16_side_effect:     ; @load_local_lo_hi_v2i16_side_effect
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v2, v0
	v_mov_b32_e32 v3, 0x7b
	ds_write_b16 v1, v3
	ds_read_u16 v1, v0 offset:16
	s_waitcnt lgkmcnt(2)
	v_mov_b32_e32 v0, v2
	s_waitcnt lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end46:
	.size	load_local_lo_hi_v2i16_side_effect, .Lfunc_end46-load_local_lo_hi_v2i16_side_effect
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 56
; NumSgprs: 32
; NumVgprs: 4
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_global_v2i16_split         ; -- Begin function load_global_v2i16_split
	.p2align	2
	.type	load_global_v2i16_split,@function
load_global_v2i16_split:                ; @load_global_v2i16_split
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v2, vcc, 2, v0
	v_addc_u32_e32 v3, vcc, 0, v1, vcc
	flat_load_ushort v0, v[0:1] glc
	s_waitcnt vmcnt(0)
	flat_load_ushort v1, v[2:3] glc
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end47:
	.size	load_global_v2i16_split, .Lfunc_end47-load_global_v2i16_split
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 40
; NumSgprs: 34
; NumVgprs: 4
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_flat_v2i16_split           ; -- Begin function load_flat_v2i16_split
	.p2align	2
	.type	load_flat_v2i16_split,@function
load_flat_v2i16_split:                  ; @load_flat_v2i16_split
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v2, vcc, 2, v0
	v_addc_u32_e32 v3, vcc, 0, v1, vcc
	flat_load_ushort v0, v[0:1] glc
	s_waitcnt vmcnt(0)
	flat_load_ushort v1, v[2:3] glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end48:
	.size	load_flat_v2i16_split, .Lfunc_end48-load_flat_v2i16_split
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 40
; NumSgprs: 34
; NumVgprs: 4
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_constant_v2i16_split       ; -- Begin function load_constant_v2i16_split
	.p2align	2
	.type	load_constant_v2i16_split,@function
load_constant_v2i16_split:              ; @load_constant_v2i16_split
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	v_add_i32_e32 v2, vcc, 2, v0
	v_addc_u32_e32 v3, vcc, 0, v1, vcc
	flat_load_ushort v0, v[0:1] glc
	flat_load_ushort v1, v[2:3] glc
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end49:
	.size	load_constant_v2i16_split, .Lfunc_end49-load_constant_v2i16_split
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 36
; NumSgprs: 34
; NumVgprs: 4
; ScratchSize: 0
; MemoryBound: 0
	.text
	.globl	load_private_v2i16_split        ; -- Begin function load_private_v2i16_split
	.p2align	2
	.type	load_private_v2i16_split,@function
load_private_v2i16_split:               ; @load_private_v2i16_split
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	buffer_load_ushort v0, off, s[0:3], s32 glc
	s_waitcnt vmcnt(0)
	buffer_load_ushort v1, off, s[0:3], s32 offset:2 glc
	s_waitcnt vmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end50:
	.size	load_private_v2i16_split, .Lfunc_end50-load_private_v2i16_split
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 32
; NumSgprs: 33
; NumVgprs: 2
; ScratchSize: 8
; MemoryBound: 0
	.text
	.globl	load_local_hi_v2i16_store_local_lo ; -- Begin function load_local_hi_v2i16_store_local_lo
	.p2align	2
	.type	load_local_hi_v2i16_store_local_lo,@function
load_local_hi_v2i16_store_local_lo:     ; @load_local_hi_v2i16_store_local_lo
; %bb.0:                                ; %entry
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_mov_b32 m0, -1
	ds_read_u16 v2, v1
	ds_write_b16 v1, v0
	s_waitcnt lgkmcnt(1)
	v_mov_b32_e32 v1, v2
	s_waitcnt lgkmcnt(0)
	s_setpc_b64 s[30:31]
.Lfunc_end51:
	.size	load_local_hi_v2i16_store_local_lo, .Lfunc_end51-load_local_hi_v2i16_store_local_lo
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 40
; NumSgprs: 32
; NumVgprs: 3
; ScratchSize: 0
; MemoryBound: 0
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
