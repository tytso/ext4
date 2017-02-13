#undef TRACE_SYSTEM
#define TRACE_SYSTEM jbd2

#if !defined(_TRACE_JBD2_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_JBD2_H

#include <linux/jbd2.h>
#include <linux/tracepoint.h>

struct transaction_chp_stats_s;
struct transaction_run_stats_s;

TRACE_EVENT(jbd2_checkpoint,

	TP_PROTO(journal_t *journal, int result),

	TP_ARGS(journal, result),

	TP_STRUCT__entry(
		__field(	dev_t,	dev			)
		__field(	int,	result			)
	),

	TP_fast_assign(
		__entry->dev		= journal->j_fs_dev->bd_dev;
		__entry->result		= result;
	),

	TP_printk("dev %d,%d result %d",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->result)
);

DECLARE_EVENT_CLASS(jbd2_commit,

	TP_PROTO(journal_t *journal, transaction_t *commit_transaction),

	TP_ARGS(journal, commit_transaction),

	TP_STRUCT__entry(
		__field(	dev_t,	dev			)
		__field(	char,	sync_commit		  )
		__field(	int,	transaction		  )
	),

	TP_fast_assign(
		__entry->dev		= journal->j_fs_dev->bd_dev;
		__entry->sync_commit = commit_transaction->t_synchronous_commit;
		__entry->transaction	= commit_transaction->t_tid;
	),

	TP_printk("dev %d,%d transaction %d sync %d",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->transaction, __entry->sync_commit)
);

DEFINE_EVENT(jbd2_commit, jbd2_start_commit,

	TP_PROTO(journal_t *journal, transaction_t *commit_transaction),

	TP_ARGS(journal, commit_transaction)
);

DEFINE_EVENT(jbd2_commit, jbd2_commit_locking,

	TP_PROTO(journal_t *journal, transaction_t *commit_transaction),

	TP_ARGS(journal, commit_transaction)
);

DEFINE_EVENT(jbd2_commit, jbd2_commit_flushing,

	TP_PROTO(journal_t *journal, transaction_t *commit_transaction),

	TP_ARGS(journal, commit_transaction)
);

DEFINE_EVENT(jbd2_commit, jbd2_commit_logging,

	TP_PROTO(journal_t *journal, transaction_t *commit_transaction),

	TP_ARGS(journal, commit_transaction)
);

DEFINE_EVENT(jbd2_commit, jbd2_drop_transaction,

	TP_PROTO(journal_t *journal, transaction_t *commit_transaction),

	TP_ARGS(journal, commit_transaction)
);

TRACE_EVENT(jbd2_end_commit,
	TP_PROTO(journal_t *journal, transaction_t *commit_transaction),

	TP_ARGS(journal, commit_transaction),

	TP_STRUCT__entry(
		__field(	dev_t,	dev			)
		__field(	char,	sync_commit		  )
		__field(	int,	transaction		  )
		__field(	int,	head		  	  )
	),

	TP_fast_assign(
		__entry->dev		= journal->j_fs_dev->bd_dev;
		__entry->sync_commit = commit_transaction->t_synchronous_commit;
		__entry->transaction	= commit_transaction->t_tid;
		__entry->head		= journal->j_tail_sequence;
	),

	TP_printk("dev %d,%d transaction %d sync %d head %d",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->transaction, __entry->sync_commit, __entry->head)
);

TRACE_EVENT(jbd2_submit_inode_data,
	TP_PROTO(struct inode *inode),

	TP_ARGS(inode),

	TP_STRUCT__entry(
		__field(	dev_t,	dev			)
		__field(	ino_t,	ino			)
	),

	TP_fast_assign(
		__entry->dev	= inode->i_sb->s_dev;
		__entry->ino	= inode->i_ino;
	),

	TP_printk("dev %d,%d ino %lu",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  (unsigned long) __entry->ino)
);

TRACE_EVENT(jbd2_handle_start,
	TP_PROTO(dev_t dev, unsigned long tid, unsigned int type,
		 unsigned int line_no, int requested_blocks),

	TP_ARGS(dev, tid, type, line_no, requested_blocks),

	TP_STRUCT__entry(
		__field(		dev_t,	dev		)
		__field(	unsigned long,	tid		)
		__field(	 unsigned int,	type		)
		__field(	 unsigned int,	line_no		)
		__field(		  int,	requested_blocks)
	),

	TP_fast_assign(
		__entry->dev		  = dev;
		__entry->tid		  = tid;
		__entry->type		  = type;
		__entry->line_no	  = line_no;
		__entry->requested_blocks = requested_blocks;
	),

	TP_printk("dev %d,%d tid %lu type %u line_no %u "
		  "requested_blocks %d",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->tid,
		  __entry->type, __entry->line_no, __entry->requested_blocks)
);

TRACE_EVENT(jbd2_handle_extend,
	TP_PROTO(dev_t dev, unsigned long tid, unsigned int type,
		 unsigned int line_no, int buffer_credits,
		 int requested_blocks),

	TP_ARGS(dev, tid, type, line_no, buffer_credits, requested_blocks),

	TP_STRUCT__entry(
		__field(		dev_t,	dev		)
		__field(	unsigned long,	tid		)
		__field(	 unsigned int,	type		)
		__field(	 unsigned int,	line_no		)
		__field(		  int,	buffer_credits  )
		__field(		  int,	requested_blocks)
	),

	TP_fast_assign(
		__entry->dev		  = dev;
		__entry->tid		  = tid;
		__entry->type		  = type;
		__entry->line_no	  = line_no;
		__entry->buffer_credits   = buffer_credits;
		__entry->requested_blocks = requested_blocks;
	),

	TP_printk("dev %d,%d tid %lu type %u line_no %u "
		  "buffer_credits %d requested_blocks %d",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->tid,
		  __entry->type, __entry->line_no, __entry->buffer_credits,
		  __entry->requested_blocks)
);

TRACE_EVENT(jbd2_handle_stats,
	TP_PROTO(dev_t dev, unsigned long tid, unsigned int type,
		 unsigned int line_no, int interval, int sync,
		 int requested_blocks, int dirtied_blocks),

	TP_ARGS(dev, tid, type, line_no, interval, sync,
		requested_blocks, dirtied_blocks),

	TP_STRUCT__entry(
		__field(		dev_t,	dev		)
		__field(	unsigned long,	tid		)
		__field(	 unsigned int,	type		)
		__field(	 unsigned int,	line_no		)
		__field(		  int,	interval	)
		__field(		  int,	sync		)
		__field(		  int,	requested_blocks)
		__field(		  int,	dirtied_blocks	)
	),

	TP_fast_assign(
		__entry->dev		  = dev;
		__entry->tid		  = tid;
		__entry->type		  = type;
		__entry->line_no	  = line_no;
		__entry->interval	  = interval;
		__entry->sync		  = sync;
		__entry->requested_blocks = requested_blocks;
		__entry->dirtied_blocks	  = dirtied_blocks;
	),

	TP_printk("dev %d,%d tid %lu type %u line_no %u interval %d "
		  "sync %d requested_blocks %d dirtied_blocks %d",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->tid,
		  __entry->type, __entry->line_no, __entry->interval,
		  __entry->sync, __entry->requested_blocks,
		  __entry->dirtied_blocks)
);

TRACE_EVENT(jbd2_run_stats,
	TP_PROTO(dev_t dev, unsigned long tid,
		 struct transaction_run_stats_s *stats),

	TP_ARGS(dev, tid, stats),

	TP_STRUCT__entry(
		__field(		dev_t,	dev		)
		__field(	unsigned long,	tid		)
		__field(	unsigned long,	wait		)
		__field(	unsigned long,	request_delay	)
		__field(	unsigned long,	running		)
		__field(	unsigned long,	locked		)
		__field(	unsigned long,	flushing	)
		__field(	unsigned long,	logging		)
		__field(		__u32,	handle_count	)
		__field(		__u32,	blocks		)
		__field(		__u32,	blocks_logged	)
	),

	TP_fast_assign(
		__entry->dev		= dev;
		__entry->tid		= tid;
		__entry->wait		= stats->rs_wait;
		__entry->request_delay	= stats->rs_request_delay;
		__entry->running	= stats->rs_running;
		__entry->locked		= stats->rs_locked;
		__entry->flushing	= stats->rs_flushing;
		__entry->logging	= stats->rs_logging;
		__entry->handle_count	= stats->rs_handle_count;
		__entry->blocks		= stats->rs_blocks;
		__entry->blocks_logged	= stats->rs_blocks_logged;
	),

	TP_printk("dev %d,%d tid %lu wait %u request_delay %u running %u "
		  "locked %u flushing %u logging %u handle_count %u "
		  "blocks %u blocks_logged %u",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->tid,
		  jiffies_to_msecs(__entry->wait),
		  jiffies_to_msecs(__entry->request_delay),
		  jiffies_to_msecs(__entry->running),
		  jiffies_to_msecs(__entry->locked),
		  jiffies_to_msecs(__entry->flushing),
		  jiffies_to_msecs(__entry->logging),
		  __entry->handle_count, __entry->blocks,
		  __entry->blocks_logged)
);

TRACE_EVENT(jbd2_checkpoint_stats,
	TP_PROTO(dev_t dev, unsigned long tid,
		 struct transaction_chp_stats_s *stats),

	TP_ARGS(dev, tid, stats),

	TP_STRUCT__entry(
		__field(		dev_t,	dev		)
		__field(	unsigned long,	tid		)
		__field(	unsigned long,	chp_time	)
		__field(		__u32,	forced_to_close	)
		__field(		__u32,	written		)
		__field(		__u32,	dropped		)
	),

	TP_fast_assign(
		__entry->dev		= dev;
		__entry->tid		= tid;
		__entry->chp_time	= stats->cs_chp_time;
		__entry->forced_to_close= stats->cs_forced_to_close;
		__entry->written	= stats->cs_written;
		__entry->dropped	= stats->cs_dropped;
	),

	TP_printk("dev %d,%d tid %lu chp_time %u forced_to_close %u "
		  "written %u dropped %u",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->tid,
		  jiffies_to_msecs(__entry->chp_time),
		  __entry->forced_to_close, __entry->written, __entry->dropped)
);

TRACE_EVENT(jbd2_update_log_tail,

	TP_PROTO(journal_t *journal, tid_t first_tid,
		 unsigned long block_nr, unsigned long freed),

	TP_ARGS(journal, first_tid, block_nr, freed),

	TP_STRUCT__entry(
		__field(	dev_t,	dev			)
		__field(	tid_t,	tail_sequence		)
		__field(	tid_t,	first_tid		)
		__field(unsigned long,	block_nr		)
		__field(unsigned long,	freed			)
	),

	TP_fast_assign(
		__entry->dev		= journal->j_fs_dev->bd_dev;
		__entry->tail_sequence	= journal->j_tail_sequence;
		__entry->first_tid	= first_tid;
		__entry->block_nr	= block_nr;
		__entry->freed		= freed;
	),

	TP_printk("dev %d,%d from %u to %u offset %lu freed %lu",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->tail_sequence, __entry->first_tid,
		  __entry->block_nr, __entry->freed)
);

TRACE_EVENT(jbd2_write_superblock,

	TP_PROTO(journal_t *journal, int write_op),

	TP_ARGS(journal, write_op),

	TP_STRUCT__entry(
		__field(	dev_t,  dev			)
		__field(	  int,  write_op		)
	),

	TP_fast_assign(
		__entry->dev		= journal->j_fs_dev->bd_dev;
		__entry->write_op	= write_op;
	),

	TP_printk("dev %d,%d write_op %x", MAJOR(__entry->dev),
		  MINOR(__entry->dev), __entry->write_op)
);

TRACE_EVENT(jbd2_lock_buffer_stall,

	TP_PROTO(dev_t dev, unsigned long stall_ms),

	TP_ARGS(dev, stall_ms),

	TP_STRUCT__entry(
		__field(        dev_t, dev	)
		__field(unsigned long, stall_ms	)
	),

	TP_fast_assign(
		__entry->dev		= dev;
		__entry->stall_ms	= stall_ms;
	),

	TP_printk("dev %d,%d stall_ms %lu",
		MAJOR(__entry->dev), MINOR(__entry->dev),
		__entry->stall_ms)
);

TRACE_EVENT(jbd2_jmap_replace,

	TP_PROTO(struct jmap_entry *jentry, struct blk_mapping *mapping, \
		int t_idx),

	TP_ARGS(jentry, mapping, t_idx),

	TP_STRUCT__entry(
		__field(sector_t, fsblk		)
		__field(sector_t, old_logblk	)
		__field(sector_t, new_logblk	)
		__field(int, old_t_idx		)
		__field(int, new_t_idx		)
	),

	TP_fast_assign(
		__entry->fsblk		= mapping->fsblk;
		__entry->old_logblk	= jentry->mapping.logblk;
		__entry->new_logblk	= mapping->logblk;
		__entry->old_t_idx       = jentry->t_idx;
		__entry->new_t_idx       = t_idx;
	),

	TP_printk("remap %llu from %llu to %llu, move from transaction at index %d to transaction at index %d",
		  (unsigned long long) __entry->fsblk,
		  (unsigned long long) __entry->old_logblk,
		  (unsigned long long) __entry->new_logblk,
		  __entry->old_t_idx,
		  __entry->new_t_idx)
);

TRACE_EVENT(jbd2_jmap_insert,

	TP_PROTO(struct blk_mapping *mapping, int t_idx),

	TP_ARGS(mapping, t_idx),

	TP_STRUCT__entry(
		__field(sector_t, fsblk	)
		__field(sector_t, logblk)
		__field(int, t_idx)
	),

	TP_fast_assign(
		__entry->fsblk	= mapping->fsblk;
		__entry->logblk	= mapping->logblk;
		__entry->t_idx = t_idx;
	),

	TP_printk("map %llu to %llu, insert to transaction %d",
		  (unsigned long long) __entry->fsblk,
		  (unsigned long long) __entry->logblk,
		  __entry->t_idx)
);

TRACE_EVENT(jbd2_jmap_lookup,

	TP_PROTO(sector_t fsblk, sector_t logblk, const char *func),

	TP_ARGS(fsblk, logblk, func),

	TP_STRUCT__entry(
		__field(sector_t, fsblk	)
		__field(sector_t, logblk)
		__string(func, func)
	),

	TP_fast_assign(
		__entry->fsblk	= fsblk;
		__entry->logblk	= logblk;
		__assign_str(func, func);
	),

	TP_printk("%s: lookup %llu -> %llu",
		  __get_str(func),
		  (unsigned long long) __entry->fsblk,
		  (unsigned long long) __entry->logblk)
);

TRACE_EVENT(jbd2_jmap_printf,

	TP_PROTO(const char *s),

	TP_ARGS(s),

	TP_STRUCT__entry(
		__string(s, s)
	),

	TP_fast_assign(
		__assign_str(s, s);
	),

	TP_printk("%s",
		__get_str(s))
);

TRACE_EVENT(jbd2_jmap_printf1,

	TP_PROTO(const char *s, sector_t fsblk),

	TP_ARGS(s, fsblk),

	TP_STRUCT__entry(
		__string(s, s)
		__field(sector_t, fsblk	)
	),

	TP_fast_assign(
		__assign_str(s, s);
		__entry->fsblk	= fsblk;
	),

	TP_printk("%s: %llu",
		  __get_str(s),
		  (unsigned long long) __entry->fsblk)
);

TRACE_EVENT(jbd2_jmap_printf2,

	TP_PROTO(const char *s, sector_t fsblk, sector_t logblk),

	TP_ARGS(s, fsblk, logblk),

	TP_STRUCT__entry(
		__string(s, s)
		__field(sector_t, fsblk	)
		__field(sector_t, logblk)
	),

	TP_fast_assign(
		__assign_str(s, s);
		__entry->fsblk	= fsblk;
		__entry->logblk	= logblk;
	),

	TP_printk("%s: %llu:%llu",
		  __get_str(s),
		  (unsigned long long) __entry->fsblk,
		  (unsigned long long) __entry->logblk)
);

TRACE_EVENT(jbd2_transaction_infos_add,

	TP_PROTO(int t_idx, struct transaction_info *ti, int nr_mappings),

	TP_ARGS(t_idx, ti, nr_mappings),

	TP_STRUCT__entry(
		__field(int, t_idx	)
		__field(tid_t, tid	)
		__field(sector_t, offset)
		__field(int, nr_mappings)
	),

	TP_fast_assign(
		__entry->t_idx	= t_idx;
		__entry->tid	= ti->tid;
		__entry->offset = ti->offset;
		__entry->nr_mappings = nr_mappings;
	),

	TP_printk("inserted transaction %u (offset %llu) at index %d with %d mappings",
		  __entry->tid,
		  (unsigned long long) __entry->offset,
		  __entry->t_idx,
		  __entry->nr_mappings)
);

#endif /* _TRACE_JBD2_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
