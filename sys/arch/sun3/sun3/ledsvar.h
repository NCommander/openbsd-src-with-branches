/*	$OpenBSD$	*/

extern volatile unsigned int led_n_patterns;
extern volatile unsigned int led_countmax;
extern volatile const unsigned char * volatile led_patterns;
extern volatile unsigned int led_countdown;
extern volatile unsigned int led_px;

extern int ledrw(struct uio *);
