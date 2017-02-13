#ifndef _LINUX_JMAP_H
#define _LINUX_JMAP_H

#include <linux/buffer_head.h>
#include <linux/journal-head.h>
#include <linux/list.h>
#include <linux/circ_buf.h>
#include <linux/completion.h>

/*
 * Forward declaration for journal_t so that we don't get circular dependency
 * between jbd2.h and jmap.h
 */
struct journal_s;
typedef struct journal_s journal_t;

/*
 * Maximum number of transactions.  This guides the size of the circular buffer
 * in which we store housekeeping information per transaction.  We start
 * cleaning either when the circular buffer is full or when we hit the free
 * space threshold, whichever happens first.  For starters, we make this
 * constant large to make sure that we start cleaning only when we hit the free
 * space threshold.  Later we can empirically determine a sensible value.
 */
#define MAX_LIVE_TRANSACTIONS 65536

/*
 * A mapping from file system block to log block.
 */
struct blk_mapping {
	sector_t fsblk;
	sector_t logblk;
};

/*
 * An RB-tree entry wrapper for blk_mapping with extra housekeeping information.
 */
struct jmap_entry {
	struct rb_node rb_node;

	/* The actual mapping information. */
	struct blk_mapping mapping;

	/*
	 * If a block that is mapped gets deleted, the revoked bit is set.  A
	 * lookup for a deleted block fails.  If a deleted block gets
	 * re-allocated as a metadata block, the mapping is updated and revoked
	 * bit is cleared.
	 */
	bool revoked;

	/*
	 * All log blocks that are part of the same transaction in the log are
	 * chained with a linked list.  The root of the list is stored in the
	 * transaction_info structure described below.
	 */
	struct list_head list;

	/*
	 * The last time when fsblk was written again to the journal and
	 * therefore was remapped to a different log block.
	 */
	unsigned long fsblk_last_modified;

	/*
	 * Index of the transaction in the transaction_info_buffer (described
	 * below) of which the log block is part of.
	 */
	int t_idx;
};

/*
 * Housekeeping information about committed transaction.
 */
struct transaction_info {
	/* Id of the transaction */
	tid_t tid;

	/* Offset where the transaction starts in the log */
	sector_t offset;

	/*
	 * A list of live blocks referenced in the RB-tree that belong to this
	 * transaction.  It is used during cleaning to locate live blocks and
	 * migrate them to appropriate location.  If this list is empty, then
	 * the transaction does not contain any live blocks and we can reuse its
	 * space.  If this list is not empty, then we can quickly locate all the
	 * live blocks in this transaction.
	 */
	struct list_head live_blks;
};

/*
 * An array of transaction_info structures about all the transactions in the
 * log.  Since there can only be a limited number of transactions in the log, we
 * use a circular buffer to store housekeeping information about transactions.
 */
struct transaction_infos {
	struct transaction_info *buf;
	int head;
	int tail;
};

extern int jbd2_smr_journal_init(journal_t *journal);
extern void jbd2_smr_journal_exit(journal_t *journal);

extern int jbd2_journal_init_jmap_cache(void);
extern void jbd2_journal_destroy_jmap_cache(void);

extern int jbd2_init_transaction_infos(journal_t *journal);
extern void jbd2_free_transaction_infos(journal_t *journal);
extern int jbd2_transaction_infos_add(journal_t *journal,
				transaction_t *transaction,
				struct blk_mapping *mappings,
				int nr_mappings);

extern struct jmap_entry *jbd2_jmap_lookup(journal_t *journal, sector_t fsblk,
					const char *func);
extern void jbd2_jmap_revoke(journal_t *journal, sector_t fsblk);
extern void jbd2_jmap_cancel_revoke(journal_t *journal, sector_t fsblk);
extern void jbd2_submit_bh(journal_t *journal, int rw, int op_flags,
			   struct buffer_head *bh, const char *func);
extern int read_block_from_log(journal_t *journal, struct buffer_head *bh,
			       int op_flags, sector_t blk);
extern void jbd2_ll_rw_block(journal_t *journal, int rw, int op_flags, int nr,
			     struct buffer_head *bhs[], const char *func);
extern int jbd2_bh_submit_read(journal_t *journal, struct buffer_head *bh,
			       const char *func);
extern void jbd2_sb_breadahead(journal_t *journal, struct super_block *sb,
			       sector_t block);

/*
 * Cleaner stuff is below.
 */

/*
 * Number of blocks to read at once, for cleaning.
 */
#define CLEANER_BATCH_SIZE 16

/*
 * Context structure for the cleaner.
 */
struct cleaner_ctx {
	/*
	 * We set to true once we drop below low watermark and it stays so until
	 * we rise above the high watermark.  It is accessed by the commit
	 * thread and the foreground kernel threads during the journal
	 * destruction, therefore it is atomic.
	 */
	atomic_t cleaning;

	/*
	 * We clean in batches of blocks.  This flag indicates if we are
	 * currently cleaning a batch.  It is accessed by the commit thread and
	 * the cleaner thread, therefore it is atomic.
	 */
	atomic_t batch_in_progress;

	/*
	 * We find live blocks to clean from the live blocks list of the
	 * transaction at the tail.  This list can be larger than our batch size
	 * and we may need several attempts to process it.  We cache the
	 * position of the next entry to start from in |pos|.  Since cleaner
	 * thread can run concurrently with the commit thread that can modify
	 * the live blocks list of the transaction at the tail (for example, if
	 * it needs to drop a revoked entry or if |pos| points to an entry that
	 * has been updated and should move from the live blocks list of the
	 * transaction at the tail to the live blocks list of current
	 * transaction) we protect |pos| with |pos_lock|.
	 */
	struct jmap_entry *pos;
	spinlock_t pos_lock;

	/*
	 * Live block mappings for the blocks that we copy in a batch.
	 */
	struct blk_mapping mappings[CLEANER_BATCH_SIZE];

	/*
	 * Buffer heads for the live blocks read in a batch.
	 */
	struct buffer_head *bhs[CLEANER_BATCH_SIZE];

	/*
	 * Number of pending reads in a batch.  Every submitted read increments
	 * it and every completed read decrements it.
	 */
	atomic_t nr_pending_reads;

	/*
	 * The cleaner thread sleeps on this condition variable until the last
	 * completed read wakes the up the cleaner thread.
	 */
	struct completion live_block_reads;

	/* TODO: temporary for debugging, remove once done. */
	atomic_t nr_txns_committed;
	atomic_t nr_txns_cleaned;

	journal_t *journal;
	struct work_struct work;
};

extern int low_on_space(journal_t *journal);
extern int high_on_space(journal_t *journal);
extern bool cleaning(journal_t *journal);
extern void stop_cleaning(journal_t *journal);
extern void start_cleaning(journal_t *journal);
extern void clean_next_batch(journal_t *journal);
extern bool cleaning_batch_complete(journal_t *journal);
extern bool try_to_move_tail(journal_t *journal);

#endif
