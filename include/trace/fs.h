#ifndef _TRACE_FS_H
#define _TRACE_FS_H

#include <linux/fs.h>
#include <linux/tracepoint.h>

DEFINE_TRACE(do_sys_open,
	TPPROTO(struct file *filp, int flags, int mode, long fd),
		TPARGS(filp, flags, mode, fd));

#endif
