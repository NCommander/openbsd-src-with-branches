/* $OpenBSD$ */
void ite_restart_blanker __P((struct ite_softc *));
void ite_reset_blanker __P((struct ite_softc *));
void ite_disable_blanker __P((struct ite_softc *));
void ite_enable_blanker __P((struct ite_softc *));

extern int blank_time;
