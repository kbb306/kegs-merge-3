/****************************************************************/
/*			Apple IIgs emulator			*/
/*			Copyright 1998 Kent Dickey		*/
/*								*/
/*	This code may not be used in a commercial product	*/
/*	without prior written permission of the author.		*/
/*								*/
/*	You may freely distribute this code.			*/ 
/*								*/
/*	You can contact the author at kentd@cup.hp.com.		*/
/*	HP has nothing to do with this software.		*/
/****************************************************************/

const char rcsid_engine_c_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/engine_c.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include "sim65816.h"
#include "dis.h"
#include "video.h"
#include "engine.h"

static void check_breakpoints(word32 addr);
static word32 get_memory8_io_stub(word32 addr, byte *stat, double *fcycs_ptr, double fplus_x_m1);
static word32 get_memory16_pieces_stub(word32 addr, byte *stat, double *fcycs_ptr, Fplus *fplus_ptr);
static word32 get_memory24_pieces_stub(word32 addr, byte *stat, double *fcycs_ptr, Fplus *fplus_ptr);
static void set_memory8_io_stub(word32 addr, word32 val, byte *stat, double *fcycs_ptr, double fplus_x_m1);
static void set_memory16_pieces_stub(word32 addr, word32 val, double *fcycs_ptr, Fplus *fplus_ptr);
static void set_memory24_pieces_stub(word32 addr, word32 val, double *fcycs_ptr, Fplus *fplus_ptr);
static word32 do_adc_sbc8(word32 in1, word32 in2, word32 psr, int sub);
static word32 do_adc_sbc16(word32 in1, word32 in2, word32 psr, int sub);
static word32 get_remaining_operands(word32 addr, word32 opcode, word32 psr, Fplus *fplus_ptr);


#if 0
# define LOG_PC
/* define FCYCS_PTR_FCYCLES_ROUND_SLOW to get accurate 1MHz write to slow mem*/
/*  this might help joystick emulation in some Apple //gs games like */
/*  Madness */
# define FCYCS_PTR_FCYCLES_ROUND_SLOW	FCYCLES_ROUND; *fcycs_ptr = fcycles;
#endif

#ifndef FCYCS_PTR_FCYCLES_ROUND_SLOW
# define FCYCS_PTR_FCYCLES_ROUND_SLOW
#endif

int size_tab[] = {
#include "size_c"
};

int bogus[] = {
	0,
#include "op_routs.h"
};

#define FINISH(arg1, arg2)	g_ret1 = arg1; g_ret2 = arg2; goto finish;

#define CYCLES_PLUS_1	fcycles += fplus_1;
#define CYCLES_PLUS_2	fcycles += fplus_2;
#define CYCLES_PLUS_3	fcycles += fplus_3;
#define CYCLES_PLUS_4	fcycles += (fplus_1 + fplus_3);
#define CYCLES_PLUS_5	fcycles += (fplus_2 + fplus_3);
#define CYCLES_MINUS_1	fcycles -= fplus_1;
#define CYCLES_MINUS_2	fcycles -= fplus_2;

#define CYCLES_FINISH	fcycles = g_fcycles_stop + fplus_1;

#define FCYCLES_ROUND	fcycles = (int)(fcycles + fplus_x_m1);

#define	LOG_PC_MACRO()							\
		tmp_pc_ptr = log_pc_ptr++;				\
		tmp_pc_ptr->dbank_kpc = (dbank << 24) + kpc;		\
		tmp_pc_ptr->instr = (opcode << 24) + arg_ptr[1] +	\
			(arg_ptr[2] << 8) + (arg_ptr[3] << 16);		\
		tmp_pc_ptr->psr_acc = ((psr & ~(0x82)) << 16) + acc +	\
			(neg << 23) + ((!zero) << 17);			\
		tmp_pc_ptr->xreg_yreg = (xreg << 16) + yreg;		\
		tmp_pc_ptr->stack_direct = (stack << 16) + direct;	\
		tmp_pc_ptr->dcycs = fcycles + g_last_vbl_dcycs - fplus_2; \
		if(log_pc_ptr >= log_pc_end_ptr) {			\
			log_pc_ptr = log_pc_start_ptr;			\
		}

#define	GET_1BYTE_ARG	arg = arg_ptr[1];
#define	GET_2BYTE_ARG	arg = arg_ptr[1] + (arg_ptr[2] << 8);
#define	GET_3BYTE_ARG	arg = arg_ptr[1] + (arg_ptr[2] << 8) + (arg_ptr[3]<<16);

/* HACK HACK HACK */
#define	UPDATE_PSR(dummy, old_psr)				\
	if(psr & 0x100) {					\
		psr |= 0x30;					\
		stack = 0x100 + (stack & 0xff);			\
	}							\
	if((old_psr ^ psr) & 0x10) {				\
		if(psr & 0x10) {				\
			xreg = xreg & 0xff;			\
			yreg = yreg & 0xff;			\
		}						\
	}							\
	if(((psr & 0x4) == 0) && g_irq_pending) {		\
		FINISH(RET_IRQ, 0);				\
	}							\
	if((old_psr ^ psr) & 0x20) {				\
		goto recalc_accsize;				\
	}

#define GET_MEMORY8(addr,dest)					\
	addr_latch = (addr);					\
	CYCLES_PLUS_1;						\
	stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);	\
	wstat = PTR2WORD(stat) & 0xff;				\
	ptr = stat - wstat + ((addr) & 0xff);			\
	if(wstat & (1 << (31 - BANK_IO_BIT))) {			\
		fcycles_tmp1 = fcycles;				\
		dest = get_memory8_io_stub((addr), stat,	\
				&fcycles_tmp1, fplus_x_m1);	\
		fcycles = fcycles_tmp1;				\
	} else {						\
		dest = *ptr;					\
	}

#define GET_MEMORY(addr,dest)	GET_MEMORY8(addr, dest)

#define GET_MEMORY16(addr, dest)				\
	save_addr = addr;					\
	stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);	\
	wstat = PTR2WORD(stat) & 0xff;				\
	ptr = stat - wstat + ((addr) & 0xff);			\
	if((wstat & (1 << (31 - BANK_IO_BIT))) || (((addr) & 0xff) == 0xff)) { \
		fcycles_tmp1 = fcycles;				\
		dest = get_memory16_pieces_stub((addr), stat,	\
			&fcycles_tmp1, fplus_ptr);		\
		fcycles = fcycles_tmp1;				\
	} else {						\
		CYCLES_PLUS_2;					\
		dest = ptr[0] + (ptr[1] << 8);			\
	}							\
	addr_latch = save_addr;

#define GET_MEMORY24(addr, dest)				\
	save_addr = addr;					\
	stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);	\
	wstat = PTR2WORD(stat) & 0xff;				\
	ptr = stat - wstat + ((addr) & 0xff);			\
	if((wstat & (1 << (31 - BANK_IO_BIT))) || (((addr) & 0xfe) == 0xfe)) { \
		fcycles_tmp1 = fcycles;				\
		dest = get_memory24_pieces_stub((addr), stat,	\
			&fcycles_tmp1, fplus_ptr);		\
		fcycles = fcycles_tmp1;				\
	} else {						\
		CYCLES_PLUS_3;					\
		dest = ptr[0] + (ptr[1] << 8) + (ptr[2] << 16);	\
	}							\
	addr_latch = save_addr;

#define GET_MEMORY_DIRECT_PAGE16(addr, dest)				\
	save_addr = addr;						\
	if(psr & 0x100) {						\
		if((direct & 0xff) == 0) {				\
			save_addr = (save_addr & 0xff) + direct;	\
		}							\
	}								\
	if((psr & 0x100) && (((addr) & 0xff) == 0xff)) {		\
		GET_MEMORY8(save_addr, getmem_tmp);			\
		save_addr++;						\
		if((direct & 0xff) == 0) {				\
			save_addr = (save_addr & 0xff) + direct;	\
		}							\
		GET_MEMORY8(save_addr, dest);				\
		dest = (dest << 8) + getmem_tmp;			\
	} else {							\
		GET_MEMORY16(save_addr, dest);				\
	}


#define PUSH8(arg)						\
	SET_MEMORY8(stack, arg);				\
	stack--;						\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}							\
	stack = stack & 0xffff;

#define PUSH16(arg)						\
	if((stack & 0xfe) == 0) {				\
		/* stack will cross page! */			\
		PUSH8((arg) >> 8);				\
		PUSH8(arg);					\
	} else {						\
		stack -= 2;					\
		stack = stack & 0xffff;				\
		SET_MEMORY16(stack + 1, arg);			\
	}

#define PUSH16_UNSAFE(arg)					\
	save_addr = (stack - 1) & 0xffff;			\
	stack -= 2;						\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}							\
	stack = stack & 0xffff;					\
	SET_MEMORY16(save_addr, arg);

#define PUSH24_UNSAFE(arg)					\
	save_addr = (stack - 2) & 0xffff;			\
	stack -= 3;						\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}							\
	stack = stack & 0xffff;					\
	SET_MEMORY24(save_addr, arg);

#define PULL8(dest)						\
	stack++;						\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}							\
	stack = stack & 0xffff;					\
	GET_MEMORY8(stack, dest);

#define PULL16(dest)						\
	if((stack & 0xfe) == 0xfe) {	/* page cross */	\
		PULL8(dest);					\
		PULL8(pull_tmp);				\
		dest = (pull_tmp << 8) + dest;			\
	} else {						\
		GET_MEMORY16(stack + 1, dest);			\
		stack = (stack + 2) & 0xffff;			\
		if(psr & 0x100) {				\
			stack = 0x100 | (stack & 0xff);		\
		}						\
	}

#define PULL16_UNSAFE(dest)					\
	stack = (stack + 1) & 0xffff;				\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}							\
	GET_MEMORY16(stack, dest);				\
	stack = (stack + 1) & 0xffff;				\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}

#define PULL24(dest)						\
	if((stack & 0xfc) == 0xfc) {	/* page cross */	\
		PULL8(dest);					\
		PULL8(pull_tmp);				\
		pull_tmp = (pull_tmp << 8) + dest;		\
		PULL8(dest);					\
		dest = (dest << 16) + pull_tmp;			\
	} else {						\
		GET_MEMORY24(stack + 1, dest);			\
		stack = (stack + 3) & 0xffff;			\
		if(psr & 0x100) {				\
			stack = 0x100 | (stack & 0xff);		\
		}						\
	}

#define SET_MEMORY8(addr, val)						\
	CYCLES_PLUS_1;							\
	stat = GET_PAGE_INFO_WR(((addr) >> 8) & 0xffff);		\
	wstat = PTR2WORD(stat) & 0xff;					\
	ptr = stat - wstat + ((addr) & 0xff);				\
	if(wstat) {							\
		fcycles_tmp1 = fcycles;					\
		set_memory8_io_stub((addr), val, stat, &fcycles_tmp1,	\
				fplus_x_m1);				\
		fcycles = fcycles_tmp1;					\
	} else {							\
		*ptr = val;						\
	}





#define SET_MEMORY16(addr, val)						\
	stat = GET_PAGE_INFO_WR(((addr) >> 8) & 0xffff);		\
	wstat = PTR2WORD(stat) & 0xff;					\
	ptr = stat - wstat + ((addr) & 0xff);				\
	if((wstat) || (((addr) & 0xff) == 0xff)) {		\
		fcycles_tmp1 = fcycles;					\
		set_memory16_pieces_stub((addr), (val),			\
			&fcycles_tmp1, fplus_ptr);			\
		fcycles = fcycles_tmp1;					\
	} else {							\
		CYCLES_PLUS_2;						\
		ptr[0] = (val);						\
		ptr[1] = (val) >> 8;					\
	}

#define SET_MEMORY24(addr, val)						\
	stat = GET_PAGE_INFO_WR(((addr) >> 8) & 0xffff);		\
	wstat = PTR2WORD(stat) & 0xff;					\
	ptr = stat - wstat + ((addr) & 0xff);				\
	if((wstat) || (((addr) & 0xfe) == 0xfe)) {			\
		fcycles_tmp1 = fcycles;					\
		set_memory24_pieces_stub((addr), (val), 		\
			&fcycles_tmp1, fplus_ptr);			\
		fcycles = fcycles_tmp1;					\
	} else {							\
		CYCLES_PLUS_2;						\
		ptr[0] = (val);						\
		ptr[1] = (val) >> 8;					\
		ptr[2] = (val) >> 16;					\
	}

void
check_breakpoints(word32 addr)
{
	int	count;
	int	i;

	count = g_num_breakpoints;
	for(i = 0; i < count; i++) {
		if(g_breakpts[i] == addr) {
			ki_printf("Hit breakpoint at %06x\n", addr);
			set_halt(HALT_WANTTOBRK);	
		}
	}
}

word32
get_memory8_io_stub(word32 addr, byte *stat, double *fcycs_ptr,
			double fplus_x_m1)
{
	double	fcycles;
	word32	wstat;
	byte	*ptr;

	wstat = PTR2WORD(stat) & 0xff;
	if(wstat & BANK_BREAK) {
		check_breakpoints(addr);
	}
	fcycles = *fcycs_ptr;
	if(wstat & BANK_IO2_TMP) {
		FCYCLES_ROUND;
		*fcycs_ptr = fcycles;
		return get_memory_io((addr), fcycs_ptr);
	} else {
		ptr = stat - wstat + (addr & 0xff);
		return *ptr;
	}
}

word32
get_memory16_pieces_stub(word32 addr, byte *stat, double *fcycs_ptr,
		Fplus	*fplus_ptr)
{
	byte	*ptr;
	double	fcycles, fcycles_tmp1;
	double	fplus_1;
	double	fplus_x_m1;
	word32	wstat;
	word32	addr_latch;
	word32	ret;
	word32	tmp1;

	fcycles = *fcycs_ptr;
	fplus_1 = fplus_ptr->plus_1;
	fplus_x_m1 = fplus_ptr->plus_x_minus_1;
	GET_MEMORY8(addr, tmp1);
	GET_MEMORY8(addr+1, ret);
	*fcycs_ptr = fcycles;
	return (ret << 8) + (tmp1);
}

word32
get_memory24_pieces_stub(word32 addr, byte *stat, double *fcycs_ptr,
		Fplus *fplus_ptr)
{
	byte	*ptr;
	double	fcycles, fcycles_tmp1;
	double	fplus_1;
	double	fplus_x_m1;
	word32	wstat;
	word32	addr_latch;
	word32	ret;
	word32	tmp1;
	word32	tmp2;

	fcycles = *fcycs_ptr;
	fplus_1 = fplus_ptr->plus_1;
	fplus_x_m1 = fplus_ptr->plus_x_minus_1;
	GET_MEMORY8(addr, tmp1);
	GET_MEMORY8(addr+1, tmp2);
	GET_MEMORY8(addr+2, ret);
	*fcycs_ptr = fcycles;
	return (ret << 16) + (tmp2 << 8) + tmp1;
}

void
set_memory8_io_stub(word32 addr, word32 val, byte *stat, double *fcycs_ptr,
		double fplus_x_m1)
{
	double	fcycles;
	word32	setmem_tmp1;
	word32	tmp1, tmp2;
	byte	*ptr;
	word32	wstat;

	wstat = PTR2WORD(stat) & 0xff;
	if(wstat & (1 << (31 - BANK_BREAK_BIT))) {
		check_breakpoints(addr);
	}
	ptr = stat - wstat + ((addr) & 0xff);				\
	fcycles = *fcycs_ptr;
	if(wstat & (1 << (31 - BANK_IO2_BIT))) {
		FCYCLES_ROUND;
		*fcycs_ptr = fcycles;
		set_memory_io((addr), val, fcycs_ptr);
	} else if(wstat & (1 << (31 - BANK_SHADOW_BIT))) {
		FCYCS_PTR_FCYCLES_ROUND_SLOW;
		tmp1 = (addr & 0xffff);
		setmem_tmp1 = g_slow_memory_ptr[tmp1];
		*ptr = val;
		if(setmem_tmp1 != ((val) & 0xff)) {
			g_slow_memory_ptr[tmp1] = val;
			slow_mem_changed[tmp1 >> CHANGE_SHIFT] |=
				(1 << (31-((tmp1 >> SHIFT_PER_CHANGE) & 0x1f)));
		}
	} else if(wstat & (1 << (31 - BANK_SHADOW2_BIT))) {
		FCYCS_PTR_FCYCLES_ROUND_SLOW;
		tmp2 = (addr & 0xffff);
		tmp1 = 0x10000 + tmp2;
		setmem_tmp1 = g_slow_memory_ptr[tmp1];
		*ptr = val;
		if(setmem_tmp1 != ((val) & 0xff)) {
			g_slow_memory_ptr[tmp1] = val;
			slow_mem_changed[tmp2 >>CHANGE_SHIFT] |=
				(1 <<(31-((tmp2 >> SHIFT_PER_CHANGE) & 0x1f)));
		}
	} else {
		/* breakpoint only */
		*ptr = val;
	}
}

void
set_memory16_pieces_stub(word32 addr, word32 val, double *fcycs_ptr,
		Fplus	*fplus_ptr)
{
	byte	*ptr;
	byte	*stat;
	double	fcycles, fcycles_tmp1;
	double	fplus_1;
	double	fplus_x_m1;
	word32	wstat;

	fcycles = *fcycs_ptr;
	fplus_1 = fplus_ptr->plus_1;
	fplus_x_m1 = fplus_ptr->plus_x_minus_1;
	SET_MEMORY8(addr, val);
	SET_MEMORY8(addr+1, val >> 8);

	*fcycs_ptr = fcycles;
}

void
set_memory24_pieces_stub(word32 addr, word32 val, double *fcycs_ptr,
		Fplus	*fplus_ptr)
{
	byte	*ptr;
	byte	*stat;
	double	fcycles, fcycles_tmp1;
	double	fplus_1;
	double	fplus_x_m1;
	word32	wstat;

	fcycles = *fcycs_ptr;
	fplus_1 = fplus_ptr->plus_1;
	fplus_x_m1 = fplus_ptr->plus_x_minus_1;
	SET_MEMORY8(addr, val);
	SET_MEMORY8(addr+1, val >> 8);
	SET_MEMORY8(addr+2, val >> 16);

	*fcycs_ptr = fcycles;
}


word32
get_memory_c(word32 addr, int cycs)
{
	byte	*stat;
	byte	*ptr;
	double	fcycles, fcycles_tmp1;
	double	fplus_1;
	double	fplus_x_m1;
	word32	addr_latch;
	word32	wstat;
	word32	ret;

	fcycles = 0;
	fplus_1 = 0;
	fplus_x_m1 = 0;
	GET_MEMORY8(addr, ret);
	return ret;
}
#if 0
	stat = page_info[(addr>>8) & 0xffff].rd;
	ptr = (byte *)((stat & 0xffffff00) | (addr & 0xff));
	if(stat & (1 << (31 -BANK_IO_BIT))) {
		return get_memory_io(addr, &in_fcycles);
	}
	return *ptr;
#endif

word32
get_memory16_c(word32 addr, int cycs)
{
	/*
	double	fcycs;

	fcycs = 0;
	return get_memory_c(addr, fcycs) +
			(get_memory_c(addr+1, fcycs) << 8);
			*/
	cycs=0;
	return get_memory_c(addr, cycs) +	(get_memory_c(addr+1, cycs) << 8);

}

word32
get_memory24_c(word32 addr, int cycs)
{
	/*
	double	fcycs;

	fcycs = 0;
	return get_memory_c(addr, fcycs) +
			(get_memory_c(addr+1, fcycs) << 8) +
			(get_memory_c(addr+2, fcycs) << 16);
			*/
	cycs = 0;
	return get_memory_c(addr, cycs) +	(get_memory_c(addr+1, cycs) << 8) + (get_memory_c(addr+2, cycs) << 16);
}

void
set_memory_c(word32 addr, word32 val, int cycs)
{
	byte	*stat;
	byte	*ptr;
	double	fcycles, fcycles_tmp1;
	double	fplus_1;
	double	fplus_x_m1;
	word32	wstat;

	fcycles = 0;
	fplus_1 = 0;
	fplus_x_m1 = 0;
	SET_MEMORY8(addr, val);
}
void
set_memory16_c(word32 addr, word32 val, int cycs)
{
	set_memory_c(addr, val, 0);
	set_memory_c(addr + 1, val >> 8, 0);
}

#if 0
void
set_memory24_c(word32 addr, word32 val, int cycs)
{
	set_memory_c(addr, val, 0);
	set_memory_c(addr + 1, val >> 8, 0);
	set_memory_c(addr + 2, val >> 16, 0);
}
#endif

word32
do_adc_sbc8(word32 in1, word32 in2, word32 psr, int sub)
{
	word32	sum, carry, overflow;
	word32	zero;
	int	decimal;

	overflow = 0;
	decimal = psr & 8;
	if(sub) {
		in2 = (in2 ^ 0xff);
	}
	if(!decimal) {
		sum = (in1 & 0xff) + in2 + (psr & 1);
		overflow = ((sum ^ in2) >> 1) & 0x40;
	} else {
		/* decimal */
		sum = (in1 & 0xf) + (in2 & 0xf) + (psr & 1);
		if(sub) {
			if(sum < 0x10) {
				sum = (sum - 0x6) & 0xf;
			}
		} else {
			if(sum >= 0xa) {
				sum = (sum - 0xa) | 0x10;
			}
		}

		sum = (in1 & 0xf0) + (in2 & 0xf0) + sum;
		overflow = ((sum >> 2) ^ (sum >> 1)) & 0x40;
		if(sub) {
			if(sum < 0x100) {
				sum = (sum + 0xa0) & 0xff;
			}
		} else {
			if(sum >= 0xa0) {
				sum += 0x60;
			}
		}
	}

	zero = ((sum & 0xff) == 0);
	carry = (sum >= 0x100);
	if((in1 ^ in2) & 0x80) {
		overflow = 0;
	}

	psr = psr & (~0xc3);
	psr = psr + (sum & 0x80) + overflow + (zero << 1) + carry;

	return (psr << 16) + (sum & 0xff);
}

word32
do_adc_sbc16(word32 in1, word32 in2, word32 psr, int sub)
{
	word32	sum, carry, overflow;
	word32	tmp1, tmp2;
	word32	zero;
	int	decimal;

	overflow = 0;
	decimal = psr & 8;
	if(!decimal) {
		if(sub) {
			in2 = (in2 ^ 0xffff);
		}
		sum = in1 + in2 + (psr & 1);
		overflow = ((sum ^ in2) >> 9) & 0x40;
	} else {
		/* decimal */
		if(sub) {
			tmp1 = do_adc_sbc8(in1 & 0xff, in2 & 0xff, psr, sub);
			psr = (tmp1 >> 16);
			tmp2 = do_adc_sbc8((in1 >> 8) & 0xff,
						(in2 >> 8) & 0xff, psr, sub);
			in2 = (in2 ^ 0xfffff);
		} else {
			tmp1 = do_adc_sbc8(in1 & 0xff, in2 & 0xff, psr, sub);
			psr = (tmp1 >> 16);
			tmp2 = do_adc_sbc8((in1 >> 8) & 0xff,
						(in2 >> 8) &0xff, psr, sub);
		}
		sum = ((tmp2 & 0xff) << 8) + (tmp1 & 0xff) +
					(((tmp2 >> 16) & 1) << 16);
		overflow = (tmp2 >> 16) & 0x40;
	}

	zero = ((sum & 0xffff) == 0);
	carry = (sum >= 0x10000);
	if((in1 ^ in2) & 0x8000) {
		overflow = 0;
	}

	psr = psr & (~0xc3);
	psr = psr + ((sum & 0x8000) >> 8) + overflow + (zero << 1) + carry;

	return (psr << 16) + (sum & 0xffff);
}

int	g_ret1;
int	g_ret2;


void
fixed_memory_ptrs_init()
{
	/* set g_slow_memory_ptr, g_rom_fc_ff_ptr, g_dummy_memory1_ptr, */
	/*  and rom_cards_ptr */

	if (g_slow_memory_ptr||g_dummy_memory1_ptr||g_rom_fc_ff_ptr||g_rom_cards_ptr)
		ki_printf("memory leaks...\n");

	g_slow_memory_ptr = memalloc_align(128*1024, 0, 0);
	g_dummy_memory1_ptr = memalloc_align(256, 1024, 0);
	g_rom_fc_ff_ptr = memalloc_align(256*1024, 1024, 0);
	g_rom_cards_ptr = memalloc_align(16*256, 1024, 0);

#if 0
	ki_printf("g_memory_ptr: %08x, dummy_mem: %08x, slow_mem_ptr: %08x\n",
		(word32)g_memory_ptr, (word32)g_dummy_memory1_ptr,
		(word32)g_slow_memory_ptr);
	ki_printf("g_rom_fc_ff_ptr: %08x, g_rom_cards_ptr: %08x\n",
		(word32)g_rom_fc_ff_ptr, (word32)g_rom_cards_ptr);
	ki_printf("page_info_rd = %08x, page_info_wr end = %08x\n",
		(word32)&(page_info_rd_wr[0]),
		(word32)&(page_info_rd_wr[PAGE_INFO_PAD_SIZE+0x1ffff].rd_wr));
#endif
}

// OG
void fixed_memory_ptrs_shut()
{
	/* set g_slow_memory_ptr, g_rom_fc_ff_ptr, g_dummy_memory1_ptr, */
	/*  and rom_cards_ptr */

	memfree_align(g_slow_memory_ptr); g_slow_memory_ptr=NULL;
	memfree_align(g_dummy_memory1_ptr); g_dummy_memory1_ptr=NULL;
	memfree_align(g_rom_fc_ff_ptr); g_rom_fc_ff_ptr=NULL;
	memfree_align(g_rom_cards_ptr); g_rom_cards_ptr=NULL;
}
// ~OG

void
set_halt_act(int val)
{
	halt_sim |= val;
	g_fcycles_stop = (double)0.0;
}

void
clr_halt_act()
{
	halt_sim = 0;
}

word32
get_remaining_operands(word32 addr, word32 opcode, word32 psr, Fplus *fplus_ptr)
{
	byte	*stat;
	byte	*ptr;
	double	fcycles, fcycles_tmp1;
	double	fplus_1, fplus_2, fplus_3;
	double	fplus_x_m1;
	word32	addr_latch;
	word32	wstat;
	word32	save_addr;
	word32	arg;
	int	size;

	fcycles = 0;
	fplus_1 = 0;
	fplus_2 = 0;
	fplus_3 = 0;
	fplus_x_m1 = 0;

	size = size_tab[opcode];

	switch(size) {
	case 0:
		arg = 0;
		break;
	case 1:
		GET_MEMORY8(addr+1, arg);
		break;
	case 2:
		GET_MEMORY16(addr+1, arg);
		break;
	case 3:
		GET_MEMORY24(addr+1, arg);
		break;
	case 4:
		if(psr & 0x20) {
			GET_MEMORY8(addr+1, arg);
		} else {
			GET_MEMORY16(addr+1, arg);
		}
		break;
	case 5:
		if(psr & 0x10) {
			GET_MEMORY8(addr+1, arg);
		} else {
			GET_MEMORY16(addr+1, arg);
		}
		break;
	default:
		ki_printf("Unknown size: %d\n", size);
		arg = 0;
		my_exit(-2);
	}

	return arg;
}

#define FETCH_OPCODE							\
	addr = kpc;							\
	CYCLES_PLUS_2;							\
	stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);		\
	wstat = PTR2WORD(stat) & 0xff;					\
	ptr = stat - wstat + ((addr) & 0xff);				\
	arg_ptr = ptr;							\
	opcode = *ptr;							\
	if((wstat & (1 << (31-BANK_IO_BIT))) || ((addr & 0xff) > 0xfc)) {\
		if(wstat & BANK_BREAK) {				\
			check_breakpoints(addr);			\
		}							\
		if((addr & 0xfffff0) == 0x00c700) {			\
			if(addr == 0xc700) {				\
				FINISH(RET_C700, 0);			\
			} else if(addr == 0xc70a) {			\
				FINISH(RET_C70A, 0);			\
			} else if(addr == 0xc70d) {			\
				FINISH(RET_C70D, 0);			\
			}						\
		}							\
		if(wstat & (1 << (31 - BANK_IO2_BIT))) {		\
			FCYCLES_ROUND;					\
			fcycles_tmp1 = fcycles;				\
			opcode = get_memory_io((addr), &fcycles_tmp1);	\
			fcycles = fcycles_tmp1;				\
		} else {						\
			opcode = *ptr;					\
		}							\
		arg = get_remaining_operands(addr, opcode, psr, fplus_ptr);\
		arg_ptr = (byte *)&tmp_bytes;				\
		arg_ptr[1] = arg;					\
		arg_ptr[2] = arg >> 8;					\
		arg_ptr[3] = arg >> 16;					\
	}

#ifdef _DEBUG
	word32	kpc;
#endif

int
enter_engine(Engine_reg *engine_ptr)
{
	register byte	*ptr;
	byte	*arg_ptr;
	Pc_log	*tmp_pc_ptr;
	byte	*stat;
	word32	wstat;
	word32	arg;
#ifndef _DEBUG
	register word32	kpc;
#endif
	register word32	acc;
	register word32	xreg;
	register word32	yreg;
	word32	stack;
	word32	dbank;
	register word32	direct;
	register word32	psr;
	register word32	zero;
	register word32	neg;
	word32	getmem_tmp;
	word32	save_addr;
	word32	pull_tmp;
	word32	tmp_bytes;
	double	fcycles;
	Fplus	*fplus_ptr;
	double	fplus_1;
	double	fplus_2;
	double	fplus_3;
	double	fplus_x_m1;
	double	fcycles_tmp1;

	word32	opcode;
	register word32	addr;
	word32	addr_latch;
	word32	tmp1, tmp2;


	tmp_pc_ptr = 0;

	kpc = engine_ptr->kpc;
	acc = engine_ptr->acc;
	xreg = engine_ptr->xreg;
	yreg = engine_ptr->yreg;
	stack = engine_ptr->stack;
	dbank = engine_ptr->dbank;
	direct = engine_ptr->direct;
	psr = engine_ptr->psr;
	fcycles = engine_ptr->fcycles;
	fplus_ptr = engine_ptr->fplus_ptr;
	zero = !(psr & 2);
	neg = (psr >> 7) & 1;

	fplus_1 = fplus_ptr->plus_1;
	fplus_2 = fplus_ptr->plus_2;
	fplus_3 = fplus_ptr->plus_3;
	fplus_x_m1 = fplus_ptr->plus_x_minus_1;

	g_ret1 = 0;

recalc_accsize:
	if(psr & 0x20) {
		while(fcycles <= g_fcycles_stop) {
#if 0
			if((neg & ~1) || (psr & (~0x1ff))) {
				halt_printf("psr = %04x\n", psr);
			}
#endif

			FETCH_OPCODE;

#ifdef LOG_PC
			LOG_PC_MACRO();
#endif

			switch(opcode) {
			default:
				halt_printf("acc8 unk op: %02x\n", opcode);
				arg = 9
#define ACC8
#include "defs_instr.h"
				* 2;
				break;
#include "8inst_c"
				break;
			}
		}
	} else {
		while(fcycles <= g_fcycles_stop) {
			FETCH_OPCODE;
#ifdef LOG_PC
			LOG_PC_MACRO();
#endif

			switch(opcode) {
			default:
				halt_printf("acc16 unk op: %02x\n", opcode);
				arg = 9
#undef ACC8
#include "defs_instr.h"
				* 2;
				break;
#include "16inst_c"
				break;
			}
		}
	}

finish:
	engine_ptr->kpc = kpc;
	engine_ptr->acc = acc;
	engine_ptr->xreg = xreg;
	engine_ptr->yreg = yreg;
	engine_ptr->stack = stack;
	engine_ptr->dbank = dbank;
	engine_ptr->direct = direct;
	engine_ptr->fcycles = fcycles;

	psr = psr & (~0x82);
	psr |= (neg << 7);
	psr |= ((!zero) << 1);

	engine_ptr->psr = psr;

	return (g_ret1 << 28);
}


int	g_engine_c_mode = 1;

int defs_instr_start_8 = 0;
int defs_instr_end_8 = 0;
int defs_instr_start_16 = 0;
int defs_instr_end_16 = 0;
int op_routs_start = 0;
int op_routs_end = 0;


