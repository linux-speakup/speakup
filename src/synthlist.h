/* this is included two times */
#if defined(PASS2)
/* table of built in synths */
#define SYNTH_DECL(who) &synth_##who,
#else
/* declare extern built in synths */
#define SYNTH_DECL(who) extern struct spk_synth synth_##who;
#define PASS2
#endif

#ifdef CONFIG_SPEAKUP_ACNTPC
SYNTH_DECL(acntpc)
#endif
#ifdef CONFIG_SPEAKUP_ACNTSA
SYNTH_DECL(acntsa)
#endif
#ifdef CONFIG_SPEAKUP_APOLLO
SYNTH_DECL(apollo)
#endif
#ifdef CONFIG_SPEAKUP_AUDPTR
SYNTH_DECL(audptr)
#endif
#ifdef CONFIG_SPEAKUP_BNS
SYNTH_DECL(bns)
#endif
#ifdef CONFIG_SPEAKUP_DECEXT
SYNTH_DECL(decext)
#endif
#ifdef CONFIG_SPEAKUP_DECTLK
SYNTH_DECL(dectlk)
#endif
#ifdef CONFIG_SPEAKUP_DTLK
SYNTH_DECL(dtlk)
#endif
#ifdef CONFIG_SPEAKUP_KEYPC
SYNTH_DECL(keypc)
#endif
#ifdef CONFIG_SPEAKUP_LTLK
SYNTH_DECL(ltlk)
#endif
#ifdef CONFIG_SPEAKUP_SFTSYN
SYNTH_DECL(sftsyn)
#endif
#ifdef CONFIG_SPEAKUP_SPKOUT
SYNTH_DECL(spkout)
#endif
#ifdef CONFIG_SPEAKUP_TXPRT
SYNTH_DECL(txprt)
#endif

#undef SYNTH_DECL
