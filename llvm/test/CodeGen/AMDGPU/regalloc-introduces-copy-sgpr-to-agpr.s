	.text
	.amdgcn_target "amdgcn-amd-amdhsa--gfx908"
	.globl	regalloc_introduces_s_to_a_copy ; -- Begin function regalloc_introduces_s_to_a_copy
	.p2align	8
	.type	regalloc_introduces_s_to_a_copy,@function
regalloc_introduces_s_to_a_copy:        ; @regalloc_introduces_s_to_a_copy
; %bb.0:
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	s_add_u32 s0, s0, s7
	s_addc_u32 s1, s1, 0
	s_nop 0
	v_accvgpr_write_b32 a1, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a0, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a3, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a4, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a5, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a6, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a7, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a8, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a9, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a10, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a11, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a12, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a13, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a14, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a15, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a16, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a17, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a18, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a19, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a20, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a21, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a22, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a23, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a24, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a25, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a26, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a27, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a28, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a29, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a30, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a31, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a32, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a33, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a34, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	v_accvgpr_write_b32 a35, v34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_load_dword v34, v[0:1], off glc
	s_waitcnt vmcnt(0) lgkmcnt(0)
	buffer_wbinvl1_vol
	s_load_dword s4, s[8:9], 0x0
	s_load_dword s5, s[8:9], 0x0
	s_load_dword s6, s[8:9], 0x0
	s_load_dword s7, s[8:9], 0x0
	s_nop 0
	s_load_dword s8, s[8:9], 0x0
	v_accvgpr_read_b32 v0, a1
	s_waitcnt lgkmcnt(0)
	s_load_dword s9, s[8:9], 0x0
	s_waitcnt lgkmcnt(0)
	s_load_dword s10, s[8:9], 0x0
	s_load_dword s11, s[8:9], 0x0
	s_load_dword s12, s[8:9], 0x0
	s_load_dword s13, s[8:9], 0x0
	s_load_dword s14, s[8:9], 0x0
	s_load_dword s15, s[8:9], 0x0
	s_load_dword s16, s[8:9], 0x0
	s_load_dword s17, s[8:9], 0x0
	s_load_dword s18, s[8:9], 0x0
	s_load_dword s19, s[8:9], 0x0
	s_load_dword s20, s[8:9], 0x0
	s_load_dword s21, s[8:9], 0x0
	s_load_dword s22, s[8:9], 0x0
	s_load_dword s23, s[8:9], 0x0
	s_load_dword s24, s[8:9], 0x0
	s_load_dword s25, s[8:9], 0x0
	s_load_dword s26, s[8:9], 0x0
	s_load_dword s27, s[8:9], 0x0
	s_load_dword s28, s[8:9], 0x0
	s_load_dword s29, s[8:9], 0x0
	s_load_dword s30, s[8:9], 0x0
	s_load_dword s31, s[8:9], 0x0
	s_load_dword s34, s[8:9], 0x0
	s_load_dword s35, s[8:9], 0x0
	s_load_dword s36, s[8:9], 0x0
	s_load_dword s37, s[8:9], 0x0
	s_load_dword s38, s[8:9], 0x0
	s_load_dword s39, s[8:9], 0x0
	s_load_dword s40, s[8:9], 0x0
	v_accvgpr_write_b32 a2, v34
	v_mov_b32_e32 v34, s4
	buffer_store_dword v34, off, s[0:3], 0 offset:136 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s5
	buffer_store_dword v34, off, s[0:3], 0 offset:4 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s6
	buffer_store_dword v34, off, s[0:3], 0 offset:12 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s7
	buffer_store_dword v34, off, s[0:3], 0 offset:8 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s8
	buffer_store_dword v34, off, s[0:3], 0 offset:20 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s9
	buffer_store_dword v34, off, s[0:3], 0 offset:16 ; 4-byte Folded Spill
	s_waitcnt lgkmcnt(0)
	v_mov_b32_e32 v34, s10
	buffer_store_dword v34, off, s[0:3], 0 offset:28 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s11
	buffer_store_dword v34, off, s[0:3], 0 offset:24 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s12
	buffer_store_dword v34, off, s[0:3], 0 offset:36 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s13
	buffer_store_dword v34, off, s[0:3], 0 offset:32 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s14
	buffer_store_dword v34, off, s[0:3], 0 offset:44 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s15
	buffer_store_dword v34, off, s[0:3], 0 offset:40 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s16
	buffer_store_dword v34, off, s[0:3], 0 offset:52 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s17
	buffer_store_dword v34, off, s[0:3], 0 offset:48 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s18
	buffer_store_dword v34, off, s[0:3], 0 offset:60 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s19
	buffer_store_dword v34, off, s[0:3], 0 offset:56 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s20
	buffer_store_dword v34, off, s[0:3], 0 offset:68 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s21
	buffer_store_dword v34, off, s[0:3], 0 offset:64 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s22
	buffer_store_dword v34, off, s[0:3], 0 offset:76 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s23
	buffer_store_dword v34, off, s[0:3], 0 offset:72 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s24
	buffer_store_dword v34, off, s[0:3], 0 offset:84 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s25
	buffer_store_dword v34, off, s[0:3], 0 offset:80 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s26
	buffer_store_dword v34, off, s[0:3], 0 offset:92 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s27
	buffer_store_dword v34, off, s[0:3], 0 offset:88 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s28
	buffer_store_dword v34, off, s[0:3], 0 offset:100 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s29
	buffer_store_dword v34, off, s[0:3], 0 offset:96 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s30
	buffer_store_dword v34, off, s[0:3], 0 offset:108 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s31
	buffer_store_dword v34, off, s[0:3], 0 offset:104 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s34
	buffer_store_dword v34, off, s[0:3], 0 offset:116 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s35
	buffer_store_dword v34, off, s[0:3], 0 offset:112 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s36
	buffer_store_dword v34, off, s[0:3], 0 offset:124 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s37
	buffer_store_dword v34, off, s[0:3], 0 offset:120 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s38
	buffer_store_dword v34, off, s[0:3], 0 offset:132 ; 4-byte Folded Spill
	v_mov_b32_e32 v34, s39
	buffer_store_dword v34, off, s[0:3], 0 offset:128 ; 4-byte Folded Spill
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a0
	v_mov_b32_e32 v34, s40
	s_waitcnt vmcnt(0) lgkmcnt(0)
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a3
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a4
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a5
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a6
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a7
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a8
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a9
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a10
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a11
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a12
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a13
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a14
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a15
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a16
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a17
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a18
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a19
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a20
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a21
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a22
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a23
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a24
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a25
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a26
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a27
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a28
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a29
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a30
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a31
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a32
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a33
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a34
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a35
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	v_accvgpr_read_b32 v0, a2
	s_waitcnt vmcnt(0) lgkmcnt(0)
	s_nop 0
	global_store_dword v[0:1], v0, off
	buffer_load_dword v35, off, s[0:3], 0 offset:136 ; 4-byte Folded Reload
	s_nop 0
	buffer_load_dword v0, off, s[0:3], 0 offset:4 ; 4-byte Folded Reload
	buffer_load_dword v1, off, s[0:3], 0 offset:8 ; 4-byte Folded Reload
	buffer_load_dword v2, off, s[0:3], 0 offset:12 ; 4-byte Folded Reload
	buffer_load_dword v3, off, s[0:3], 0 offset:16 ; 4-byte Folded Reload
	buffer_load_dword v4, off, s[0:3], 0 offset:20 ; 4-byte Folded Reload
	buffer_load_dword v5, off, s[0:3], 0 offset:24 ; 4-byte Folded Reload
	buffer_load_dword v6, off, s[0:3], 0 offset:28 ; 4-byte Folded Reload
	buffer_load_dword v7, off, s[0:3], 0 offset:32 ; 4-byte Folded Reload
	buffer_load_dword v8, off, s[0:3], 0 offset:36 ; 4-byte Folded Reload
	buffer_load_dword v9, off, s[0:3], 0 offset:40 ; 4-byte Folded Reload
	buffer_load_dword v10, off, s[0:3], 0 offset:44 ; 4-byte Folded Reload
	buffer_load_dword v11, off, s[0:3], 0 offset:48 ; 4-byte Folded Reload
	buffer_load_dword v12, off, s[0:3], 0 offset:52 ; 4-byte Folded Reload
	buffer_load_dword v13, off, s[0:3], 0 offset:56 ; 4-byte Folded Reload
	buffer_load_dword v14, off, s[0:3], 0 offset:60 ; 4-byte Folded Reload
	buffer_load_dword v15, off, s[0:3], 0 offset:64 ; 4-byte Folded Reload
	buffer_load_dword v16, off, s[0:3], 0 offset:68 ; 4-byte Folded Reload
	buffer_load_dword v17, off, s[0:3], 0 offset:72 ; 4-byte Folded Reload
	buffer_load_dword v18, off, s[0:3], 0 offset:76 ; 4-byte Folded Reload
	buffer_load_dword v19, off, s[0:3], 0 offset:80 ; 4-byte Folded Reload
	buffer_load_dword v20, off, s[0:3], 0 offset:84 ; 4-byte Folded Reload
	buffer_load_dword v21, off, s[0:3], 0 offset:88 ; 4-byte Folded Reload
	buffer_load_dword v22, off, s[0:3], 0 offset:92 ; 4-byte Folded Reload
	buffer_load_dword v23, off, s[0:3], 0 offset:96 ; 4-byte Folded Reload
	buffer_load_dword v24, off, s[0:3], 0 offset:100 ; 4-byte Folded Reload
	buffer_load_dword v25, off, s[0:3], 0 offset:104 ; 4-byte Folded Reload
	buffer_load_dword v26, off, s[0:3], 0 offset:108 ; 4-byte Folded Reload
	buffer_load_dword v27, off, s[0:3], 0 offset:112 ; 4-byte Folded Reload
	buffer_load_dword v28, off, s[0:3], 0 offset:116 ; 4-byte Folded Reload
	buffer_load_dword v29, off, s[0:3], 0 offset:120 ; 4-byte Folded Reload
	buffer_load_dword v30, off, s[0:3], 0 offset:124 ; 4-byte Folded Reload
	buffer_load_dword v31, off, s[0:3], 0 offset:128 ; 4-byte Folded Reload
	buffer_load_dword v32, off, s[0:3], 0 offset:132 ; 4-byte Folded Reload
	s_waitcnt vmcnt(33)
	v_accvgpr_write_b32 a0, v35             ;  Reload Reuse
	s_waitcnt vmcnt(0)
	s_nop 0
	s_endpgm
	.section	.rodata,#alloc
	.p2align	6
	.amdhsa_kernel regalloc_introduces_s_to_a_copy
		.amdhsa_group_segment_fixed_size 0
		.amdhsa_private_segment_fixed_size 140
		.amdhsa_kernarg_size 56
		.amdhsa_user_sgpr_count 6
		.amdhsa_user_sgpr_private_segment_buffer 1
		.amdhsa_user_sgpr_dispatch_ptr 1
		.amdhsa_user_sgpr_queue_ptr 1
		.amdhsa_user_sgpr_kernarg_segment_ptr 1
		.amdhsa_user_sgpr_dispatch_id 1
		.amdhsa_user_sgpr_flat_scratch_init 0
		.amdhsa_user_sgpr_private_segment_size 0
		.amdhsa_system_sgpr_private_segment_wavefront_offset 1
		.amdhsa_system_sgpr_workgroup_id_x 1
		.amdhsa_system_sgpr_workgroup_id_y 1
		.amdhsa_system_sgpr_workgroup_id_z 1
		.amdhsa_system_sgpr_workgroup_info 0
		.amdhsa_system_vgpr_workitem_id 2
		.amdhsa_next_free_vgpr 36
		.amdhsa_next_free_sgpr 81
		.amdhsa_reserve_vcc 0
		.amdhsa_reserve_flat_scratch 0
		.amdhsa_reserve_xnack_mask 1
		.amdhsa_float_round_mode_32 0
		.amdhsa_float_round_mode_16_64 0
		.amdhsa_float_denorm_mode_32 3
		.amdhsa_float_denorm_mode_16_64 3
		.amdhsa_dx10_clamp 1
		.amdhsa_ieee_mode 1
		.amdhsa_fp16_overflow 0
		.amdhsa_exception_fp_ieee_invalid_op 0
		.amdhsa_exception_fp_denorm_src 0
		.amdhsa_exception_fp_ieee_div_zero 0
		.amdhsa_exception_fp_ieee_overflow 0
		.amdhsa_exception_fp_ieee_underflow 0
		.amdhsa_exception_fp_ieee_inexact 0
		.amdhsa_exception_int_div_zero 0
	.end_amdhsa_kernel
	.text
.Lfunc_end0:
	.size	regalloc_introduces_s_to_a_copy, .Lfunc_end0-regalloc_introduces_s_to_a_copy
                                        ; -- End function
	.section	.AMDGPU.csdata
; Kernel info:
; codeLenInByte = 3028
; NumSgprs: 41
; NumVgprs: 36
; NumAgprs: 36
; TotalNumVgprs: 36
; ScratchSize: 140
; MemoryBound: 0
; FloatMode: 240
; IeeeMode: 1
; LDSByteSize: 0 bytes/workgroup (compile time only)
; SGPRBlocks: 10
; VGPRBlocks: 8
; NumSGPRsForWavesPerEU: 81
; NumVGPRsForWavesPerEU: 36
; Occupancy: 7
; WaveLimiterHint : 0
; COMPUTE_PGM_RSRC2:SCRATCH_EN: 1
; COMPUTE_PGM_RSRC2:USER_SGPR: 6
; COMPUTE_PGM_RSRC2:TRAP_HANDLER: 0
; COMPUTE_PGM_RSRC2:TGID_X_EN: 1
; COMPUTE_PGM_RSRC2:TGID_Y_EN: 1
; COMPUTE_PGM_RSRC2:TGID_Z_EN: 1
; COMPUTE_PGM_RSRC2:TIDIG_COMP_CNT: 2
	.section	".note.GNU-stack"
	.amdgpu_metadata
---
amdhsa.kernels:
  - .agpr_count:     36
    .args:
      - .offset:         0
        .size:           8
        .value_kind:     hidden_global_offset_x
      - .offset:         8
        .size:           8
        .value_kind:     hidden_global_offset_y
      - .offset:         16
        .size:           8
        .value_kind:     hidden_global_offset_z
      - .address_space:  global
        .offset:         24
        .size:           8
        .value_kind:     hidden_hostcall_buffer
      - .address_space:  global
        .offset:         32
        .size:           8
        .value_kind:     hidden_none
      - .address_space:  global
        .offset:         40
        .size:           8
        .value_kind:     hidden_none
      - .address_space:  global
        .offset:         48
        .size:           8
        .value_kind:     hidden_multigrid_sync_arg
    .group_segment_fixed_size: 0
    .kernarg_segment_align: 8
    .kernarg_segment_size: 56
    .max_flat_workgroup_size: 1024
    .name:           regalloc_introduces_s_to_a_copy
    .private_segment_fixed_size: 140
    .sgpr_count:     41
    .sgpr_spill_count: 0
    .symbol:         regalloc_introduces_s_to_a_copy.kd
    .vgpr_count:     36
    .vgpr_spill_count: 34
    .wavefront_size: 64
amdhsa.target:   amdgcn-amd-amdhsa--gfx908
amdhsa.version:
  - 1
  - 1
...

	.end_amdgpu_metadata
