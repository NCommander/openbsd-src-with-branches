/* $OpenBSD$ */
void ite_restart_blanker(struct ite_softc *);
void ite_reset_blanker(struct ite_softc *);
void ite_disable_blanker(struct ite_softc *);
void ite_enable_blanker(struct ite_softc *);

extern int blank_time;
