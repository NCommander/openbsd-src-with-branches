/*	$OpenBSD$	*/


struct pdc_tod;

void	configure	__P((void));
void	dumpconf	__P((void));
void	pdc_iodc __P((int (*)__P((void)), int, ...));

