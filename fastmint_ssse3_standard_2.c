/* -*- Mode: C; c-file-style: "stroustrup" -*- */
/* Translated altivec4x2 -> ssse3 by pseudaria(pseudaria@gmail.com)  */

#include "libfastmint.h"
#include <tmmintrin.h>

#if (defined(__i386__) || defined(__amd64__)) && defined(__GNUC__) && defined(__SSSE3__)
typedef int ssse3_d_t __attribute__ ((vector_size (16)));
typedef int ssse3_q_t __attribute__ ((vector_size (16)));
typedef long long int v2di __attribute__ ((vector_size (16)));
#endif

int minter_ssse3_standard_2_test( void ) {
    /* This minter runs only on x86 and AMD64 hardware supporting SSSE3 - and will only compile on GCC */
#if !defined( COMPACT )
#if (defined(__i386__) || defined(__amd64__)) && defined(__GNUC__) && defined(__SSSE3__)
    return (gProcessorSupportFlags & HC_CPU_SUPPORTS_SSSE3) != 0;
#endif
#else  
    /* Not an x86 or AMD64, or compiler doesn't support SSSE3 or GNU assembly */
    return 0;
#endif
}

/* Define low-level primitives in terms of operations */
/* #define S(n, X) ( ( (X) << (n) ) | ( (X) >> ( 32 - (n) ) ) ) */

#define XOR(a,b) ( (ssse3_d_t) (((ssse3_q_t) a) ^ ((ssse3_q_t) b)))
#define AND(a,b) ( (ssse3_d_t) (((ssse3_q_t) a) & ((ssse3_q_t) b)))
#define ANDNOT(a,m) ( (ssse3_d_t) (~((ssse3_q_t) m) & (ssse3_q_t) a )) //reverse order
#define OR(a,b) ( (ssse3_d_t) (((ssse3_q_t) a) | ((ssse3_q_t) b)))
#define ADD(a,b) ( (ssse3_d_t) __builtin_ia32_paddd128( (ssse3_q_t) a,(ssse3_q_t) b) )

/* Altivec -> SSE */
#define VEC_SEL(a,b,c) ( OR(AND(c,b), ANDNOT(a,c)) )
#define VEC_CMPGT(a,b) ( (ssse3_d_t) __builtin_ia32_pcmpgtd128((ssse3_q_t) (a+0x80000000), (ssse3_q_t) (b+0x80000000)))
#define VEC_CMPEQ(a,b) ( (ssse3_d_t) __builtin_ia32_pcmpeqd128((ssse3_q_t) a, (ssse3_q_t) b))

#if (defined(__i386__) || defined(__amd64__)) && defined(__GNUC__) && defined(__SSSE3__)
static inline ssse3_d_t S1(ssse3_d_t X)
{
    ssse3_d_t G = {} ;

    asm ("movdqa %[x],%[g]\n\t"
	 "pslld %[sl],%[x]\n\t"
	 "psrld %[sr],%[g]\n\t"
	 "por %[g],%[x]"
	 : [g] "=x" (G), [x] "=x" (X)
	 : "[x]" (X), [sl] "g" (1), [sr] "g" (31)
	);

    return X;
}
#endif

/* #define F1( B, C, D ) ( ( (B) & (C) ) | ( ~(B) & (D) ) ) */
/* #define F1( B, C, D ) ( (D) ^ ( (B) & ( (C) ^ (D) ) ) ) */
#define F1( B, C, D ) (				\
	F = AND(B,C),				\
	G = ANDNOT(D,B),			\
	OR(F,G) )
/* #define F2( B, C, D ) ( (B) ^ (C) ^ (D) ) */
#define F2( B, C, D ) (				\
	F = XOR(B,C),				\
	XOR(F,D) )
/* #define F3( B, C, D ) ( (B) & (C) ) | ( (C) & (D) ) | ( (B) & (D) ) */
/* #define F3( B, C, D ) ( ( (B) & ( (C) | (D) )) | ( (C) & (D) ) ) */
#define F3( B, C, D ) (				\
	F = OR(C,D),				\
	G = AND(C,D),				\
	F = AND(B,F),				\
	OR(F,G) )
/* #define F4( B, C, D ) ( (B) ^ (C) ^ (D) ) */
#define F4(B,C,D) F2(B,C,D)

#define K1 0x5A827999  /* constant used for rounds 0..19 */
#define K2 0x6ED9EBA1  /* constant used for rounds 20..39 */
#define K3 0x8F1BBCDC  /* constant used for rounds 40..59 */
#define K4 0xCA62C1D6  /* constant used for rounds 60..79 */

/* #define Wf(t) (W[t] = S(1, W[t-16] ^ W[t-14] ^ W[t-8] ^ W[t-3])) */
#define Wf(W,t,F,G) (				\
	F = XOR((W)[t-16], (W)[t-14]),		\
	G = XOR((W)[t-8], (W)[t-3]),		\
	F = XOR(F,G),				\
	(W)[t] = S1(F) )

#define Wfly(W,t,F,G) ( (t) < 16 ? (W)[t] : Wf(W,t,F,G) )

/*
  #define ROUND(t,A,B,C,D,E,Func,K,W) \
  E = ADD(E,K); \
  F = S(5,A); \
  E = ADD(F,E); \
  F = Wfly(W,t); \
  E = ADD(F,E); \
  F = Func(B,C,D); \
  E = ADD(F,E); \
  B = S(30,B);
*/

#define ROUND_F1_n(t,A,B,C,D,E,K,W)					\
    asm (								\
	"\n\t movdqa  %[d1], %%xmm5" /* begin F1(B1,C1,D1) */		\
	"\n\t movdqa  %[d2], %%xmm13" /* begin F1(B2,C2,D2) */		\
	"\n\t pxor  %[c1], %%xmm5"					\
	"\n\t pxor  %[c2], %%xmm13"					\
	"\n\t movdqa  %[a1], %%xmm7" /* begin S(5,A1) */		\
	"\n\t movdqa  %[a2], %%xmm15" /* begin S(5,A2) */		\
	"\n\t pand  %[b1], %%xmm5"					\
	"\n\t pand  %[b2], %%xmm13"					\
	"\n\t pslld $5,   %%xmm7"					\
	"\n\t pslld $5,   %%xmm15"					\
	"\n\t pxor  %[d1], %%xmm5"					\
	"\n\t pxor  %[d2], %%xmm13"					\
	"\n\t movdqa  %[a1], %%xmm6"					\
	"\n\t movdqa  %[a2], %%xmm14"					\
	"\n\t paddd %%xmm5,%[e1]"  /* sum F1(B,C,D) to E1 */		\
	"\n\t paddd %%xmm13,%[e2]"  /* sum F1(B,C,D) to E2 */		\
	"\n\t psrld $27,  %%xmm6"					\
	"\n\t psrld $27,  %%xmm14"					\
	"\n\t paddd %[k], %[e1]"  /* sum K to E */			\
	"\n\t paddd %[k], %[e2]"  /* sum K to E */			\
	"\n\t por   %%xmm6,%%xmm7"					\
	"\n\t por   %%xmm14,%%xmm15"					\
	"\n\t movdqa  %[b1], %%xmm5" /* begin S(30,B1) */		\
	"\n\t movdqa  %[b2], %%xmm13" /* begin S(30,B2) */		\
	"\n\t paddd %%xmm7,%[e1]"  /* sum S(5,A1) to E1 */		\
	"\n\t paddd %%xmm15,%[e2]"  /* sum S(5,A2) to E2 */		\
	"\n\t pslld $30,  %[b1]"					\
	"\n\t pslld $30,  %[b2]"					\
	"\n\t psrld $2,   %%xmm5"					\
	"\n\t psrld $2,   %%xmm13"					\
	"\n\t paddd %[W1t],%[e1]"  /* sum W1[t] to E1 */		\
	"\n\t paddd %[W2t],%[e2]"  /* sum W2[t] to E2 */		\
	"\n\t por   %%xmm5,%[b1]"  /* complete S(30,B1) */		\
	"\n\t por   %%xmm13,%[b2]"  /* complete S(30,B2) */		\
	: [a1] "+x" (A##1), [b1] "+x" (B##1), [c1] "+x" (C##1), [d1] "+x" (D##1), [e1] "+x" (E##1), \
	  [a2] "+x" (A##2), [b2] "+x" (B##2), [c2] "+x" (C##2), [d2] "+x" (D##2), [e2] "+x" (E##2) \
	: [W1t] "m" ((W##1)[t]), [W2t] "m" ((W##2)[t]), [k] "m" (K)	\
	: "xmm5", "xmm6", "xmm7", "xmm13", "xmm14", "xmm15" );

#define ROUND_F1_u(t,A,B,C,D,E,K,W)					\
    asm (								\
	"\n\t movdqa  %[d1], %%xmm5" /* begin F1(B1,C1,D1) */		\
	"\n\t movdqa  %[d2], %%xmm13" /* begin F1(B2,C2,D2) */		\
	"\n\t pxor  %[c1], %%xmm5"					\
	"\n\t pxor  %[c2], %%xmm13"					\
	"\n\t movdqa  %[a1], %%xmm7" /* begin S(5,A1) */		\
	"\n\t movdqa  %[a2], %%xmm15" /* begin S(5,A2) */		\
	"\n\t pand  %[b1], %%xmm5"					\
	"\n\t pand  %[b2], %%xmm13"					\
	"\n\t pslld $5,   %%xmm7"					\
	"\n\t pslld $5,   %%xmm15"					\
	"\n\t pxor  %[d1], %%xmm5"					\
	"\n\t pxor  %[d2], %%xmm13"					\
	"\n\t movdqa  %[a1], %%xmm6"					\
	"\n\t movdqa  %[a2], %%xmm14"					\
	"\n\t paddd %%xmm5,%[e1]"  /* sum F1(B1,C1,D1) to E1 */		\
	"\n\t paddd %%xmm13,%[e2]"  /* sum F1(B2,C2,D2) to E2 */	\
	"\n\t psrld $27,  %%xmm6"					\
	"\n\t psrld $27,  %%xmm14"					\
	"\n\t paddd %[k], %[e1]"  /* sum K to E1 */			\
	"\n\t paddd %[k], %[e2]"  /* sum K to E2 */			\
	"\n\t por   %%xmm6,%%xmm7"					\
	"\n\t por   %%xmm14,%%xmm15"					\
	"\n\t movdqa  %[b1], %%xmm5" /* begin S(30,B1) */		\
	"\n\t movdqa  %[b2], %%xmm13" /* begin S(30,B2) */		\
	"\n\t paddd %%xmm7,%[e1]"  /* sum S(5,A1) to E1 */		\
	"\n\t paddd %%xmm15,%[e2]"  /* sum S(5,A2) to E2 */		\
	"\n\t movdqu  (%[W1t_3]),%%xmm7" /* begin W1f(t) */		\
	"\n\t movdqu  (%[W2t_3]),%%xmm15" /* begin W2f(t) */		\
	"\n\t movdqu  -80(%[W1t_3]),%%xmm6"				\
	"\n\t movdqu  -80(%[W2t_3]),%%xmm14"				\
	"\n\t pxor  -176(%[W1t_3]),%%xmm7"				\
	"\n\t pxor  -176(%[W2t_3]),%%xmm15"				\
	"\n\t pxor  -208(%[W1t_3]),%%xmm6"				\
	"\n\t pxor  -208(%[W2t_3]),%%xmm14"				\
	"\n\t pslld $30,  %[b1]"					\
	"\n\t pslld $30,  %[b2]"					\
	"\n\t pxor  %%xmm6,%%xmm7"					\
	"\n\t pxor  %%xmm14,%%xmm15"					\
	"\n\t movdqa  %%xmm7,%%xmm6"					\
	"\n\t movdqa  %%xmm15,%%xmm14"					\
	"\n\t pslld $1,   %%xmm7"					\
	"\n\t pslld $1,   %%xmm15"					\
	"\n\t psrld $31,  %%xmm6"					\
	"\n\t psrld $31,  %%xmm14"					\
	"\n\t psrld $2,   %%xmm5"					\
	"\n\t psrld $2,   %%xmm13"					\
	"\n\t por   %%xmm6,%%xmm7"					\
	"\n\t por   %%xmm14,%%xmm15"					\
	"\n\t por   %%xmm5,%[b1]"  /* complete S(30,B) */		\
	"\n\t por   %%xmm13,%[b2]"  /* complete S(30,B) */		\
	"\n\t paddd %%xmm7,%[e1]"  /* sum Wf(t) to E */			\
	"\n\t paddd %%xmm15,%[e2]"  /* sum Wf(t) to E */		\
	"\n\t movdqa  %%xmm7,%[W1t]" /* write back W1f(t) to W1[t] */	\
	"\n\t movdqa  %%xmm15,%[W2t]" /* write back W2f(t) to W2[t] */	\
	: [a1] "+x" (A##1), [b1] "+x" (B##1), [c1] "+x" (C##1), [d1] "+x" (D##1), [e1] "+x" (E##1), \
	  [a2] "+x" (A##2), [b2] "+x" (B##2), [c2] "+x" (C##2), [d2] "+x" (D##2), [e2] "+x" (E##2), [W1t] "=m" ((W##1)[t]), [W2t] "=m" ((W##2)[t]) \
	: [W1t_3] "r" (W##1+t-3), [W2t_3] "r" (W##2+t-3), [k] "m" (K)	\
	: "xmm5", "xmm6", "xmm7", "xmm13", "xmm14", "xmm15" );

#define ROUND_F2_n(t,A,B,C,D,E,K,W)					\
    asm (								\
	 "\n\t movdqa  %[b1], %%xmm5" /* begin F2(B1,C1,D1) */		\
	 "\n\t movdqa  %[b2], %%xmm13" /* begin F2(B2,C2,D2) */		\
	 "\n\t movdqa  %[a1], %%xmm7" /* begin S(5,A1) */		\
	 "\n\t movdqa  %[a2], %%xmm15" /* begin S(5,A2) */		\
	 "\n\t pxor  %[c1], %%xmm5"					\
	 "\n\t pxor  %[c2], %%xmm13"					\
	 "\n\t pslld $5,   %%xmm7"					\
	 "\n\t pslld $5,   %%xmm15"					\
	 "\n\t pxor  %[d1], %%xmm5"					\
	 "\n\t pxor  %[d2], %%xmm13"					\
	 "\n\t movdqa  %[a1], %%xmm6"					\
	 "\n\t movdqa  %[a2], %%xmm14"					\
	 "\n\t paddd %%xmm5,%[e1]"  /* sum F2(B1,C1,D1) to E1 */	\
	 "\n\t paddd %%xmm13,%[e2]"  /* sum F2(B2,C2,D2) to E2 */	\
	 "\n\t psrld $27,  %%xmm6"					\
	 "\n\t psrld $27,  %%xmm14"					\
	 "\n\t paddd %[k], %[e1]"  /* sum K to E1 */			\
	 "\n\t paddd %[k], %[e2]"  /* sum K to E2 */			\
	 "\n\t por   %%xmm6,%%xmm7"					\
	 "\n\t por   %%xmm14,%%xmm15"					\
	 "\n\t movdqa  %[b1], %%xmm5" /* begin S(30,B) */		\
	 "\n\t movdqa  %[b2], %%xmm13" /* begin S(30,B) */		\
	 "\n\t paddd %%xmm7,%[e1]"  /* sum S(5,A1) to E1 */		\
	 "\n\t paddd %%xmm15,%[e2]"  /* sum S(5,A2) to E2 */		\
	 "\n\t pslld $30,  %[b1]"					\
	 "\n\t pslld $30,  %[b2]"					\
	 "\n\t psrld $2,   %%xmm5"					\
	 "\n\t psrld $2,   %%xmm13"					\
	 "\n\t paddd %[W1t],%[e1]"  /* sum W[t] to E */			\
	 "\n\t paddd %[W2t],%[e2]"  /* sum W[t] to E */			\
	 "\n\t por   %%xmm5,%[b1]"  /* complete S(30,B) */		\
	 "\n\t por   %%xmm13,%[b2]"  /* complete S(30,B) */		\
	 : [a1] "+x" (A##1), [b1] "+x" (B##1), [c1] "+x" (C##1), [d1] "+x" (D##1), [e1] "+x" (E##1), \
	   [a2] "+x" (A##2), [b2] "+x" (B##2), [c2] "+x" (C##2), [d2] "+x" (D##2), [e2] "+x" (E##2) \
	 : [W1t] "m" ((W##1)[t]), [W2t] "m" ((W##2)[t]), [k] "m" (K)	\
	 : "xmm5", "xmm6", "xmm7", "xmm13", "xmm14", "xmm15" );

#define ROUND_F2_u(t,A,B,C,D,E,K,W)					\
    asm (								\
	 "\n\t movdqa  %[b1], %%xmm5" /* begin F2(B1,C1,D1) */		\
	 "\n\t movdqa  %[b2], %%xmm13" /* begin F2(B2,C2,D2) */		\
	 "\n\t movdqa  %[a1], %%xmm7" /* begin S(5,A1) */		\
	 "\n\t movdqa  %[a2], %%xmm15" /* begin S(5,A2) */		\
	 "\n\t pxor  %[c1], %%xmm5"					\
	 "\n\t pxor  %[c2], %%xmm13"					\
	 "\n\t pslld $5,   %%xmm7"					\
	 "\n\t pslld $5,   %%xmm15"					\
	 "\n\t pxor  %[d1], %%xmm5"					\
	 "\n\t pxor  %[d2], %%xmm13"					\
	 "\n\t movdqa  %[a1], %%xmm6"					\
	 "\n\t movdqa  %[a2], %%xmm14"					\
	 "\n\t paddd %%xmm5,%[e1]"  /* sum F2(B1,C1,D1) to E1 */	\
	 "\n\t paddd %%xmm13,%[e2]"  /* sum F2(B2,C2,D2) to E2 */	\
	 "\n\t psrld $27,  %%xmm6"					\
	 "\n\t psrld $27,  %%xmm14"					\
	 "\n\t paddd %[k], %[e1]"  /* sum K to E1 */			\
	 "\n\t paddd %[k], %[e2]"  /* sum K to E2 */			\
	 "\n\t por   %%xmm6,%%xmm7"					\
	 "\n\t por   %%xmm14,%%xmm15"					\
	 "\n\t movdqa  %[b1], %%xmm5" /* begin S(30,B1) */		\
	 "\n\t movdqa  %[b2], %%xmm13" /* begin S(30,B2) */		\
	 "\n\t paddd %%xmm7,%[e1]"  /* sum S(5,A1) to E1 */		\
	 "\n\t paddd %%xmm15,%[e2]"  /* sum S(5,A2) to E2 */		\
	 "\n\t movdqu  (%[W1t_3]),%%xmm7" /* begin W1f(t) */		\
	 "\n\t movdqu  (%[W2t_3]),%%xmm15" /* begin W2f(t) */		\
	 "\n\t movdqu  -80(%[W1t_3]),%%xmm6"				\
	 "\n\t movdqu  -80(%[W2t_3]),%%xmm14"				\
	 "\n\t pxor  -176(%[W1t_3]),%%xmm7"				\
	 "\n\t pxor  -176(%[W2t_3]),%%xmm15"				\
	 "\n\t pxor  -208(%[W1t_3]),%%xmm6"				\
	 "\n\t pxor  -208(%[W2t_3]),%%xmm14"				\
	 "\n\t pslld $30,  %[b1]"					\
	 "\n\t pslld $30,  %[b2]"					\
	 "\n\t pxor  %%xmm6,%%xmm7"					\
	 "\n\t pxor  %%xmm14,%%xmm15"					\
	 "\n\t movdqa  %%xmm7,%%xmm6"					\
	 "\n\t movdqa  %%xmm15,%%xmm14"					\
	 "\n\t pslld $1,   %%xmm7"					\
	 "\n\t pslld $1,   %%xmm15"					\
	 "\n\t psrld $31,  %%xmm6"					\
	 "\n\t psrld $31,  %%xmm14"					\
	 "\n\t psrld $2,   %%xmm5"					\
	 "\n\t psrld $2,   %%xmm13"					\
	 "\n\t por   %%xmm6,%%xmm7"					\
	 "\n\t por   %%xmm14,%%xmm15"					\
	 "\n\t por   %%xmm5,%[b1]"  /* complete S(30,B1) */		\
	 "\n\t por   %%xmm13,%[b2]"  /* complete S(30,B2) */		\
	 "\n\t paddd %%xmm7,%[e1]"  /* sum W1f(t) to E1 */		\
	 "\n\t paddd %%xmm15,%[e2]"  /* sum W2f(t) to E2 */		\
	 "\n\t movdqa  %%xmm7,%[W1t]" /* write back W1f(t) to W1[t] */	\
	 "\n\t movdqa  %%xmm15,%[W2t]" /* write back W2f(t) to W2[t] */ \
	 : [a1] "+x" (A##1), [b1] "+x" (B##1), [c1] "+x" (C##1), [d1] "+x" (D##1), [e1] "+x" (E##1), \
	     [a2] "+x" (A##2), [b2] "+x" (B##2), [c2] "+x" (C##2), [d2] "+x" (D##2), [e2] "+x" (E##2), [W1t] "=m" ((W##1)[t]), [W2t] "=m" ((W##2)[t]) \
	     : [W1t_3] "r" (W##1+t-3), [W2t_3] "r" (W##2+t-3), [k] "m" (K) \
	     : "xmm5", "xmm6", "xmm7", "xmm13", "xmm14", "xmm15" );

#define ROUND_F3_n(t,A,B,C,D,E,K,W)					\
    asm (								\
	"\n\t movdqa  %[d1], %%xmm5" /* begin F3(B1,C1,D1) */		\
	"\n\t movdqa  %[d2], %%xmm13" /* begin F3(B2,C2,D2) */		\
	"\n\t movdqa  %[d1], %%xmm6"					\
	"\n\t movdqa  %[d2], %%xmm14"					\
	"\n\t por   %[c1], %%xmm5"					\
	"\n\t por   %[c2], %%xmm13"					\
	"\n\t pand  %[c1], %%xmm6"					\
	"\n\t pand  %[c2], %%xmm14"					\
	"\n\t movdqa  %[a1], %%xmm7" /* begin S(5,A1) */		\
	"\n\t movdqa  %[a2], %%xmm15" /* begin S(5,A2) */		\
	"\n\t pand  %[b1], %%xmm5"					\
	"\n\t pand  %[b2], %%xmm13"					\
	"\n\t pslld $5,   %%xmm7"					\
	"\n\t pslld $5,   %%xmm15"					\
	"\n\t por   %%xmm6,%%xmm5"					\
	"\n\t por   %%xmm14,%%xmm13"					\
	"\n\t movdqa  %[a1], %%xmm6"					\
	"\n\t movdqa  %[a2], %%xmm14"					\
	"\n\t paddd %%xmm5,%[e1]"  /* sum F3(B1,C1,D1) to E1 */		\
	"\n\t paddd %%xmm13,%[e2]"  /* sum F3(B2,C2,D2) to E2 */	\
	"\n\t psrld $27,  %%xmm6"					\
	"\n\t psrld $27,  %%xmm14"					\
	"\n\t paddd %[k], %[e1]"  /* sum K to E1 */			\
	"\n\t paddd %[k], %[e2]"  /* sum K to E2 */			\
	"\n\t por   %%xmm6,%%xmm7"					\
	"\n\t por   %%xmm14,%%xmm15"					\
	"\n\t movdqa  %[b1], %%xmm5" /* begin S(30,B1) */		\
	"\n\t movdqa  %[b2], %%xmm13" /* begin S(30,B2) */		\
	"\n\t paddd %%xmm7,%[e1]"  /* sum S(5,A1) to E1 */		\
	"\n\t paddd %%xmm15,%[e2]"  /* sum S(5,A2) to E2 */		\
	"\n\t pslld $30,  %[b1]"					\
	"\n\t pslld $30,  %[b2]"					\
	"\n\t psrld $2,   %%xmm5"					\
	"\n\t psrld $2,   %%xmm13"					\
	"\n\t paddd %[W1t],%[e1]"  /* sum W1[t] to E1 */		\
	"\n\t paddd %[W2t],%[e2]"  /* sum W2[t] to E2 */		\
	"\n\t por   %%xmm5,%[b1]"  /* complete S(30,B1) */		\
	"\n\t por   %%xmm13,%[b2]"  /* complete S(30,B2) */		\
	: [a1] "+x" (A##1), [b1] "+x" (B##1), [c1] "+x" (C##1), [d1] "+x" (D##1), [e1] "+x" (E##1), \
	  [a2] "+x" (A##2), [b2] "+x" (B##2), [c2] "+x" (C##2), [d2] "+x" (D##2), [e2] "+x" (E##2) \
	: [W1t] "m" ((W##1)[t]), [W2t] "m" ((W##2)[t]), [k] "m" (K)	\
	: "xmm5", "xmm6", "xmm7", "xmm13", "xmm14", "xmm15" );

#define ROUND_F3_u(t,A,B,C,D,E,K,W)					\
    asm (								\
	"\n\t movdqa  %[d1], %%xmm5" /* begin F3(B1,C1,D1) */		\
	"\n\t movdqa  %[d2], %%xmm13" /* begin F3(B2,C2,D2) */		\
	"\n\t movdqa  %[d1], %%xmm6"					\
	"\n\t movdqa  %[d2], %%xmm14"					\
	"\n\t por   %[c1], %%xmm5"					\
	"\n\t por   %[c2], %%xmm13"					\
	"\n\t pand  %[c1], %%xmm6"					\
	"\n\t pand  %[c2], %%xmm14"					\
	"\n\t movdqa  %[a1], %%xmm7" /* begin S(5,A1) */		\
	"\n\t movdqa  %[a2], %%xmm15" /* begin S(5,A2) */		\
	"\n\t pand  %[b1], %%xmm5"					\
	"\n\t pand  %[b2], %%xmm13"					\
	"\n\t pslld $5,   %%xmm7"					\
	"\n\t pslld $5,   %%xmm15"					\
	"\n\t por   %%xmm6,%%xmm5"					\
	"\n\t por   %%xmm14,%%xmm13"					\
	"\n\t movdqa  %[a1], %%xmm6"					\
	"\n\t movdqa  %[a2], %%xmm14"					\
	"\n\t paddd %%xmm5,%[e1]"  /* sum F3(B1,C1,D1) to E1 */		\
	"\n\t paddd %%xmm13,%[e2]"  /* sum F3(B2,C2,D2) to E2 */	\
	"\n\t psrld $27,  %%xmm6"					\
	"\n\t psrld $27,  %%xmm14"					\
	"\n\t paddd %[k], %[e1]"  /* sum K to E1 */			\
	"\n\t paddd %[k], %[e2]"  /* sum K to E2 */			\
	"\n\t por   %%xmm6,%%xmm7"					\
	"\n\t por   %%xmm14,%%xmm15"					\
	"\n\t movdqa  %[b1], %%xmm5" /* begin S(30,B1) */		\
	"\n\t movdqa  %[b2], %%xmm13" /* begin S(30,B2) */		\
	"\n\t paddd %%xmm7,%[e1]"  /* sum S(5,A1) to E1 */		\
	"\n\t paddd %%xmm15,%[e2]"  /* sum S(5,A2) to E2 */		\
	"\n\t movdqu  (%[W1t_3]),%%xmm7" /* begin W1f(t) */		\
	"\n\t movdqu  (%[W2t_3]),%%xmm15" /* begin W2f(t) */		\
	"\n\t movdqu  -80(%[W1t_3]),%%xmm6"				\
	"\n\t movdqu  -80(%[W2t_3]),%%xmm14"				\
	"\n\t pxor  -176(%[W1t_3]),%%xmm7"				\
	"\n\t pxor  -176(%[W2t_3]),%%xmm15"				\
	"\n\t pxor  -208(%[W1t_3]),%%xmm6"				\
	"\n\t pxor  -208(%[W2t_3]),%%xmm14"				\
	"\n\t pslld $30,  %[b1]"					\
	"\n\t pslld $30,  %[b2]"					\
	"\n\t pxor  %%xmm6,%%xmm7"					\
	"\n\t pxor  %%xmm14,%%xmm15"					\
	"\n\t movdqa  %%xmm7,%%xmm6"					\
	"\n\t movdqa  %%xmm15,%%xmm14"					\
	"\n\t pslld $1,   %%xmm7"					\
	"\n\t pslld $1,   %%xmm15"					\
	"\n\t psrld $31,  %%xmm6"					\
	"\n\t psrld $31,  %%xmm14"					\
	"\n\t psrld $2,   %%xmm5"					\
	"\n\t psrld $2,   %%xmm13"					\
	"\n\t por   %%xmm6,%%xmm7"					\
	"\n\t por   %%xmm14,%%xmm15"					\
	"\n\t por   %%xmm5,%[b1]"  /* complete S(30,B1) */		\
	"\n\t por   %%xmm13,%[b2]"  /* complete S(30,B2) */		\
	"\n\t paddd %%xmm7,%[e1]"  /* sum W1f(t) to E1 */		\
	"\n\t paddd %%xmm15,%[e2]"  /* sum W2f(t) to E2 */		\
	"\n\t movdqa  %%xmm7,%[W1t]" /* write back W1f(t) to W1[t] */	\
	"\n\t movdqa  %%xmm15,%[W2t]" /* write back W2f(t) to W2[t] */	\
	: [a1] "+x" (A##1), [b1] "+x" (B##1), [c1] "+x" (C##1), [d1] "+x" (D##1), [e1] "+x" (E##1), \
	  [a2] "+x" (A##2), [b2] "+x" (B##2), [c2] "+x" (C##2), [d2] "+x" (D##2), [e2] "+x" (E##2), [W1t] "=m" ((W##1)[t]), [W2t] "=m" ((W##2)[t]) \
	: [W1t_3] "r" (W##1+t-3), [W2t_3] "r" (W##2+t-3), [k] "m" (K)	\
	: "xmm5", "xmm6", "xmm7", "xmm13", "xmm14", "xmm15" );

#define ROUND_F4_u ROUND_F2_u
#define ROUND_F4_n ROUND_F2_n

#define ROUNDu(t,A,B,C,D,E,Func,K)		\
    if((t) < 16) {				\
	ROUND_##Func##_n(t,A,B,C,D,E,K,W);	\
    } else {					\
	ROUND_##Func##_u(t,A,B,C,D,E,K,W);	\
    }

#define ROUNDn(t,A,B,C,D,E,Func,K)		\
    ROUND_##Func##_n(t,A,B,C,D,E,K,W);		\

#define ROUND5( t, Func, K )			\
    ROUNDu( t + 0, A, B, C, D, E, Func, K );	\
    ROUNDu( t + 1, E, A, B, C, D, Func, K );	\
    ROUNDu( t + 2, D, E, A, B, C, Func, K );	\
    ROUNDu( t + 3, C, D, E, A, B, Func, K );	\
    ROUNDu( t + 4, B, C, D, E, A, Func, K );

#if defined(MINTER_CALLBACK_CLEANUP_FP)
#undef MINTER_CALLBACK_CLEANUP_FP
#endif
#define MINTER_CALLBACK_CLEANUP_FP __builtin_ia32_emms()

unsigned long minter_ssse3_standard_2(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS)
{
#if !defined( COMPACT )
#if (defined(__i386__) || defined(__amd64__)) && defined(__GNUC__) && defined(__SSSE3__)
    MINTER_CALLBACK_VARS;
    unsigned long iters = 0 ;
    int n = 0, t = 0, gotBits = 0, maxBits = (bits > 16) ? 16 : bits;
    uInt32 bitMask1Low = 0 , bitMask1High = 0 , s = 0 ;
    ssse3_d_t vBitMaskHigh = {} , vBitMaskLow = {} ;
    register ssse3_d_t A1 = {} , B1 = {} , C1 = {} , D1 = {} , E1 = {}, A2 = {} , B2 = {} , C2 = {} , D2 = {} , E2 = {};

    ssse3_d_t Fa = {},Fb = {},Ga = {},Gb = {};
    ssse3_d_t MA1 = {}, MA2 = {}, MA = {}, MB1 = {}, MB2 = {}, MB = {};
    ssse3_d_t W1[80] = {} ,W2[80] = {} ;
    ssse3_d_t H[5] = {} , pH[5] = {} ;
    ssse3_d_t K[4] = {} ;
    ssse3_d_t M = {};
    ssse3_d_t Debug1 = {}, Debug2 = {},Debug3 = {},Debug4 = {},Debug5 = {},Debug6 = {},Debug7 = {},Debug8 = {};
    uInt32 *Hw = (uInt32*) H;
    uInt32 *pHw = (uInt32*) pH;
    uInt32 IA = 0, IB = 0, m = 0;
    const char *p = encodeAlphabets[EncodeBase64];
    unsigned char *X1 = (unsigned char*) W1;
    unsigned char *X2 = (unsigned char*) W2;
    unsigned char *output = (unsigned char*) block, *X;
	
    if ( *best > 0 ) { maxBits = *best+1; }

    /* Splat Kn constants into SSSE3-style array */
    ((uInt32*)K)[0] = ((uInt32*)K)[1] = ((uInt32*)K)[2] = ((uInt32*)K)[3] = K1;
    ((uInt32*)K)[4] = ((uInt32*)K)[5] = ((uInt32*)K)[6] = ((uInt32*)K)[7] = K2;
    ((uInt32*)K)[8] = ((uInt32*)K)[9] = ((uInt32*)K)[10] = ((uInt32*)K)[11] = K3;
    ((uInt32*)K)[12] = ((uInt32*)K)[13] = ((uInt32*)K)[14] = ((uInt32*)K)[15] = K4;
    
    /* Work out which bits to mask out for test */
    if(maxBits < 32) {
	if ( bits == 0 ) { bitMask1Low = 0; } else {
	    bitMask1Low = ~((((uInt32) 1) << (32 - maxBits)) - 1);
	}
	bitMask1High = 0;
    } else {
	bitMask1Low = ~0;
	bitMask1High = ~((((uInt32) 1) << (64 - maxBits)) - 1);
    }
    ((uInt32*) &vBitMaskLow )[0] = bitMask1Low ;
    ((uInt32*) &vBitMaskLow )[1] = bitMask1Low ;
    ((uInt32*) &vBitMaskHigh)[0] = bitMask1High;
    ((uInt32*) &vBitMaskHigh)[1] = bitMask1High;
    maxBits = 0;
    
    /* Copy block and IV to vectorised internal storage */
    /* Assume little-endian order, as we're on x86 or AMD64 */
    for(t=0; t < 16; t++) {
	X1[t*16+ 0] = X1[t*16+ 4] = X1[t*16+ 8] = X1[t*16+12] = output[t*4+3];
	X1[t*16+ 1] = X1[t*16+ 5] = X1[t*16+ 9] = X1[t*16+13] = output[t*4+2];
	X1[t*16+ 2] = X1[t*16+ 6] = X1[t*16+10] = X1[t*16+14] = output[t*4+1];
	X1[t*16+ 3] = X1[t*16+ 7] = X1[t*16+11] = X1[t*16+15] = output[t*4+0];
	
	X2[t*16+ 0] = X2[t*16+ 4] = X2[t*16+ 8] = X2[t*16+12] = output[t*4+3];
	X2[t*16+ 1] = X2[t*16+ 5] = X2[t*16+ 9] = X2[t*16+13] = output[t*4+2];
	X2[t*16+ 2] = X2[t*16+ 6] = X2[t*16+10] = X2[t*16+14] = output[t*4+1];
	X2[t*16+ 3] = X2[t*16+ 7] = X2[t*16+11] = X2[t*16+15] = output[t*4+0];
    }
    for(t=0; t < 5; t++) {
	Hw[t*4+0] = Hw[t*4+1] = Hw[t*4+2] = Hw[t*4+3] = pHw[t*4+0] = pHw[t*4+1] = pHw[t*4+2] = pHw[t*4+3] = IV[t];}
    
    /* The Tight Loop - everything in here should be extra efficient */
    for(iters=0; iters < maxIter-8; iters += 8) {
	
	/* Encode iteration count into tail */
	/* Iteration count is always 2-aligned, so only least-significant character needs multiple lookup */
	/* Further, we assume we're always little-endian */
	X1[(((tailIndex - 1) & ~3) << 2) + (((tailIndex - 1) & 3) ^ 3) +  0] = p[(iters & 0x3c) + 0];
	X2[(((tailIndex - 1) & ~3) << 2) + (((tailIndex - 1) & 3) ^ 3) +  0] = p[(iters & 0x3c) + 1];
	X1[(((tailIndex - 1) & ~3) << 2) + (((tailIndex - 1) & 3) ^ 3) +  4] = p[(iters & 0x3c) + 2];
	X2[(((tailIndex - 1) & ~3) << 2) + (((tailIndex - 1) & 3) ^ 3) +  4] = p[(iters & 0x3c) + 3];
	X1[(((tailIndex - 1) & ~3) << 2) + (((tailIndex - 1) & 3) ^ 3) +  8] = p[(iters & 0x3c) + 4];
	X2[(((tailIndex - 1) & ~3) << 2) + (((tailIndex - 1) & 3) ^ 3) +  8] = p[(iters & 0x3c) + 5];
	X1[(((tailIndex - 1) & ~3) << 2) + (((tailIndex - 1) & 3) ^ 3) + 12] = p[(iters & 0x3c) + 6];
	X2[(((tailIndex - 1) & ~3) << 2) + (((tailIndex - 1) & 3) ^ 3) + 12] = p[(iters & 0x3c) + 7];
		
	if(!(iters & 0x3f)) {
	    if ( iters >> 6 ) {
		X1[(((tailIndex - 2) & ~3) << 2) + (((tailIndex - 2) & 3) ^ 3) +  0] =
		    X1[(((tailIndex - 2) & ~3) << 2) + (((tailIndex - 2) & 3) ^ 3) +  4] =
		    X1[(((tailIndex - 2) & ~3) << 2) + (((tailIndex - 2) & 3) ^ 3) +  8] =
		    X1[(((tailIndex - 2) & ~3) << 2) + (((tailIndex - 2) & 3) ^ 3) + 12] =
		    X2[(((tailIndex - 2) & ~3) << 2) + (((tailIndex - 2) & 3) ^ 3) +  0] =
		    X2[(((tailIndex - 2) & ~3) << 2) + (((tailIndex - 2) & 3) ^ 3) +  4] =
		    X2[(((tailIndex - 2) & ~3) << 2) + (((tailIndex - 2) & 3) ^ 3) +  8] =
		    X2[(((tailIndex - 2) & ~3) << 2) + (((tailIndex - 2) & 3) ^ 3) + 12] = p[(iters >>  6) & 0x3f];
	    }
	    if ( iters >> 12 ) { 
		X1[(((tailIndex - 3) & ~3) << 2) + (((tailIndex - 3) & 3) ^ 3) +  0] =
		    X1[(((tailIndex - 3) & ~3) << 2) + (((tailIndex - 3) & 3) ^ 3) +  4] =
		    X1[(((tailIndex - 3) & ~3) << 2) + (((tailIndex - 3) & 3) ^ 3) +  8] =
		    X1[(((tailIndex - 3) & ~3) << 2) + (((tailIndex - 3) & 3) ^ 3) + 12] =
		    X2[(((tailIndex - 3) & ~3) << 2) + (((tailIndex - 3) & 3) ^ 3) +  0] =
		    X2[(((tailIndex - 3) & ~3) << 2) + (((tailIndex - 3) & 3) ^ 3) +  4] =
		    X2[(((tailIndex - 3) & ~3) << 2) + (((tailIndex - 3) & 3) ^ 3) +  8] =
		    X2[(((tailIndex - 3) & ~3) << 2) + (((tailIndex - 3) & 3) ^ 3) + 12] = p[(iters >> 12) & 0x3f];
	    }
	    if ( iters >> 18 ) {
		X1[(((tailIndex - 4) & ~3) << 2) + (((tailIndex - 4) & 3) ^ 3) +  0] =
		    X1[(((tailIndex - 4) & ~3) << 2) + (((tailIndex - 4) & 3) ^ 3) +  4] =
		    X1[(((tailIndex - 4) & ~3) << 2) + (((tailIndex - 4) & 3) ^ 3) +  8] =
		    X1[(((tailIndex - 4) & ~3) << 2) + (((tailIndex - 4) & 3) ^ 3) + 12] =
		    X2[(((tailIndex - 4) & ~3) << 2) + (((tailIndex - 4) & 3) ^ 3) +  0] =
		    X2[(((tailIndex - 4) & ~3) << 2) + (((tailIndex - 4) & 3) ^ 3) +  4] =
		    X2[(((tailIndex - 4) & ~3) << 2) + (((tailIndex - 4) & 3) ^ 3) +  8] =
		    X2[(((tailIndex - 4) & ~3) << 2) + (((tailIndex - 4) & 3) ^ 3) + 12] = p[(iters >> 18) & 0x3f];
	    }
	    if ( iters >> 24 ) {
		X1[(((tailIndex - 5) & ~3) << 2) + (((tailIndex - 5) & 3) ^ 3) +  0] =
		    X1[(((tailIndex - 5) & ~3) << 2) + (((tailIndex - 5) & 3) ^ 3) +  4] =
		    X1[(((tailIndex - 5) & ~3) << 2) + (((tailIndex - 5) & 3) ^ 3) +  8] =
		    X1[(((tailIndex - 5) & ~3) << 2) + (((tailIndex - 5) & 3) ^ 3) + 12] =
		    X2[(((tailIndex - 5) & ~3) << 2) + (((tailIndex - 5) & 3) ^ 3) +  0] =
		    X2[(((tailIndex - 5) & ~3) << 2) + (((tailIndex - 5) & 3) ^ 3) +  4] =
		    X2[(((tailIndex - 5) & ~3) << 2) + (((tailIndex - 5) & 3) ^ 3) +  8] =
		    X2[(((tailIndex - 5) & ~3) << 2) + (((tailIndex - 5) & 3) ^ 3) + 12] = p[(iters >> 24) & 0x3f];
	    }
	    if ( iters >> 30 ) {
		X1[(((tailIndex - 6) & ~3) << 2) + (((tailIndex - 6) & 3) ^ 3) +  0] =
		    X1[(((tailIndex - 6) & ~3) << 2) + (((tailIndex - 6) & 3) ^ 3) +  4] =
		    X1[(((tailIndex - 6) & ~3) << 2) + (((tailIndex - 6) & 3) ^ 3) +  8] =
		    X1[(((tailIndex - 6) & ~3) << 2) + (((tailIndex - 6) & 3) ^ 3) + 12] =
		    X2[(((tailIndex - 6) & ~3) << 2) + (((tailIndex - 6) & 3) ^ 3) +  0] =
		    X2[(((tailIndex - 6) & ~3) << 2) + (((tailIndex - 6) & 3) ^ 3) +  4] =
		    X2[(((tailIndex - 6) & ~3) << 2) + (((tailIndex - 6) & 3) ^ 3) +  8] =
		    X2[(((tailIndex - 6) & ~3) << 2) + (((tailIndex - 6) & 3) ^ 3) + 12] = p[(iters >> 30) & 0x3f];
	    }
	}
		
	/* Force compiler to flush and reload SSSE3 registers */
	asm volatile ( "nop" : : : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15", "memory" );
    
	/* Bypass shortcuts below on certain iterations */
	if((!(iters & 0xffffff)) && (tailIndex == 52 || tailIndex == 32)) {
	    A1 = A2 = H[0];
	    B1 = B2 = H[1];
	    C1 = C2 = H[2];
	    D1 = D2 = H[3];
	    E1 = E2 = H[4];

	    /* Populate W buffer */
	    for(t=16; t < 32; t++) {
		Wf(W1,t,Fa,Ga);
		Wf(W2,t,Fb,Gb);
	    }
			
	    ROUNDn( 0, A, B, C, D, E, F1, K[0] );
	    ROUNDn( 1, E, A, B, C, D, F1, K[0] );
	    ROUNDn( 2, D, E, A, B, C, F1, K[0] );
	    ROUNDn( 3, C, D, E, A, B, F1, K[0] );
	    ROUNDn( 4, B, C, D, E, A, F1, K[0] );
	    ROUNDn( 5, A, B, C, D, E, F1, K[0] );
	    ROUNDn( 6, E, A, B, C, D, F1, K[0] );
			
	    if(tailIndex == 52) {
		ROUNDn( 7, D, E, A, B, C, F1, K[0] );
		ROUNDn( 8, C, D, E, A, B, F1, K[0] );
		ROUNDn( 9, B, C, D, E, A, F1, K[0] );
		ROUNDn(10, A, B, C, D, E, F1, K[0] );
		ROUNDn(11, E, A, B, C, D, F1, K[0] );
	    }
			
	    pH[0] = A1;
	    pH[1] = B1;
	    pH[2] = C1;
	    pH[3] = D1;
	    pH[4] = E1;
	}
		
	/* Set up working variables */
	A1 = A2 = pH[0];
	B1 = B2 = pH[1];
	C1 = C2 = pH[2];
	D1 = D2 = pH[3];
	E1 = E2 = pH[4];
		
	/* Do the rounds */
	switch(tailIndex) {
	default:
	    ROUNDn( 0, A, B, C, D, E, F1, K[0] );
	    ROUNDn( 1, E, A, B, C, D, F1, K[0] );
	    ROUNDn( 2, D, E, A, B, C, F1, K[0] );
	    ROUNDn( 3, C, D, E, A, B, F1, K[0] );
	    ROUNDn( 4, B, C, D, E, A, F1, K[0] );
	    ROUNDn( 5, A, B, C, D, E, F1, K[0] );
	    ROUNDn( 6, E, A, B, C, D, F1, K[0] );
	case 32:
	    ROUNDn( 7, D, E, A, B, C, F1, K[0] );
	    ROUNDn( 8, C, D, E, A, B, F1, K[0] );
	    ROUNDn( 9, B, C, D, E, A, F1, K[0] );
	    ROUNDn(10, A, B, C, D, E, F1, K[0] );
	    ROUNDn(11, E, A, B, C, D, F1, K[0] );
	case 52:
	    ROUNDn(12, D, E, A, B, C, F1, K[0] );
	    ROUNDn(13, C, D, E, A, B, F1, K[0] );
	    ROUNDn(14, B, C, D, E, A, F1, K[0] );
	    ROUNDn(15, A, B, C, D, E, F1, K[0] );
	}

	if(tailIndex == 52) {
	    ROUNDn(16, E, A, B, C, D, F1, K[0] );
	    ROUNDn(17, D, E, A, B, C, F1, K[0] );
	    ROUNDn(18, C, D, E, A, B, F1, K[0] );
	    ROUNDn(19, B, C, D, E, A, F1, K[0] );
	    ROUNDu(20, A, B, C, D, E, F2, K[1] );
	    ROUNDn(21, E, A, B, C, D, F2, K[1] );
	    ROUNDn(22, D, E, A, B, C, F2, K[1] );
	    ROUNDu(23, C, D, E, A, B, F2, K[1] );
	    ROUNDn(24, B, C, D, E, A, F2, K[1] );
	    ROUNDn(25, A, B, C, D, E, F2, K[1] );
	    ROUNDu(26, E, A, B, C, D, F2, K[1] );
	    ROUNDn(27, D, E, A, B, C, F2, K[1] );
	    ROUNDu(28, C, D, E, A, B, F2, K[1] );
	    ROUNDu(29, B, C, D, E, A, F2, K[1] );
	    ROUNDn(30, A, B, C, D, E, F2, K[1] );
	} else if (tailIndex == 32) {
	    ROUNDn(16, E, A, B, C, D, F1, K[0] );
	    ROUNDn(17, D, E, A, B, C, F1, K[0] );
	    ROUNDn(18, C, D, E, A, B, F1, K[0] );
	    ROUNDn(19, B, C, D, E, A, F1, K[0] );
	    ROUNDn(20, A, B, C, D, E, F2, K[1] );
	    ROUNDu(21, E, A, B, C, D, F2, K[1] );
	    ROUNDn(22, D, E, A, B, C, F2, K[1] );
	    ROUNDu(23, C, D, E, A, B, F2, K[1] );
	    ROUNDu(24, B, C, D, E, A, F2, K[1] );
	    ROUNDn(25, A, B, C, D, E, F2, K[1] );
	    ROUNDu(26, E, A, B, C, D, F2, K[1] );
	    ROUNDu(27, D, E, A, B, C, F2, K[1] );
	    ROUNDn(28, C, D, E, A, B, F2, K[1] );
	    ROUNDu(29, B, C, D, E, A, F2, K[1] );
	    ROUNDu(30, A, B, C, D, E, F2, K[1] );
	} else {
	    ROUNDu(16, E, A, B, C, D, F1, K[0] );
	    ROUNDu(17, D, E, A, B, C, F1, K[0] );
	    ROUNDu(18, C, D, E, A, B, F1, K[0] );
	    ROUNDu(19, B, C, D, E, A, F1, K[0] );
	    ROUNDu(20, A, B, C, D, E, F2, K[1] );
	    ROUNDu(21, E, A, B, C, D, F2, K[1] );
	    ROUNDu(22, D, E, A, B, C, F2, K[1] );
	    ROUNDu(23, C, D, E, A, B, F2, K[1] );
	    ROUNDu(24, B, C, D, E, A, F2, K[1] );
	    ROUNDu(25, A, B, C, D, E, F2, K[1] );
	    ROUNDu(26, E, A, B, C, D, F2, K[1] );
	    ROUNDu(27, D, E, A, B, C, F2, K[1] );
	    ROUNDu(28, C, D, E, A, B, F2, K[1] );
	    ROUNDu(29, B, C, D, E, A, F2, K[1] );
	    ROUNDu(30, A, B, C, D, E, F2, K[1] );
	}
	  
	ROUNDu(31, E, A, B, C, D, F2, K[1] );
	ROUNDu(32, D, E, A, B, C, F2, K[1] );
	ROUNDu(33, C, D, E, A, B, F2, K[1] );
	ROUNDu(34, B, C, D, E, A, F2, K[1] );
	ROUNDu(35, A, B, C, D, E, F2, K[1] );
	ROUNDu(36, E, A, B, C, D, F2, K[1] );
	ROUNDu(37, D, E, A, B, C, F2, K[1] );
	ROUNDu(38, C, D, E, A, B, F2, K[1] );
	ROUNDu(39, B, C, D, E, A, F2, K[1] );
		
	ROUND5(40, F3, K[2] );
	ROUND5(45, F3, K[2] );
	ROUND5(50, F3, K[2] );
	ROUND5(55, F3, K[2] );
		
	ROUND5(60, F4, K[3] );
	ROUND5(65, F4, K[3] );
	ROUND5(70, F4, K[3] );
	ROUND5(75, F4, K[3] );
		
	/* Mix in the IV again */
	MA1 = ADD(A1, H[0]);
	MB1 = ADD(B1, H[1]);
	MA2 = ADD(A2, H[0]);
	MB2 = ADD(B2, H[1]);

	/* Quickly extract the best results from each pipe into a single vector set */
	/*
	 * vec_sel
	 * A1 1 0 0 1/ 1 1 1 0/ 0 1 0 1/ 1 0 1 0
	 * A2 1 1 1 0/ 1 1 1 0/ 1 0 0 1/ 0 1 1 0
	 *Aeq 0 0 0 0/ 1 1 1 1/ 0 0 0 0/ 0 0 0 0 eq element -> all 1
	 *Agt 0 0 0 0/ 0 0 0 0/ 0 0 0 0/ 1 1 1 1 gt element -> all 1
	 * B1 0 1 1 0/ 1 0 1 0/ 1 1 1 0/ 0 1 0 1
	 * B2 1 1 0 0/ 0 1 0 0/ 1 1 0 1/ 0 1 0 1
	 *Bgt 0 0 0 0/ 1 1 1 1/ 1 1 1 1/ 0 0 0 0
	 * M  0 0 0 0/ 1 1 1 1/ 0 0 0 0/ 1 1 1 1 if Aeq_i == 0 -> Agt else Bgt 
	 * 0  0 0 0 0/ 0 0 0 0/ 0 0 0 0/ 0 0 0 0
	 * 1  1 1 1 1/ 1 1 1 1/ 1 1 1 1/ 1 1 1 1
	 * N  0 0 0 0/ 1 1 1 1/ 0 0 0 0/ 1 1 1 1
	 * A  1 0 0 1/ 1 1 1 0/ 0 1 0 1/ 0 1 1 0 less one pick
	 * B  0 1 1 0/ 0 1 0 0/ 1 1 1 0/ 0 1 0 1
	 */
	Debug1 = ( (ssse3_d_t) __builtin_ia32_pandn128( (v2di) MA2, (v2di) MA1 ));
	Debug2 = ( (ssse3_d_t) (((ssse3_q_t) MA1) & ((ssse3_q_t) MA2)));
	Debug3 = ((ssse3_d_t) __builtin_ia32_pcmpgtd128((ssse3_q_t) (MA1+0x80000000), (ssse3_q_t) (MA2+0x80000000)));
	Debug4 = ((ssse3_d_t) __builtin_ia32_pcmpgtd128((ssse3_q_t) (MB1+0x80000000), (ssse3_q_t) (MB2+0x80000000)));
	Debug5 = ((ssse3_d_t) __builtin_ia32_pcmpeqd128((ssse3_q_t) MA1, (ssse3_q_t) MA2));
	M = VEC_SEL(VEC_CMPGT(MA1,MA2), VEC_CMPGT(MB1,MB2), VEC_CMPEQ(MA1,MA2));
	MA = VEC_SEL(MA1, MA2, M);
	MB = VEC_SEL(MB1, MB2, M);
	Debug6 = AND(M,MA2);
	Debug7 = ANDNOT(MA1,M);
	Debug8 = OR(Debug6,Debug7);
    
	/* Go over each vector element in turn */
	for(n=0; n < 4; n++) {
	    /* Extract A and B components */
	    IA = ((uInt32*) &MA)[n];
	    IB = ((uInt32*) &MB)[n];
	    m  = ((uInt32*) &M)[n];
				
	    /* Is this the best bit count so far? */
	    if(!(IA & bitMask1Low) && !(IB & bitMask1High)) {
		/* Count bits */
		gotBits = 0;
		if(IA) {
		    s = IA;
		    while(!(s & 0x80000000)) {
			s <<= 1;
			gotBits++;
		    }
		} else {
		    gotBits = 32;
		    if(IB) {
			s = IB;
			while(!(s & 0x80000000)) {
			    s <<= 1;
			    gotBits++;
			}
		    } else {
			gotBits = 64;
		    }
		}
		if ( gotBits > *best ) { *best = gotBits; }
		/* Regenerate the bit mask */
		maxBits = gotBits+1;
		if(maxBits < 32) {
		    bitMask1Low = ~((((uInt32) 1) << (32 - maxBits)) - 1);
		    bitMask1High = 0;
		} else {
		    bitMask1Low = ~0;
		    bitMask1High = ~((((uInt32) 1) << (64 - maxBits)) - 1);
		}
		((uInt32*) &vBitMaskLow )[0] = bitMask1Low ;
		((uInt32*) &vBitMaskLow )[1] = bitMask1Low ;
		((uInt32*) &vBitMaskHigh)[0] = bitMask1High;
		((uInt32*) &vBitMaskHigh)[1] = bitMask1High;
				
		/* Copy this result back to the block buffer, little-endian */
		if(m)
		    X = X2;
		else
		    X = X1;
				
		for(t=0; t < 16; t++) {
		    output[t*4+0] = X[t*16+3+n*4];
		    output[t*4+1] = X[t*16+2+n*4];
		    output[t*4+2] = X[t*16+1+n*4];
		    output[t*4+3] = X[t*16+0+n*4];
		}
				
		/* Is it good enough to bail out? */
		if(gotBits >= bits) {
		    /* Shut down use of SSSE3 */
		    __builtin_ia32_emms();
				  
		    return iters+8;
		}
	    }
	}
	MINTER_CALLBACK();
    }
	
    /* Shut down use of SSSE3 */
    __builtin_ia32_emms();

    return iters+8;

    /* For other platforms */
#else
    return 0;
#endif
#else  /* defined( COMPACT ) */
    return 0;
#endif
}
