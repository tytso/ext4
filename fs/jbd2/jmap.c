#include <linux/blk_types.h>
#include <linux/jbd2.h>
#include <linux/jmap.h>
#include <trace/events/jbd2.h>

static struct kmem_cache *jbd2_jmap_cache;

int jbd2_journal_init_jmap_cache(void)
{
	jbd2_jmap_cache = KMEM_CACHE(jmap_entry, SLAB_RECLAIM_ACCOUNT);
	if (!jbd2_jmap_cache)
		return -ENOMEM;
	return 0;
}

void jbd2_journal_destroy_jmap_cache(void)
{
	if (jbd2_jmap_cache)
		kmem_cache_destroy(jbd2_jmap_cache);
	jbd2_jmap_cache = NULL;
}

/*
 * Allocate an array of transaction_info structures and initialize the list
 * heads inside them.
 */
int jbd2_init_transaction_infos(journal_t *journal)
{
	int i;
	struct transaction_infos *tis = kzalloc(sizeof(*tis), GFP_KERNEL);
	if (!tis)
		return -ENOMEM;

	tis->buf = kzalloc(sizeof(*tis->buf) * MAX_LIVE_TRANSACTIONS,
			GFP_KERNEL);
	if (!tis->buf) {
		kfree(tis);
		return -ENOMEM;
	}

	for (i = 0; i < MAX_LIVE_TRANSACTIONS; ++i)
		INIT_LIST_HEAD(&tis->buf[i].live_logblks);

	journal->j_transaction_infos = tis;
	return 0;
}

/*
 * Free the array of transaction_info structures.
 */
void jbd2_free_transaction_infos(journal_t *journal)
{
	struct transaction_infos *tis = journal->j_transaction_infos;
	if (!tis)
		return;
	kfree(tis->buf);
	kfree(tis);
}

/*
 * Fill an entry to be stored in jmap.
 */
static void fill_entry(struct jmap_entry *entry, struct blk_mapping *mapping,
			int t_idx, struct list_head *list)
{
	entry->mapping = *mapping;
	entry->fsblk_last_modified = jiffies;
	entry->t_idx = t_idx;
	list_add(&entry->list, list);
}

/*
 * A helper function for jbd2_transaction_infos_add.  Scans through the mappings
 * array, dropping revoked entries from jmap and updating existing entries.
 * Moves the new mappings to the beginning of the mappings array and returns the
 * number of new mappings.  Should be called with a write lock on j_jmap_lock.
 */
static int process_existing_mappings(journal_t *journal,
				struct transaction_info *ti, int t_idx,
				struct blk_mapping *mappings, int nr_mappings)
{
	struct jmap_entry *je;
	int i, nr_new = 0;

	for (i = 0; i < nr_mappings; ++i) {
		je = jbd2_jmap_lookup(journal, mappings[i].fsblk, __func__);
		if (!je) {
			mappings[nr_new++] = mappings[i];
			continue;
		}
		/*
		 * We are either deleting the entry because it was revoked, or
		 * we are moving it to the live blocks list of this transaction.
		 * In either case, we remove it from its existing list.
		 */
		list_del(&je->list);

		if (je->revoked) {
			rb_erase(&je->rb_node, &journal->j_jmap);
			kmem_cache_free(jbd2_jmap_cache, je);
		} else {
			trace_jbd2_jmap_replace(je, &mappings[i], t_idx);
			fill_entry(je, &mappings[i], t_idx, &ti->live_logblks);
		}
	}
	return nr_new;
}

/*
 * A helper function for jbd2_transaction_infos_add.  Allocates an array of
 * jmap_entry structures and returns the pointer to array if successful.
 * Otherwise, returns NULL.
 */
static struct jmap_entry **alloc_jmap_entries(int nr_entries)
{
	struct jmap_entry **jmap_entries;
	int i;

	jmap_entries = kmalloc(sizeof(struct jmap_entry *) * nr_entries,
			GFP_NOFS);
	if (!jmap_entries)
		return NULL;

	for (i = 0; i < nr_entries; i++) {
		jmap_entries[i] = kmem_cache_zalloc(jbd2_jmap_cache, GFP_NOFS);
		if (!jmap_entries[i])
			goto out_err;
	}
	return jmap_entries;

out_err:
	for (i = 0; i < nr_entries && jmap_entries[i]; ++i)
		kmem_cache_free(jbd2_jmap_cache, jmap_entries[i]);
	kfree(jmap_entries);
	return NULL;
}

/*
 * A helper function for jbd2_transaction_infos_add.  Adds new mappings to jmap
 * and updates the linked list of live logblks of the new transaction.  Should
 * be called with write lock on j_jmap_lock.
 */
static void add_new_mappings(journal_t *journal, struct transaction_info *ti,
			int t_idx, struct blk_mapping *mappings,
			struct jmap_entry **new_entries, int nr_new)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct jmap_entry *je;
	int i;

	for (i = 0; i < nr_new; ++i) {
		p = &journal->j_jmap.rb_node;
		while (*p) {
			parent = *p;
			je = rb_entry(parent, struct jmap_entry, rb_node);

			if (mappings[i].fsblk < je->mapping.fsblk)
				p = &(*p)->rb_left;
			else if (mappings[i].fsblk > je->mapping.fsblk)
				p = &(*p)->rb_right;
			else
				BUG_ON(1);
		}
		fill_entry(new_entries[i], &mappings[i], t_idx,
			&ti->live_logblks);
		rb_link_node(&new_entries[i]->rb_node, parent, p);
		rb_insert_color(&new_entries[i]->rb_node, &journal->j_jmap);
		trace_jbd2_jmap_insert(&mappings[i], t_idx);
	}
}

/*
 * This function is called after a transaction commits.  It adds new
 * transaction_info structure to transaction_infos and populates jmap map with
 * the new mappings that are part of the committed transaction.  It also adds
 * all the mappings to the linked list that is part of the transaction_info
 * structure.
 */
int jbd2_transaction_infos_add(journal_t *journal, transaction_t *transaction,
			struct blk_mapping *mappings, int nr_mappings)
{
	struct transaction_infos *tis = journal->j_transaction_infos;
	int t_idx = tis->head;
	struct transaction_info *ti = &tis->buf[t_idx];
	struct jmap_entry **new_entries = NULL;
	int nr_new = 0;

	/*
	 * We are possibly reusing space of an old transaction_info.  The old
	 * transaction should not have any live blocks in it.
	 */
	BUG_ON(!list_empty(&ti->live_logblks));

	write_lock(&journal->j_jmap_lock);
	nr_new = process_existing_mappings(journal, ti, t_idx, mappings,
					nr_mappings);
	write_unlock(&journal->j_jmap_lock);

	if (nr_new == 0)
		goto move_head;

	new_entries = alloc_jmap_entries(nr_new);
	if (!new_entries)
		return -ENOMEM;

	write_lock(&journal->j_jmap_lock);
	add_new_mappings(journal, ti, t_idx, mappings, new_entries, nr_new);
	write_unlock(&journal->j_jmap_lock);

	kfree(new_entries);

move_head:
	write_lock(&journal->j_jmap_lock);
	ti->tid = transaction->t_tid;
	ti->offset = transaction->t_log_start;
	tis->head = (tis->head + 1) & (MAX_LIVE_TRANSACTIONS - 1);
	write_unlock(&journal->j_jmap_lock);

	trace_jbd2_transaction_infos_add(t_idx, ti, nr_mappings);
	return 0;
}

/*
 * Look up fsblk in the jmap and return the corresponding jmap entry if found.
 * Should be called with a read lock on j_jmap_lock.
 */
struct jmap_entry *jbd2_jmap_lookup(journal_t *journal, sector_t fsblk,
				const char *func)
{
	struct rb_node *p;

	BUG_ON(!journal);

	for (p = journal->j_jmap.rb_node; p; ) {
		struct jmap_entry *je = rb_entry(p, struct jmap_entry, rb_node);
		if (je->mapping.fsblk > fsblk)
			p = p->rb_left;
		else if (je->mapping.fsblk < fsblk)
			p = p->rb_right;
		else {
			trace_jbd2_jmap_lookup(fsblk, je->mapping.logblk, func);
			return je;
		}
	}
	trace_jbd2_jmap_lookup(fsblk, 0, func);
	return NULL;
}

/*
 * Revoke a mapping for the fsblk in the jmap.  A lookup for fsblk will return
 * NULL and the mapping will be removed from the jmap during commit, unless
 * fsblk is reallocated as a metadata block.
 */
void jbd2_jmap_revoke(journal_t *journal, sector_t fsblk)
{
	struct jmap_entry *je;

	write_lock(&journal->j_jmap_lock);
	je = jbd2_jmap_lookup(journal, fsblk, __func__);
	/*
	 * For now, since we do not construct jmap from the journal, it is
	 * possible that a metadata block that was revoked is not in the jmap.
	 * Eventually, this should not be the case and we should have a
	 * BUG_ON(!je) here.
	 */
	if (je) {
		BUG_ON(je->revoked);
		je->revoked = true;
	}
	write_unlock(&journal->j_jmap_lock);
}

/*
 * Cancel a revoke for the fsblk in the jmap.
 */
void jbd2_jmap_cancel_revoke(journal_t *journal, sector_t fsblk)
{
	struct jmap_entry *je;

	write_lock(&journal->j_jmap_lock);
	je = jbd2_jmap_lookup(journal, fsblk, __func__);
	BUG_ON(!je);
	BUG_ON(!je->revoked);
	je->revoked = false;
	write_unlock(&journal->j_jmap_lock);
}

/*
 * Read bh from its most up-to-date location, either from the file system or
 * from the log.
 *
 * If there is no mapping for the bh in jmap, this function acts like submit_bh.
 * Otherwise, it submits a read for the block pointed by the mapping located in
 * the log.  Upon completion, bh will be filled with the contents of the block
 * read from the log.
 */
void jbd2_submit_bh(journal_t *journal, int rw, int op_flags,
		    struct buffer_head *bh, const char *func)
{
	sector_t fsblk = bh->b_blocknr;
	sector_t logblk;
	struct jmap_entry *je;

	BUG_ON(!buffer_locked(bh));

	if (!journal || !(journal->j_flags & JBD2_LAZY)) {
		submit_bh(rw, op_flags, bh);
		return;
	}

	read_lock(&journal->j_jmap_lock);
	je = jbd2_jmap_lookup(journal, fsblk, func);
	if (!je) {
		read_unlock(&journal->j_jmap_lock);
		submit_bh(rw, op_flags, bh);
		return;
	}
	logblk = je->mapping.logblk;
	read_unlock(&journal->j_jmap_lock);

	BUG_ON(rw == WRITE);
	read_block_from_log(journal, bh, op_flags, logblk);
}

/*
 * End_io handler for read_block_from_log that copies the contents of
 * log_bh read from log to the embedded bh.
 */
static void jbd2_end_log_read(struct buffer_head *log_bh, int uptodate)
{
	struct buffer_head *bh = log_bh->b_private;

	if (uptodate) {
		trace_jbd2_jmap_printf1("read from log", bh->b_blocknr);
		memcpy(bh->b_data, log_bh->b_data, log_bh->b_size);
	} else {
		trace_jbd2_jmap_printf1("failed to read from log", bh->b_blocknr);
	}

	unlock_buffer(log_bh);
	put_bh(log_bh);
	brelse(log_bh);

	bh->b_end_io(bh, uptodate);
}

/*
 * This function fills |bh| with the contents of the |blk|.  Assume
 * jmap maps metadata block 123 to log block 100123.  To read the
 * metadata block 123, we obtain a buffer head for it and call
 * read_block_from_log passing the obtained buffer head as |bh| and
 * 100123 as |blk|.  If block 100123 is cached, then we copy the
 * contents to |bh| and return.  Otherwise, we submit a request and
 * end_io handler copies the contents of block 100123 to |bh|.
 * Returns -ENOMEM if getblk fails, 1 if block is not cached, 0 if
 * block is cached.
 */
int read_block_from_log(journal_t *journal, struct buffer_head *bh,
			int op_flags, sector_t blk)
{
	struct buffer_head *log_bh;

	BUG_ON(!buffer_locked(bh));

	log_bh = __getblk(journal->j_fs_dev, blk, bh->b_size);
	if (unlikely(!log_bh)) {
		bh->b_end_io(bh, 0);
		return -ENOMEM;
	}

	lock_buffer(log_bh);
	if (buffer_uptodate(log_bh)) {
		memcpy(bh->b_data, log_bh->b_data, bh->b_size);
		unlock_buffer(log_bh);
		brelse(log_bh);
		bh->b_end_io(bh, 1);
		return 0;
	}

	log_bh->b_end_io = jbd2_end_log_read;
	log_bh->b_private = bh;
	get_bh(log_bh);
	submit_bh(READ, op_flags, log_bh);
	return 1;
}

/*
 * Copy of ll_rw_block that uses jbd2_submit_bh instead of submit_bh.
 */
void jbd2_ll_rw_block(journal_t *journal, int rw, int op_flags,
		      int nr, struct buffer_head *bhs[], const char *func)
{
	int i;

	for (i = 0; i < nr; i++) {
		struct buffer_head *bh = bhs[i];

		if (!trylock_buffer(bh))
			continue;
		BUG_ON(rw == WRITE);
		if (!buffer_uptodate(bh)) {
			bh->b_end_io = end_buffer_read_sync;
			get_bh(bh);
			jbd2_submit_bh(journal, rw, op_flags, bh, func);
			continue;
		}
		unlock_buffer(bh);
	}
}

/*
 * Copy of bh_submit_read that uses jbd2_submit_bh instead of submit_bh.
 */
int jbd2_bh_submit_read(journal_t *journal, struct buffer_head *bh,
			const char *func)
{
	BUG_ON(!buffer_locked(bh));

	if (buffer_uptodate(bh)) {
		unlock_buffer(bh);
		return 0;
	}

	get_bh(bh);
	bh->b_end_io = end_buffer_read_sync;
	jbd2_submit_bh(journal, READ, 0, bh, func);
	wait_on_buffer(bh);
	if (buffer_uptodate(bh))
		return 0;
	return -EIO;
}

int jbd2_smr_journal_init(journal_t *journal)
{
	journal->j_jmap = RB_ROOT;
	rwlock_init(&journal->j_jmap_lock);
	return jbd2_init_transaction_infos(journal);
}

void jbd2_smr_journal_exit(journal_t *journal)
{
	jbd2_free_transaction_infos(journal);
}

void jbd2_sb_breadahead(journal_t *journal, struct super_block *sb,
			sector_t block)
{
	struct buffer_head *bh = __getblk(sb->s_bdev, block, sb->s_blocksize);
	if (likely(bh)) {
		jbd2_ll_rw_block(journal, REQ_OP_READ, REQ_RAHEAD, 1,
				 &bh, __func__);
		brelse(bh);
	}
}
