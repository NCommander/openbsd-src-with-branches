/*	$OpenBSD$	*/

pid_t gadget_getpid() {
	pid_t ans = 0;
#if defined(__amd64__)
	asm("mov $0x14, %%eax; syscall; mov %%eax, %0;" :"=r"(ans)::"%eax", "%ecx", "%r11");
#endif
	return ans;
}
