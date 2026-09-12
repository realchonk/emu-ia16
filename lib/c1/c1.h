#ifndef FILE_C1_H
#define FILE_C1_H
#include <stddef.h>

#define NSPOOL	256	/* maximum number of stmts */
#define NEPOOL	1024	/* maximum number of exprs */
#define NLPOOL	256	/* maximum number of local variables */

typedef unsigned char	byte;
typedef unsigned short	word;
typedef unsigned long	dword;

struct expr {
	struct expr	*e_next;
	size_t		 e_off;
	byte		 e_type;
	union {
		word	 ev_six;
		struct {
			word	i_ty;
			word	i_v;
		} ev_i;
		struct {
			word	I_ty;
			dword	I_v;
		} ev_I;
		struct {
			word	g_ty;
			word	g_sym;
		} ev_g;
		struct {
			word	l_ty;
			byte	l_slot;
		} ev_l;
		struct {
			word		 u_ty;
			struct expr	*u_e;
			byte		 u_op;
		} ev_u;
		struct {
			word		 b_ty;
			struct expr	*b_l, *b_r;
			byte		 b_op;
		} ev_b;
		struct {
			word		 t_ty;
			struct expr	*t_c, *t_t, *t_f;
		} ev_t;
		struct {
			word		 c_ty;
			struct expr	*c_f, *c_a;
			byte		 c_n;
		} ev_c;
		struct {
			word		 a_ty;
			struct expr	*a_d, *a_s;
		} ev_a;
		struct {
			struct expr	*n_e;
			word		 n_ty;
			word		 n_scale;
			byte		 n_op;
			byte		 n_when;
		} ev_n;
		struct {
			word		 C_ty;
			struct expr	*C_l, *C_r;
		} ev_C;
		struct {
			word		 d_ty;
			struct expr	*d_e;
		} ev_d;
	} e_v;
};

#define e_six	e_v.ev_six
#define e_i	e_v.ev_i
#define e_I	e_v.ev_I
#define e_g	e_v.ev_g
#define e_l	e_v.ev_l
#define e_u	e_v.ev_u
#define e_b	e_v.ev_b
#define e_t	e_v.ev_t
#define e_c	e_v.ev_c
#define e_a	e_v.ev_a
#define e_n	e_v.ev_n
#define e_C	e_v.ev_C
#define e_d	e_v.ev_d

struct stmt {
	struct stmt	*s_next;
	size_t		 s_off;
	byte		 s_type;
	union {
		word	 	 sv_sym;
		struct expr	*sv_e;
		struct {
			word		 R_ty;
			struct expr	*R_e;
		} sv_R;
		struct {
			struct expr	*B_c;
			word		 B_t, B_f;
		} sv_B;
	} s_v;
};

extern struct stmt	spool[NSPOOL];
extern struct expr	epool[NEPOOL];
extern word		lpool[NLPOOL];

#define s_sym	s_v.sv_sym
#define s_e	s_v.sv_e
#define s_R	s_v.sv_R
#define s_B	s_v.sv_B

#endif /* FILE_C1_H */
