#include <linux/blk_types.h>
#include <linux/jbd2.h>
#include <linux/jmap.h>
#include <linux/list.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <trace/events/jbd2.h>

inline int low_on_space(journal_t *journal)
{
	int x = atomic_read(&journal->j_cleaner_ctx->nr_txns_committed);
	if (x > 10) {
		trace_jbd2_jmap_printf1("low on space", x);
		return true;
	}
	trace_jbd2_jmap_printf1("not low on space", x);
	return false;
}

inline int high_on_space(journal_t *journal)
{
	if (atomic_read(&journal->j_cleaner_ctx->nr_txns_cleaned) < 2) {
		trace_jbd2_jmap_printf("not enough cleaned");
		return false;
	}
	trace_jbd2_jmap_printf("enough cleaned");
	atomic_set(&journal->j_cleaner_ctx->nr_txns_cleaned, 0);
	atomic_set(&journal->j_cleaner_ctx->nr_txns_committed, 0);
	return true;
}

inline bool cleaning(journal_t *journal)
{
	return atomic_read(&journal->j_cleaner_ctx->cleaning);
}

inline void stop_cleaning(journal_t *journal)
{
	trace_jbd2_jmap_printf("stopped cleaning");
	atomic_set(&journal->j_cleaner_ctx->cleaning, 0);
}

inline void start_cleaning(journal_t *journal)
{
	trace_jbd2_jmap_printf("started cleaning");
	atomic_set(&journal->j_cleaner_ctx->cleaning, 1);
	clean_next_batch(journal);
}

inline bool cleaning_batch_complete(journal_t *journal)
{
	return cleaning(journal) &&
		atomic_read(&journal->j_cleaner_ctx->batch_in_progress) == 0;
}

/*
 * Tries to move the tail forward (hence free space) as long as the transaction
 * at the tail has only stale blocks.  Returns true if manages to free a
 * transaction, false otherwise.
 */
bool try_to_move_tail(journal_t *journal)
{
	struct transaction_infos *tis = journal->j_transaction_infos;
	struct transaction_info *ti, *ti1;

	/*
	 * Advance the tail as far as possible by skipping over transactions
	 * with no live blocks.
	 */
	write_lock(&journal->j_jmap_lock);
	ti = ti1 = &tis->buf[tis->tail];

	for ( ; list_empty(&ti->live_blks); ti = &tis->buf[tis->tail]) {
		trace_jbd2_jmap_printf2("cleaned a transaction",
					tis->tail, ti->tid);
		tis->tail = (tis->tail + 1) & (MAX_LIVE_TRANSACTIONS - 1);
		atomic_inc(&journal->j_cleaner_ctx->nr_txns_cleaned);
	}
	write_unlock(&journal->j_jmap_lock);

	if (ti == ti1)
		return false;
	/*
	 * In the worst case, this will end up updating the journal superblock
	 * after cleaning up every transaction.  Should we avoid it?
	 */
	write_unlock(&journal->j_state_lock);
	jbd2_update_log_tail(journal, ti->tid, ti->offset);
	write_lock(&journal->j_state_lock);

	return true;
}

/*
 * Finds the live blocks at the tail transaction and copies the corresponding
 * mappings to |ctx->mappings|.  Returns the number of live block mappings
 * copied.  Should be called with a read lock on |j_jmap_lock|.
 */
int find_live_blocks(struct cleaner_ctx *ctx)
{
	journal_t *journal = ctx->journal;
	struct transaction_infos *tis = journal->j_transaction_infos;
	struct transaction_info *ti = &tis->buf[tis->tail];
	struct jmap_entry *je = NULL;
	int i, nr_live = 0;

	if (unlikely(list_empty(&ti->live_blks)))
		goto done;

	spin_lock(&ctx->pos_lock);
	if (!ctx->pos)
		ctx->pos = list_first_entry(&ti->live_blks, typeof(*je), list);
	je = ctx->pos;
	spin_unlock(&ctx->pos_lock);

	list_for_each_entry_from(je, &ti->live_blks, list) {
		if (je->revoked)
			continue;
		ctx->mappings[nr_live++] = je->mapping;
		if (nr_live == CLEANER_BATCH_SIZE)
			break;
	}

done:
	trace_jbd2_jmap_printf1("found live blocks", nr_live);
	for (i = 0; i < nr_live; ++i)
		trace_jbd2_jmap_printf2("m",
					ctx->mappings[i].fsblk,
					ctx->mappings[i].logblk);
	return nr_live;
}

void live_block_read_end_io(struct buffer_head *bh, int uptodate)
{
	struct cleaner_ctx *ctx = bh->b_private;

	if (uptodate) {
		set_buffer_uptodate(bh);
		if (atomic_dec_and_test(&ctx->nr_pending_reads))
			complete(&ctx->live_block_reads);
	} else {
		WARN_ON(1);
		clear_buffer_uptodate(bh);
	}

	unlock_buffer(bh);
	put_bh(bh);
}

/*
 * Reads live blocks in |ctx->mappings| populated by find_live_blocks into
 * buffer heads in |ctx->bhs|.  Returns true if at least one of the reads goes
 * out to disk and false otherwise.  If this function returns true then the
 * client should sleep on the condition variable |ctx->live_block_reads|.  The
 * client will be woken up when all reads are complete, through the end_io
 * handler attached to buffer heads read from disk.
 */
bool read_live_blocks(struct cleaner_ctx *ctx, int nr_live)
{
	journal_t *journal = ctx->journal;
	bool slow = false;
	struct blk_plug plug;
	bool plugged = false;
	int i, rc;

	for (i = 0; i < nr_live; ++i) {
		ctx->bhs[i] = __getblk(journal->j_dev, ctx->mappings[i].fsblk,
				journal->j_blocksize);
		if (unlikely(!ctx->bhs[i]))
			goto out_err;
		if (buffer_uptodate(ctx->bhs[i]))
			continue;
		plugged = true;
		blk_start_plug(&plug);
		lock_buffer(ctx->bhs[i]);
		ctx->bhs[i]->b_private = ctx;
		ctx->bhs[i]->b_end_io = live_block_read_end_io;
		atomic_inc(&ctx->nr_pending_reads);
		get_bh(ctx->bhs[i]);
		rc = read_block_from_log(ctx->journal, ctx->bhs[i],
					 REQ_RAHEAD, ctx->mappings[i].logblk);
		if (unlikely(rc < 0))
			goto out_err;
		if (rc) {
			slow = true;
			trace_jbd2_jmap_printf2("reading from disk",
						ctx->mappings[i].fsblk,
						ctx->mappings[i].logblk);
		} else {
			trace_jbd2_jmap_printf2("cached",
						ctx->mappings[i].fsblk,
						ctx->mappings[i].logblk);
		}
	}
	if (plugged)
		blk_finish_plug(&plug);
	return slow;

out_err:
	jbd2_journal_abort(ctx->journal, -ENOMEM);
	return false;
}

/*
 * This function finds the live blocks that became stale between the call to
 * find_live_blocks and now, and discards them.  It returns true if there are no
 * more live blocks left at the tail transaction.
 */
bool discard_stale_blocks(struct cleaner_ctx *ctx, int nr_live)
{
	journal_t *journal = ctx->journal;
	struct transaction_infos *tis = journal->j_transaction_infos;
	struct transaction_info *ti = &tis->buf[tis->tail];
	struct jmap_entry *je = NULL;
	int i = 0, j = 0, next = 0;

	trace_jbd2_jmap_printf(__func__);
	spin_lock(&ctx->pos_lock);
	BUG_ON(!ctx->pos);
	je = ctx->pos;
	list_for_each_entry_from(je, &ti->live_blks, list) {
		for (j = next; j < nr_live; ++j) {
			if (je->mapping.fsblk == ctx->mappings[j].fsblk) {
				next = j+1;
				ctx->pos = list_next_entry(je, list);
				if (je->revoked) {
					brelse(ctx->bhs[j]);
					ctx->bhs[j] = NULL;
					trace_jbd2_jmap_printf2(
						"revoked",
						ctx->mappings[i].fsblk,
						ctx->mappings[i].logblk);
				}
				break;
			} else {
				trace_jbd2_jmap_printf2(
						"moved to another list",
						ctx->mappings[i].fsblk,
						ctx->mappings[i].logblk);
				brelse(ctx->bhs[j]);
				ctx->bhs[j] = NULL;
			}
		}
		if (++i == nr_live || j == nr_live)
			break;
	}
	spin_unlock(&ctx->pos_lock);

	/*
	 * We have exited the loop.  If we haven't processed all the entries in
	 * |ctx->mappings|, that is if (j < nr_live) at the exit, and we have
	 * not processed |nr_live| entries from the live blocks list at the
	 * tail, that is if (i < nr_live) at the exit, then the live blocks list
	 * has shrunk and the tail transaction has no live blocks left.
	 */
	return j < nr_live && i < nr_live;
}

void attach_live_blocks(struct cleaner_ctx *ctx, handle_t *handle, int nr_live)
{
	int err, i;

	trace_jbd2_jmap_printf(__func__);
	for (i = 0; i < nr_live; ++i) {
		if (!ctx->bhs[i])
			continue;
		trace_jbd2_jmap_printf2("attaching",
					ctx->mappings[i].fsblk,
					ctx->mappings[i].logblk);
		err = jbd2_journal_get_write_access(handle, ctx->bhs[i]);
		if (!err)
			err = jbd2_journal_dirty_metadata(handle, ctx->bhs[i]);
		if (err) {
			jbd2_journal_abort(ctx->journal, err);
			return;
		}
	}
}

/*
 * Read the live blocks from the tail transaction and attach them to the current
 * transaction.
 */
static void do_clean_batch(struct work_struct *work)
{
	struct cleaner_ctx *ctx = container_of(work, struct cleaner_ctx, work);
	bool wake_up_commit_thread = true;
	handle_t *handle = NULL;
	int nr_live, err;

	read_lock(&ctx->journal->j_jmap_lock);
	nr_live = find_live_blocks(ctx);
	read_unlock(&ctx->journal->j_jmap_lock);

	if (nr_live < CLEANER_BATCH_SIZE)
		wake_up_commit_thread = false;
	if (nr_live == 0)
		goto done;

	reinit_completion(&ctx->live_block_reads);
	if (read_live_blocks(ctx, nr_live)) {
		trace_jbd2_jmap_printf("waiting for completion");
		wait_for_completion(&ctx->live_block_reads);
	} else {
		trace_jbd2_jmap_printf("not waiting for completion");
	}

	handle = jbd2_journal_start(ctx->journal, nr_live);
	if (IS_ERR(handle)) {
		jbd2_journal_abort(ctx->journal, PTR_ERR(handle));
		return;
	}

	read_lock(&ctx->journal->j_jmap_lock);
	if (discard_stale_blocks(ctx, nr_live))
		wake_up_commit_thread = false;
	attach_live_blocks(ctx, handle, nr_live);
	read_unlock(&ctx->journal->j_jmap_lock);

	err = jbd2_journal_stop(handle);
	if (err) {
		jbd2_journal_abort(ctx->journal, err);
		return;
	}

done:
	atomic_set(&ctx->batch_in_progress, 0);
	atomic_inc(&ctx->nr_txns_cleaned);
	if (wake_up_commit_thread) {
		trace_jbd2_jmap_printf("waking up commit thread");
		wake_up(&ctx->journal->j_wait_commit);
	} else {
		trace_jbd2_jmap_printf("not waking up commit thread");
		spin_lock(&ctx->pos_lock);
		ctx->pos = NULL;
		spin_unlock(&ctx->pos_lock);
	}
}

/*
 * Schedules the next batch of cleaning.
 */
void clean_next_batch(journal_t *journal)
{
	struct cleaner_ctx *ctx = journal->j_cleaner_ctx;

	if (!cleaning_batch_complete(journal)) {
		trace_jbd2_jmap_printf("not scheduling a new batch");
		return;
	}

	trace_jbd2_jmap_printf("scheduling a batch");
	BUG_ON(atomic_read(&ctx->nr_pending_reads));

	atomic_set(&ctx->batch_in_progress, 1);
	INIT_WORK(&ctx->work, do_clean_batch);
	schedule_work(&ctx->work);
}
