/* $OpenBSD: ite_blank.h,v 1.1 1999/11/05 17:15:34 espie Exp $ */
void ite_restart_blanker(struct ite_softc *);
void ite_reset_blanker(struct ite_softc *);
void ite_disable_blanker(struct ite_softc *);
void ite_enable_blanker(struct ite_softc *);

extern int blank_time;
