/*	$OpenBSD: vmd.c,v 1.37 2016/10/29 14:56:05 edd Exp $	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>	/* nitems */
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/cdefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <ctype.h>
#include <util.h>

#include "proc.h"
#include "vmd.h"

__dead void usage(void);

int	 main(int, char **);
int	 vmd_configure(void);
void	 vmd_sighdlr(int sig, short event, void *arg);
void	 vmd_shutdown(void);
int	 vmd_control_run(void);
int	 vmd_dispatch_control(int, struct privsep_proc *, struct imsg *);
int	 vmd_dispatch_vmm(int, struct privsep_proc *, struct imsg *);

struct vmd	*env;

static struct privsep_proc procs[] = {
	/* Keep "priv" on top as procs[0] */
	{ "priv",	PROC_PRIV,	NULL, priv },
	{ "control",	PROC_CONTROL,	vmd_dispatch_control, control },
	{ "vmm",	PROC_VMM,	vmd_dispatch_vmm, vmm, vmm_shutdown },
};

/* For the privileged process */
static struct privsep_proc *proc_priv = &procs[0];
static struct passwd proc_privpw;

int
vmd_dispatch_control(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct privsep			*ps = p->p_ps;
	int				 res = 0, cmd = 0;
	unsigned int			 v = 0;
	struct vmop_create_params	 vmc;
	struct vmop_id			 vid;
	struct vm_terminate_params	 vtp;
	struct vmop_result		 vmr;
	struct vmd_vm			*vm = NULL;
	char				*str = NULL;
	uint32_t			 id = 0;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_START_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vmc);
		memcpy(&vmc, imsg->data, sizeof(vmc));
		res = vm_register(ps, &vmc, &vm);
		if (res == -1) {
			res = errno;
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
		} else {
			res = config_setvm(ps, vm, imsg->hdr.peerid);
			if (res == -1) {
				res = errno;
				cmd = IMSG_VMDOP_START_VM_RESPONSE;
				vm_remove(vm);
			}
		}
		break;
	case IMSG_VMDOP_TERMINATE_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		if ((id = vid.vid_id) == 0) {
			/* Lookup vm (id) by name */
			if ((vm = vm_getbyname(vid.vid_name)) == NULL) {
				res = ENOENT;
				cmd = IMSG_VMDOP_TERMINATE_VM_RESPONSE;
				break;
			}
			id = vm->vm_params.vmc_params.vcp_id;
		}
		memset(&vtp, 0, sizeof(vtp));
		vtp.vtp_vm_id = id;
		if (proc_compose_imsg(ps, PROC_VMM, -1, imsg->hdr.type,
		    imsg->hdr.peerid, -1, &vtp, sizeof(vtp)) == -1)
			return (-1);
		break;
	case IMSG_VMDOP_GET_INFO_VM_REQUEST:
		proc_forward_imsg(ps, imsg, PROC_VMM, -1);
		break;
	case IMSG_VMDOP_LOAD:
		IMSG_SIZE_CHECK(imsg, str); /* at least one byte for path */
		str = get_string((uint8_t *)imsg->data,
		    IMSG_DATA_SIZE(imsg));
	case IMSG_VMDOP_RELOAD:
		vmd_reload(0, str);
		free(str);
		break;
	case IMSG_CTL_RESET:
		IMSG_SIZE_CHECK(imsg, &v);
		memcpy(&v, imsg->data, sizeof(v));
		vmd_reload(v, str);
		break;
	default:
		return (-1);
	}

	switch (cmd) {
	case 0:
		break;
	case IMSG_VMDOP_START_VM_RESPONSE:
	case IMSG_VMDOP_TERMINATE_VM_RESPONSE:
		memset(&vmr, 0, sizeof(vmr));
		vmr.vmr_result = res;
		vmr.vmr_id = id;
		if (proc_compose_imsg(ps, PROC_CONTROL, -1, cmd,
		    imsg->hdr.peerid, -1, &vmr, sizeof(vmr)) == -1)
			return (-1);
		break;
	default:
		if (proc_compose_imsg(ps, PROC_CONTROL, -1, cmd,
		    imsg->hdr.peerid, -1, &res, sizeof(res)) == -1)
			return (-1);
		break;
	}

	return (0);
}

int
vmd_dispatch_vmm(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct vmop_result	 vmr;
	struct privsep		*ps = p->p_ps;
	int			 res = 0;
	struct vmd_vm		*vm;
	struct vm_create_params	*vcp;
	struct vmop_info_result	 vir;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_START_VM_RESPONSE:
		IMSG_SIZE_CHECK(imsg, &vmr);
		memcpy(&vmr, imsg->data, sizeof(vmr));
		if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL)
			fatalx("%s: invalid vm response", __func__);
		vm->vm_pid = vmr.vmr_pid;
		vcp = &vm->vm_params.vmc_params;
		vcp->vcp_id = vmr.vmr_id;

		/*
		 * If the peerid is not -1, forward the response back to the
		 * the control socket.  If it is -1, the request originated
		 * from the parent, not the control socket.
		 */
		if (vm->vm_peerid != (uint32_t)-1) {
			vmr.vmr_result = res;
			(void)strlcpy(vmr.vmr_ttyname, vm->vm_ttyname,
			    sizeof(vmr.vmr_ttyname));
			if (proc_compose_imsg(ps, PROC_CONTROL, -1,
			    imsg->hdr.type, vm->vm_peerid, -1,
			    &vmr, sizeof(vmr)) == -1) {
				errno = vmr.vmr_result;
				log_warn("%s: failed to foward vm result",
				    vcp->vcp_name);
				vm_remove(vm);
				return (-1);
			}
		}

		if (vmr.vmr_result) {
			errno = vmr.vmr_result;
			log_warn("%s: failed to start vm", vcp->vcp_name);
			vm_remove(vm);
			break;
		}

		/* Now configure all the interfaces */
		if (vm_priv_ifconfig(ps, vm) == -1) {
			log_warn("%s: failed to configure vm", vcp->vcp_name);
			vm_remove(vm);
			break;
		}

		log_info("%s: started vm %d successfully, tty %s",
		    vcp->vcp_name, vcp->vcp_id, vm->vm_ttyname);
		break;
	case IMSG_VMDOP_TERMINATE_VM_RESPONSE:
	case IMSG_VMDOP_TERMINATE_VM_EVENT:
		IMSG_SIZE_CHECK(imsg, &vmr);
		memcpy(&vmr, imsg->data, sizeof(vmr));
		if (imsg->hdr.type == IMSG_VMDOP_TERMINATE_VM_RESPONSE)
			proc_forward_imsg(ps, imsg, PROC_CONTROL, -1);
		if (vmr.vmr_result == 0) {
			/* Remove local reference */
			vm = vm_getbyid(vmr.vmr_id);
			vm_remove(vm);
		}
		break;
	case IMSG_VMDOP_GET_INFO_VM_DATA:
		IMSG_SIZE_CHECK(imsg, &vir);
		memcpy(&vir, imsg->data, sizeof(vir));
		if ((vm = vm_getbyid(vir.vir_info.vir_id)) != NULL) {
			(void)strlcpy(vir.vir_ttyname, vm->vm_ttyname,
			    sizeof(vir.vir_ttyname));
		}
		if (proc_compose_imsg(ps, PROC_CONTROL, -1, imsg->hdr.type,
		    imsg->hdr.peerid, -1, &vir, sizeof(vir)) == -1) {
			vm_remove(vm);
			return (-1);
		}
		break;
	case IMSG_VMDOP_GET_INFO_VM_END_DATA:
		IMSG_SIZE_CHECK(imsg, &res);
		proc_forward_imsg(ps, imsg, PROC_CONTROL, -1);
		break;
	default:
		return (-1);
	}

	return (0);
}

void
vmd_sighdlr(int sig, short event, void *arg)
{
	if (privsep_process != PROC_PARENT)
		return;

	switch (sig) {
	case SIGHUP:
		log_info("%s: reload requested with SIGHUP", __func__);

		/*
		 * This is safe because libevent uses async signal handlers
		 * that run in the event loop and not in signal context.
		 */
		vmd_reload(0, NULL);
		break;
	case SIGPIPE:
		log_info("%s: ignoring SIGPIPE", __func__);
		break;
	case SIGUSR1:
		log_info("%s: ignoring SIGUSR1", __func__);
		break;
	case SIGTERM:
	case SIGINT:
		vmd_shutdown();
		break;
	default:
		fatalx("unexpected signal");
	}
}

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] [-f file]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct privsep		*ps;
	int			 ch;
	const char		*conffile = VMD_CONF;
	enum privsep_procid	 proc_id = PROC_PARENT;
	int			 proc_instance = 0;
	const char		*errp, *title = NULL;
	int			 argc0 = argc;

	/* log to stderr until daemonized */
	log_init(1, LOG_DAEMON);

	if ((env = calloc(1, sizeof(*env))) == NULL)
		fatal("calloc: env");

	while ((ch = getopt(argc, argv, "D:P:I:df:vn")) != -1) {
		switch (ch) {
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'd':
			env->vmd_debug = 2;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'v':
			env->vmd_verbose++;
			break;
		case 'n':
			env->vmd_noaction = 1;
			break;
		case 'P':
			title = optarg;
			proc_id = proc_getid(procs, nitems(procs), title);
			if (proc_id == PROC_MAX)
				fatalx("invalid process name");
			break;
		case 'I':
			proc_instance = strtonum(optarg, 0,
			    PROC_MAX_INSTANCES, &errp);
			if (errp)
				fatalx("invalid process instance");
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	if (argc > 0)
		usage();

	if (env->vmd_noaction && !env->vmd_debug)
		env->vmd_debug = 1;

	/* check for root privileges */
	if (env->vmd_noaction == 0) {
		if (geteuid())
			fatalx("need root privileges");
	}

	ps = &env->vmd_ps;
	ps->ps_env = env;
	env->vmd_fd = -1;

	if (config_init(env) == -1)
		fatal("failed to initialize configuration");

	if ((ps->ps_pw = getpwnam(VMD_USER)) == NULL)
		fatal("unknown user %s", VMD_USER);

	/* First proc runs as root without pledge but in default chroot */
	proc_priv->p_pw = &proc_privpw; /* initialized to all 0 */
	proc_priv->p_chroot = ps->ps_pw->pw_dir; /* from VMD_USER */

	/* Open /dev/vmm */
	if (env->vmd_noaction == 0) {
		env->vmd_fd = open(VMM_NODE, O_RDWR);
		if (env->vmd_fd == -1)
			fatal("%s", VMM_NODE);
	}

	/* Configure the control socket */
	ps->ps_csock.cs_name = SOCKET_NAME;
	TAILQ_INIT(&ps->ps_rcsocks);

	/* Configuration will be parsed after forking the children */
	env->vmd_conffile = conffile;

	log_init(env->vmd_debug, LOG_DAEMON);
	log_verbose(env->vmd_verbose);

	if (env->vmd_noaction)
		ps->ps_noaction = 1;
	ps->ps_instance = proc_instance;
	if (title != NULL)
		ps->ps_title[proc_id] = title;

	/* only the parent returns */
	proc_init(ps, procs, nitems(procs), argc0, argv, proc_id);

	log_procinit("parent");
	if (!env->vmd_debug && daemon(0, 0) == -1)
		fatal("can't daemonize");

	if (ps->ps_noaction == 0)
		log_info("startup");

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigterm, SIGTERM, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsighup, SIGHUP, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigusr1, SIGUSR1, vmd_sighdlr, ps);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);
	signal_add(&ps->ps_evsigusr1, NULL);

	if (!env->vmd_noaction)
		proc_connect(ps);

	if (vmd_configure() == -1)
		fatalx("configuration failed");

	event_dispatch();

	log_debug("parent exiting");

	return (0);
}

int
vmd_configure(void)
{
	struct vmd_vm		*vm;
	struct vmd_switch	*vsw;
	int		 	 res, ret = 0;

	/*
	 * pledge in the parent process:
	 * stdio - for malloc and basic I/O including events.
	 * rpath - for reload to open and read the configuration files.
	 * wpath - for opening disk images and tap devices.
	 * tty - for openpty.
	 * proc - run kill to terminate its children safely.
	 * sendfd - for disks, interfaces and other fds.
	 */
	if (pledge("stdio rpath wpath proc tty sendfd", NULL) == -1)
		fatal("pledge");

	if (parse_config(env->vmd_conffile) == -1) {
		proc_kill(&env->vmd_ps);
		exit(1);
	}

	if (env->vmd_noaction) {
		fprintf(stderr, "configuration OK\n");
		proc_kill(&env->vmd_ps);
		exit(0);
	}

	TAILQ_FOREACH(vsw, env->vmd_switches, sw_entry) {
		if (vsw->sw_running)
			continue;
		if (vm_priv_brconfig(&env->vmd_ps, vsw) == -1) {
			log_warn("%s: failed to create switch %s",
			    __func__, vsw->sw_name);
			switch_remove(vsw);
		}
	}

	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		res = config_setvm(&env->vmd_ps, vm, -1);
		if (res == -1) {
			log_warn("%s: failed to create vm %s",
			    __func__,
			    vm->vm_params.vmc_params.vcp_name);
			ret = -1;
			vm_remove(vm);
			goto fail;
		}
	}
 fail:
	return (ret);
}

void
vmd_reload(unsigned int reset, const char *filename)
{
	struct vmd_vm		*vm;
	struct vmd_switch	*vsw;
	int		 	 res;

	/* Switch back to the default config file */
	if (filename == NULL || *filename == '\0')
		filename = env->vmd_conffile;

	log_debug("%s: level %d config file %s", __func__, reset, filename);

	if (reset) {
		/* Purge the configuration */
		config_purge(env, reset);
		config_setreset(env, reset);
	} else {
		/* Reload the configuration */
		if (parse_config(filename) == -1) {
			log_debug("%s: failed to load config file %s",
			    __func__, filename);
		}

		TAILQ_FOREACH(vsw, env->vmd_switches, sw_entry) {
			if (vsw->sw_running)
				continue;
			if (vm_priv_brconfig(&env->vmd_ps, vsw) == -1) {
				log_warn("%s: failed to create switch %s",
				    __func__, vsw->sw_name);
				switch_remove(vsw);
			}
		}

		TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
			if (vm->vm_running == 0) {
				res = config_setvm(&env->vmd_ps, vm, -1);
				if (res == -1) {
					log_warn("%s: failed to create vm %s",
					    __func__,
					    vm->vm_params.vmc_params.vcp_name);
					vm_remove(vm);
				}
			} else {
				log_debug("%s: not creating vm \"%s\": "
				    "(running)", __func__,
				    vm->vm_params.vmc_params.vcp_name);
			}
		}
	}
}

void
vmd_shutdown(void)
{
	proc_kill(&env->vmd_ps);
	free(env);

	log_warnx("parent terminating");
	exit(0);
}

struct vmd_vm *
vm_getbyvmid(uint32_t vmid)
{
	struct vmd_vm	*vm;

	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (vm->vm_vmid == vmid)
			return (vm);
	}

	return (NULL);
}

struct vmd_vm *
vm_getbyid(uint32_t id)
{
	struct vmd_vm	*vm;

	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (vm->vm_params.vmc_params.vcp_id == id)
			return (vm);
	}

	return (NULL);
}

struct vmd_vm *
vm_getbyname(const char *name)
{
	struct vmd_vm	*vm;

	if (name == NULL)
		return (NULL);
	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (strcmp(vm->vm_params.vmc_params.vcp_name, name) == 0)
			return (vm);
	}

	return (NULL);
}

struct vmd_vm *
vm_getbypid(pid_t pid)
{
	struct vmd_vm	*vm;

	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (vm->vm_pid == pid)
			return (vm);
	}

	return (NULL);
}

void
vm_remove(struct vmd_vm *vm)
{
	unsigned int	 i;

	if (vm == NULL)
		return;

	TAILQ_REMOVE(env->vmd_vms, vm, vm_entry);

	for (i = 0; i < VMM_MAX_DISKS_PER_VM; i++) {
		if (vm->vm_disks[i] != -1)
			close(vm->vm_disks[i]);
	}
	for (i = 0; i < VMM_MAX_NICS_PER_VM; i++) {
		if (vm->vm_ifs[i].vif_fd != -1)
			close(vm->vm_ifs[i].vif_fd);
		free(vm->vm_ifs[i].vif_name);
		free(vm->vm_ifs[i].vif_switch);
		free(vm->vm_ifs[i].vif_group);
	}
	if (vm->vm_kernel != -1)
		close(vm->vm_kernel);
	if (vm->vm_tty != -1)
		close(vm->vm_tty);

	free(vm->vm_ttyname);
	free(vm);
}

int
vm_register(struct privsep *ps, struct vmop_create_params *vmc,
    struct vmd_vm **ret_vm)
{
	struct vmd_vm		*vm = NULL;
	struct vm_create_params	*vcp = &vmc->vmc_params;
	unsigned int		 i;

	errno = 0;
	*ret_vm = NULL;

	if ((vm = vm_getbyname(vcp->vcp_name)) != NULL) {
		*ret_vm = vm;
		errno = EALREADY;
		goto fail;
	}

	if (vcp->vcp_ncpus == 0)
		vcp->vcp_ncpus = 1;
	if (vcp->vcp_ncpus > VMM_MAX_VCPUS_PER_VM) {
		log_debug("invalid number of CPUs");
		goto fail;
	} else if (vcp->vcp_ndisks > VMM_MAX_DISKS_PER_VM) {
		log_debug("invalid number of disks");
		goto fail;
	} else if (vcp->vcp_nnics > VMM_MAX_NICS_PER_VM) {
		log_debug("invalid number of interfaces");
		goto fail;
	}

	if ((vm = calloc(1, sizeof(*vm))) == NULL)
		goto fail;

	memcpy(&vm->vm_params, vmc, sizeof(vm->vm_params));
	vm->vm_pid = -1;

	for (i = 0; i < vcp->vcp_ndisks; i++)
		vm->vm_disks[i] = -1;
	for (i = 0; i < vcp->vcp_nnics; i++)
		vm->vm_ifs[i].vif_fd = -1;
	vm->vm_kernel = -1;

	if ((vm->vm_vmid = ++env->vmd_nvm) == 0)
		fatalx("too many vms");

	TAILQ_INSERT_TAIL(env->vmd_vms, vm, vm_entry);

	*ret_vm = vm;
	return (0);
fail:
	if (errno == 0)
		errno = EINVAL;
	return (-1);
}

void
switch_remove(struct vmd_switch *vsw)
{
	struct vmd_if	*vif;

	if (vsw == NULL)
		return;

	TAILQ_REMOVE(env->vmd_switches, vsw, sw_entry);

	while ((vif = TAILQ_FIRST(&vsw->sw_ifs)) != NULL) {
		free(vif->vif_name);
		free(vif->vif_switch);
		TAILQ_REMOVE(&vsw->sw_ifs, vif, vif_entry);
		free(vif);
	}

	free(vsw->sw_group);
	free(vsw->sw_name);
	free(vsw);
}

struct vmd_switch *
switch_getbyname(const char *name)
{
	struct vmd_switch	*vsw;

	if (name == NULL)
		return (NULL);
	TAILQ_FOREACH(vsw, env->vmd_switches, sw_entry) {
		if (strcmp(vsw->sw_name, name) == 0)
			return (vsw);
	}

	return (NULL);
}

char *
get_string(uint8_t *ptr, size_t len)
{
	size_t	 i;

	for (i = 0; i < len; i++)
		if (!isprint(ptr[i]))
			break;

	return strndup(ptr, i);
}
