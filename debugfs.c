/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: DebugFS specifics
 */

#include "bnxt_re.h"
#include "bnxt.h"
#include "debugfs.h"
#include "ib_verbs.h"
#include "hdbr.h"

#ifdef ENABLE_DEBUGFS

#define BNXT_RE_DEBUGFS_NAME_BUF_SIZE	128

static struct dentry *bnxt_re_debugfs_root;
extern unsigned int restrict_stats;

static void bnxt_re_print_ext_stat(struct bnxt_re_dev *rdev,
				   struct seq_file *s);

static const char *qp_type_str[] = {
	"IB_QPT_SMI",
	"IB_QPT_GSI",
	"IB_QPT_RC",
	"IB_QPT_UC",
	"IB_QPT_UD",
	"IB_QPT_RAW_IPV6",
	"IB_QPT_RAW_ETHERTYPE",
	"IB_QPT_UNKNOWN",
	"IB_QPT_RAW_PACKET",
	"IB_QPT_XRC_INI",
	"IB_QPT_XRC_TGT",
	"IB_QPT_MAX"
};

static const char *qp_state_str[] = {
	"IB_QPS_RESET",
	"IB_QPS_INIT",
	"IB_QPS_RTR",
	"IB_QPS_RTS",
	"IB_QPS_SQD",
	"IB_QPS_SQE",
	"IB_QPS_ERR"
};


static void bnxt_re_fill_qp_info(struct bnxt_re_qp *qp)
{
	struct bnxt_re_dev *rdev = qp->rdev;
	struct bnxt_qplib_qp *qplib_qp;
	u16 type, state;
	u8 *cur_ptr;
	int rc;

	cur_ptr = qp->qp_data;
	if (!cur_ptr)
		return;

	qplib_qp = kcalloc(1, sizeof(*qplib_qp), GFP_KERNEL);
	if (!qplib_qp)
		return;

	qplib_qp->id = qp->qplib_qp.id;
	rc = bnxt_qplib_query_qp(&rdev->qplib_res, qplib_qp);
	if (rc)
		goto bail;
	type = __from_hw_to_ib_qp_type(qp->qplib_qp.type);
	cur_ptr += sprintf(cur_ptr, "type \t = %s(%d)\n",
			   (type > IB_QPT_MAX) ?
			   "IB_QPT_UNKNOWN" : qp_type_str[type],
			   type);
	state =  __to_ib_qp_state(qplib_qp->state);
	cur_ptr += sprintf(cur_ptr, "state \t = %s(%d)\n",
			   (state > IB_QPS_ERR) ?
			   "IB_QPS_UNKNOWN" : qp_state_str[state],
			   state);
	cur_ptr += sprintf(cur_ptr, "source qpn \t = %d\n", qplib_qp->id);

	if (type != IB_QPT_UD) {
		cur_ptr += sprintf(cur_ptr, "dest qpn \t = %d\n", qplib_qp->dest_qpn);
		cur_ptr += sprintf(cur_ptr, "source port \t = %d\n", qp->qp_info_entry.s_port);
	}

	cur_ptr += sprintf(cur_ptr, "dest port \t = %d\n", qp->qp_info_entry.d_port);
	cur_ptr += sprintf(cur_ptr, "port \t = %d\n", qplib_qp->port_id);

	if (type != IB_QPT_UD) {
		if (qp->qplib_qp.nw_type == CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV4) {
			cur_ptr += sprintf(cur_ptr, "source_ipaddr \t = %pI4\n",
				   &qp->qp_info_entry.s_ip.ipv4_addr);
			cur_ptr += sprintf(cur_ptr, "destination_ipaddr \t = %pI4\n",
				   &qp->qp_info_entry.d_ip.ipv4_addr);
		} else {
			cur_ptr += sprintf(cur_ptr, "source_ipaddr \t = %pI6\n",
					   qp->qp_info_entry.s_ip.ipv6_addr);
			cur_ptr += sprintf(cur_ptr, "destination_ipaddr \t = %pI6\n",
					   qp->qp_info_entry.d_ip.ipv6_addr);
		}
	}
bail:
	kfree(qplib_qp);
}

static ssize_t bnxt_re_qp_info_qp_read(struct file *filp, char __user *buffer,
				       size_t usr_buf_len, loff_t *ppos)
{
	struct bnxt_re_qp *qp = filp->private_data;

	if (usr_buf_len < BNXT_RE_DEBUGFS_QP_INFO_MAX_SIZE)
		return -ENOSPC;

	if (!qp->qp_data)
		return -ENOMEM;

	if (*ppos >= BNXT_RE_DEBUGFS_QP_INFO_MAX_SIZE)
		return 0;

	bnxt_re_fill_qp_info(qp);

	return simple_read_from_buffer(buffer, usr_buf_len, ppos,
				       (u8 *)(qp->qp_data),
				       strlen((char *)qp->qp_data));
}

static const struct file_operations bnxt_re_qp_info_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_qp_info_qp_read,
};

void bnxt_re_qp_info_add_qpinfo(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp)
{
	char qp_name[32];

	qp->qp_data = kzalloc(BNXT_RE_DEBUGFS_QP_INFO_MAX_SIZE, GFP_KERNEL);
	if (!qp->qp_data)
		return;

	sprintf(qp_name, "0x%x", qp->qplib_qp.id);
	qp->qp_info_pdev_dentry = debugfs_create_file(qp_name, 0400,
						      rdev->pdev_qpinfo_dir,
						      qp,
						      &bnxt_re_qp_info_ops);
}

void bnxt_re_qp_info_rem_qpinfo(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp)
{
	debugfs_remove(qp->qp_info_pdev_dentry);
	qp->qp_info_pdev_dentry = NULL;

	kfree(qp->qp_data);
	qp->qp_data = NULL;
}

/* Clear the driver statistics maintained in the info file */
static ssize_t bnxt_re_info_debugfs_clear(struct file *fil, const char __user *u,
					  size_t size, loff_t *off)
{
	struct seq_file *m = fil->private_data;
	struct bnxt_re_dev *rdev = m->private;
	struct bnxt_re_res_cntrs *rsors;

	rsors = &rdev->stats.rsors;

	/* Clear the driver statistics only */
	atomic_set(&rsors->max_qp_count, atomic_read(&rsors->qp_count));
	atomic_set(&rsors->max_rc_qp_count, atomic_read(&rsors->rc_qp_count));
	atomic_set(&rsors->max_ud_qp_count, atomic_read(&rsors->ud_qp_count));
	atomic_set(&rsors->max_srq_count, atomic_read(&rsors->srq_count));
	atomic_set(&rsors->max_cq_count, atomic_read(&rsors->cq_count));
	atomic_set(&rsors->max_mr_count, atomic_read(&rsors->mr_count));
	atomic_set(&rsors->max_mw_count, atomic_read(&rsors->mw_count));
	atomic_set(&rsors->max_ah_count, atomic_read(&rsors->ah_count));
	atomic_set(&rsors->max_pd_count, atomic_read(&rsors->pd_count));
	atomic_set(&rsors->resize_count, 0);

	if (rdev->dbr_sw_stats) {
		rdev->dbr_sw_stats->dbq_int_recv = 0;
		rdev->dbr_sw_stats->dbq_int_en = 0;
		rdev->dbr_sw_stats->dbq_pacing_resched = 0;
		rdev->dbr_sw_stats->dbq_pacing_complete = 0;
		rdev->dbr_sw_stats->dbq_pacing_alerts = 0;

		rdev->dbr_evt_curr_epoch = 0;
		rdev->dbr_sw_stats->dbr_drop_recov_events = 0;
		rdev->dbr_sw_stats->dbr_drop_recov_timeouts = 0;
		rdev->dbr_sw_stats->dbr_drop_recov_timeout_users = 0;
		rdev->dbr_sw_stats->dbr_drop_recov_event_skips = 0;
	}

	return size;
}

/* Clear perf state irrespective value passed.
 * Any value written to debugfs entry will clear the stats
 */
static ssize_t bnxt_re_perf_debugfs_clear(struct file *fil, const char __user *u,
				     size_t size, loff_t *off)
{
	struct seq_file *m = fil->private_data;
	struct bnxt_re_dev *rdev = m->private;
	int i;

	if (!rdev->rcfw.sp_perf_stats_enabled)
		return size;

	for (i = 0; i < RCFW_MAX_STAT_INDEX; i++) {
		rdev->rcfw.qp_create_stats[i] = 0;
		rdev->rcfw.qp_destroy_stats[i] = 0;
		rdev->rcfw.mr_create_stats[i] = 0;
		rdev->rcfw.mr_destroy_stats[i] = 0;
		rdev->rcfw.qp_modify_stats[i] = 0;
	}

	rdev->rcfw.qp_create_stats_id = 0;
	rdev->rcfw.qp_destroy_stats_id = 0;
	rdev->rcfw.mr_create_stats_id = 0;
	rdev->rcfw.mr_destroy_stats_id = 0;
	rdev->rcfw.qp_modify_stats_id = 0;

	for (i = 0; i < RCFW_MAX_LATENCY_MSEC_SLAB_INDEX; i++)
		rdev->rcfw.rcfw_lat_slab_msec[i] = 0;

	return size;
}

/* Clear the driver debug statistics */
static ssize_t bnxt_re_drv_stats_debugfs_clear(struct file *fil, const char __user *u,
					       size_t size, loff_t *off)
{
	struct seq_file *m = fil->private_data;
	struct bnxt_re_dev *rdev = m->private;


	rdev->dbg_stats->dbq.fifo_occup_slab_1 = 0;
	rdev->dbg_stats->dbq.fifo_occup_slab_2 = 0;
	rdev->dbg_stats->dbq.fifo_occup_slab_3 = 0;
	rdev->dbg_stats->dbq.fifo_occup_slab_4 = 0;
	rdev->dbg_stats->dbq.fifo_occup_water_mark = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_1 = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_2 = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_3 = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_4 = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_5 = 0;
	rdev->dbg_stats->dbq.do_pacing_water_mark = 0;
	rdev->dbg_stats->dbq.do_pacing_retry = 0;

	return size;
}

static void bnxt_re_print_roce_only_counters(struct bnxt_re_dev *rdev,
					     struct seq_file *s)
{
	struct bnxt_re_ro_counters *roce_only = &rdev->stats.dstat.cur[0];

	/* Do not polulate RoCE Only stats for VF from  Thor onwards */
	if (_is_chip_gen_p5_p7(rdev->chip_ctx) && rdev->is_virtfn)
		return;

	seq_printf(s, "\tRoCE Only Rx Pkts: %llu\n", roce_only->rx_pkts);
	seq_printf(s, "\tRoCE Only Rx Bytes: %llu\n", roce_only->rx_bytes);
	seq_printf(s, "\tRoCE Only Tx Pkts: %llu\n", roce_only->tx_pkts);
	seq_printf(s, "\tRoCE Only Tx Bytes: %llu\n", roce_only->tx_bytes);
}

static void bnxt_re_print_normal_total_counters(struct bnxt_re_dev *rdev,
					      struct seq_file *s)
{

	if (_is_chip_gen_p5_p7(rdev->chip_ctx) && rdev->is_virtfn) {
		struct bnxt_re_rdata_counters *rstat = &rdev->stats.dstat.rstat[0];

		/* Only for VF from Thor onwards */
		seq_printf(s, "\tRx Pkts: %llu\n", rstat->rx_ucast_pkts);
		seq_printf(s, "\tRx Bytes: %llu\n", rstat->rx_ucast_bytes);
		seq_printf(s, "\tTx Pkts: %llu\n", rstat->tx_ucast_pkts);
		seq_printf(s, "\tTx Bytes: %llu\n", rstat->tx_ucast_bytes);
	} else {
		struct bnxt_re_ro_counters *roce_only;
		struct bnxt_re_cc_stat *cnps;

		cnps = &rdev->stats.cnps;
		roce_only = &rdev->stats.dstat.cur[0];

		seq_printf(s, "\tRx Pkts: %llu\n", cnps->cur[0].cnp_rx_pkts +
			   roce_only->rx_pkts);
		seq_printf(s, "\tRx Bytes: %llu\n",
			   cnps->cur[0].cnp_rx_bytes + roce_only->rx_bytes);
		seq_printf(s, "\tTx Pkts: %llu\n",
			   cnps->cur[0].cnp_tx_pkts + roce_only->tx_pkts);
		seq_printf(s, "\tTx Bytes: %llu\n",
			   cnps->cur[0].cnp_tx_bytes + roce_only->tx_bytes);
	}
}

static void bnxt_re_print_bond_total_counters(struct bnxt_re_dev *rdev,
					      struct seq_file *s)
{
	struct bnxt_re_ro_counters *roce_only;
	struct bnxt_re_cc_stat *cnps;

	cnps = &rdev->stats.cnps;
	roce_only = &rdev->stats.dstat.cur[0];

	seq_printf(s, "\tRx Pkts: %llu\n",
		   cnps->cur[0].cnp_rx_pkts +
		   cnps->cur[1].cnp_rx_pkts +
		   roce_only[0].rx_pkts +
		   roce_only[1].rx_pkts);

	seq_printf(s, "\tRx Bytes: %llu\n",
		   cnps->cur[0].cnp_rx_bytes +
		   cnps->cur[1].cnp_rx_bytes +
		   roce_only[0].rx_bytes +
		   roce_only[1].rx_bytes);

	seq_printf(s, "\tTx Pkts: %llu\n",
		   cnps->cur[0].cnp_tx_pkts +
		   cnps->cur[1].cnp_tx_pkts +
		   roce_only[0].tx_pkts +
		   roce_only[1].tx_pkts);

	seq_printf(s, "\tTx Bytes: %llu\n",
		   cnps->cur[0].cnp_tx_bytes +
		   cnps->cur[1].cnp_tx_bytes +
		   roce_only[0].tx_bytes +
		   roce_only[1].tx_bytes);

	/* Disable per port stat display for gen-p5 */
	if (_is_chip_gen_p5_p7(rdev->chip_ctx))
		return;
	seq_printf(s, "\tRx Pkts P0: %llu\n",
		   cnps->cur[0].cnp_rx_pkts + roce_only[0].rx_pkts);
	seq_printf(s, "\tRx Bytes P0: %llu\n",
		   cnps->cur[0].cnp_rx_bytes + roce_only[0].rx_bytes);
	seq_printf(s, "\tTx Pkts P0: %llu\n",
		   cnps->cur[0].cnp_tx_pkts + roce_only[0].tx_pkts);
	seq_printf(s, "\tTx Bytes P0: %llu\n",
		   cnps->cur[0].cnp_tx_bytes + roce_only[0].tx_bytes);

	seq_printf(s, "\tRx Pkts P1: %llu\n",
		   cnps->cur[1].cnp_rx_pkts + roce_only[1].rx_pkts);
	seq_printf(s, "\tRx Bytes P1: %llu\n",
		   cnps->cur[1].cnp_rx_bytes + roce_only[1].rx_bytes);
	seq_printf(s, "\tTx Pkts P1: %llu\n",
		   cnps->cur[1].cnp_tx_pkts + roce_only[1].tx_pkts);
	seq_printf(s, "\tTx Bytes P1: %llu\n",
		   cnps->cur[1].cnp_tx_bytes + roce_only[1].tx_bytes);
}

static void bnxt_re_print_bond_roce_only_counters(struct bnxt_re_dev *rdev,
					     struct seq_file *s)
{
	struct bnxt_re_ro_counters *roce_only;

	roce_only = rdev->stats.dstat.cur;
	seq_printf(s, "\tRoCE Only Rx Pkts: %llu\n" ,roce_only[0].rx_pkts +
			roce_only[1].rx_pkts);
	seq_printf(s, "\tRoCE Only Rx Bytes: %llu\n", roce_only[0].rx_bytes +
			roce_only[1].rx_bytes);
	seq_printf(s, "\tRoCE Only Tx Pkts: %llu\n", roce_only[0].tx_pkts +
			roce_only[1].tx_pkts);
	seq_printf(s, "\tRoCE Only Tx Bytes: %llu\n", roce_only[0].tx_bytes +
			roce_only[1].tx_bytes);

	/* Disable per port stat display for gen-p5 onwards. */
	if (_is_chip_gen_p5_p7(rdev->chip_ctx))
		return;
	seq_printf(s, "\tRoCE Only Rx Pkts P0: %llu\n", roce_only[0].rx_pkts);
	seq_printf(s, "\tRoCE Only Rx Bytes P0: %llu\n", roce_only[0].rx_bytes);
	seq_printf(s, "\tRoCE Only Tx Pkts P0: %llu\n", roce_only[0].tx_pkts);
	seq_printf(s, "\tRoCE Only Tx Bytes P0: %llu\n", roce_only[0].tx_bytes);

	seq_printf(s, "\tRoCE Only Rx Pkts P1: %llu\n", roce_only[1].rx_pkts);
	seq_printf(s, "\tRoCE Only Rx Bytes P1: %llu\n", roce_only[1].rx_bytes);
	seq_printf(s, "\tRoCE Only Tx Pkts P1: %llu\n", roce_only[1].tx_pkts);
	seq_printf(s, "\tRoCE Only Tx Bytes P1: %llu\n", roce_only[1].tx_bytes);
}

static void bnxt_re_print_bond_counters(struct bnxt_re_dev *rdev,
					struct seq_file *s)
{
	struct bnxt_qplib_roce_stats *roce_stats;
	struct bnxt_re_rdata_counters *stats1;
	struct bnxt_re_rdata_counters *stats2;
	struct bnxt_re_cc_stat *cnps;
	long long oob_cnt = 0;
	bool en_disp;

	roce_stats = &rdev->stats.dstat.errs;
	stats1 = &rdev->stats.dstat.rstat[0];
	stats2 = &rdev->stats.dstat.rstat[1];
	cnps = &rdev->stats.cnps;
	en_disp = !_is_chip_gen_p5_p7(rdev->chip_ctx);

	seq_printf(s, "\tActive QPs P0: %lld\n", roce_stats->active_qp_count_p0);
	seq_printf(s, "\tActive QPs P1: %lld\n", roce_stats->active_qp_count_p1);

	bnxt_re_print_bond_total_counters(rdev, s);

	seq_printf(s, "\tCNP Tx Pkts: %llu\n",
		   cnps->cur[0].cnp_tx_pkts + cnps->cur[1].cnp_tx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Tx Bytes: %llu\n",
			   cnps->cur[0].cnp_tx_bytes +
			   cnps->cur[1].cnp_tx_bytes);
	seq_printf(s, "\tCNP Rx Pkts: %llu\n",
		   cnps->cur[0].cnp_rx_pkts + cnps->cur[1].cnp_rx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Rx Bytes: %llu\n",
			   cnps->cur[0].cnp_rx_bytes +
			   cnps->cur[1].cnp_rx_bytes);

	seq_printf(s, "\tCNP Tx Pkts P0: %llu\n", cnps->cur[0].cnp_tx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Tx Bytes P0: %llu\n",
			   cnps->cur[0].cnp_tx_bytes);
	seq_printf(s, "\tCNP Rx Pkts P0: %llu\n", cnps->cur[0].cnp_rx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Rx Bytes P0: %llu\n",
			   cnps->cur[0].cnp_rx_bytes);
	seq_printf(s, "\tCNP Tx Pkts P1: %llu\n", cnps->cur[1].cnp_tx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Tx Bytes P1: %llu\n",
			   cnps->cur[1].cnp_tx_bytes);
	seq_printf(s, "\tCNP Rx Pkts P1: %llu\n", cnps->cur[1].cnp_rx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Rx Bytes P1: %llu\n",
			   cnps->cur[1].cnp_rx_bytes);
	/* Print RoCE only bytes.. CNP counters include RoCE packets also */
	bnxt_re_print_bond_roce_only_counters(rdev, s);


	seq_printf(s, "\trx_roce_error_pkts: %lld\n",
		   (stats1 ? stats1->rx_error_pkts : 0) +
		   (stats2 ? stats2->rx_error_pkts : 0));
	seq_printf(s, "\trx_roce_discard_pkts: %lld\n",
		   (stats1 ? stats1->rx_discard_pkts : 0) +
		   (stats2 ? stats2->rx_discard_pkts : 0));
	if (!en_disp) {
		/* show only for Gen P5 or higher */
		seq_printf(s, "\ttx_roce_error_pkts: %lld\n",
			   (stats1 ? stats1->tx_error_pkts : 0) +
			   (stats2 ? stats2->tx_error_pkts : 0));
		seq_printf(s, "\ttx_roce_discard_pkts: %lld\n",
			   (stats1 ? stats1->tx_discard_pkts : 0) +
			   (stats2 ? stats2->tx_discard_pkts : 0));
	}
	/* No need to sum-up both port stat counts in bond mode */
	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn)) {
		seq_printf(s, "\tres_oob_drop_count: %lld\n",
			   rdev->stats.dstat.e_errs.oob);
		bnxt_re_print_ext_stat(rdev, s);
	} else {
		oob_cnt = (stats1 ? stats1->rx_discard_pkts : 0) +
			(stats2 ? stats2->rx_discard_pkts : 0) -
			rdev->stats.dstat.errs.res_oos_drop_count;

		/*
		 * oob count is calculated from the output of two seperate
		 * HWRM commands. To avoid reporting inconsistent values
		 * due to the time delta between two different queries,
		 * report newly calculated value only if it is more than the
		 * previously reported OOB value.
		 */
		if (oob_cnt < rdev->stats.dstat.prev_oob)
			oob_cnt = rdev->stats.dstat.prev_oob;
		seq_printf(s, "\tres_oob_drop_count: %lld\n", oob_cnt);
		rdev->stats.dstat.prev_oob = oob_cnt;
	}
}

static void bnxt_re_print_ext_stat(struct bnxt_re_dev *rdev,
				   struct seq_file *s)
{
	struct bnxt_re_ext_rstat *ext_s;
	struct bnxt_re_cc_stat *cnps;

	ext_s = &rdev->stats.dstat.ext_rstat[0];
	cnps = &rdev->stats.cnps;

	seq_printf(s, "\ttx_atomic_req: %llu\n", ext_s->tx.atomic_req);
	seq_printf(s, "\trx_atomic_req: %llu\n", ext_s->rx.atomic_req);
	seq_printf(s, "\ttx_read_req: %llu\n", ext_s->tx.read_req);
	seq_printf(s, "\ttx_read_resp: %llu\n", ext_s->tx.read_resp);
	seq_printf(s, "\trx_read_req: %llu\n", ext_s->rx.read_req);
	seq_printf(s, "\trx_read_resp: %llu\n", ext_s->rx.read_resp);
	seq_printf(s, "\ttx_write_req: %llu\n", ext_s->tx.write_req);
	seq_printf(s, "\trx_write_req: %llu\n", ext_s->rx.write_req);
	seq_printf(s, "\ttx_send_req: %llu\n", ext_s->tx.send_req);
	seq_printf(s, "\trx_send_req: %llu\n", ext_s->rx.send_req);
	seq_printf(s, "\trx_good_pkts: %llu\n", ext_s->grx.rx_pkts);
	seq_printf(s, "\trx_good_bytes: %llu\n", ext_s->grx.rx_bytes);
	if (_is_chip_p7(rdev->chip_ctx)) {
		seq_printf(s, "\trx_dcn_payload_cut: %llu\n", ext_s->rx_dcn_payload_cut);
		seq_printf(s, "\tte_bypassed: %llu\n", ext_s->te_bypassed);
	}

	if (rdev->binfo) {
		seq_printf(s, "\trx_ecn_marked_pkts: %llu\n",
			   cnps->cur[0].ecn_marked + cnps->cur[1].ecn_marked);
		seq_printf(s, "\trx_ecn_marked_pkts P0: %llu\n", cnps->cur[0].ecn_marked);
		seq_printf(s, "\trx_ecn_marked_pkts P1: %llu\n", cnps->cur[1].ecn_marked);
	} else {
		seq_printf(s, "\trx_ecn_marked_pkts: %llu\n", cnps->cur[0].ecn_marked);
	}
}

static void bnxt_re_print_normal_counters(struct bnxt_re_dev *rdev,
					  struct seq_file *s)
{
	struct bnxt_re_rdata_counters *stats;
	struct bnxt_re_cc_stat *cnps;
	bool en_disp;

	stats = &rdev->stats.dstat.rstat[0];
	cnps = &rdev->stats.cnps;
	en_disp = !_is_chip_gen_p5_p7(rdev->chip_ctx);

	bnxt_re_print_normal_total_counters(rdev, s);
	if (!rdev->is_virtfn) {
		seq_printf(s, "\tCNP Tx Pkts: %llu\n",
			   cnps->cur[0].cnp_tx_pkts);
		if (en_disp)
			seq_printf(s, "\tCNP Tx Bytes: %llu\n",
				   cnps->cur[0].cnp_tx_bytes);
		seq_printf(s, "\tCNP Rx Pkts: %llu\n",
			   cnps->cur[0].cnp_rx_pkts);
		if (en_disp)
			seq_printf(s, "\tCNP Rx Bytes: %llu\n",
				   cnps->cur[0].cnp_rx_bytes);
	}
	/* Print RoCE only bytes.. CNP counters include RoCE packets also */
	bnxt_re_print_roce_only_counters(rdev, s);

	seq_printf(s, "\trx_roce_error_pkts: %lld\n",
		   stats ? stats->rx_error_pkts : 0);
	seq_printf(s, "\trx_roce_discard_pkts: %lld\n",
		   stats ? stats->rx_discard_pkts : 0);
	if (!en_disp) {
		seq_printf(s, "\ttx_roce_error_pkts: %lld\n",
			   stats ? stats->tx_error_pkts : 0);
		seq_printf(s, "\ttx_roce_discards_pkts: %lld\n",
			   stats ? stats->tx_discard_pkts : 0);
	}

	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn)) {
		seq_printf(s, "\tres_oob_drop_count: %lld\n",
			   rdev->stats.dstat.e_errs.oob);
		bnxt_re_print_ext_stat(rdev, s);
	}
}

static int bnxt_re_info_debugfs_show(struct seq_file *s, void *unused)
{
	struct bnxt_re_dev *rdev = s->private;
	struct bnxt_re_ext_roce_stats *e_errs;
	struct bnxt_re_rdata_counters *rstat;
	struct bnxt_qplib_roce_stats *errs;
	unsigned long tstamp_diff;
	struct pci_dev *pdev;
	int sched_msec, i;
	int rc = 0;

	seq_printf(s, "bnxt_re debug info:\n");

	if (!bnxt_re_is_rdev_valid(rdev)) {
		rc = -ENODEV;
		goto err;
	}

	pdev = rdev->en_dev->pdev;

	errs = &rdev->stats.dstat.errs;
	rstat = &rdev->stats.dstat.rstat[0];
	e_errs = &rdev->stats.dstat.e_errs;
	sched_msec = BNXT_RE_STATS_CTX_UPDATE_TIMER;
	tstamp_diff = jiffies - rdev->stats.read_tstamp;
	if (test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags)) {
		if (restrict_stats && tstamp_diff <
		    msecs_to_jiffies(sched_msec))
			goto skip_query;
		rc = bnxt_re_get_device_stats(rdev);
		if (rc)
			dev_err(rdev_to_dev(rdev),
				"Failed to query device stats\n");
		rdev->stats.read_tstamp = jiffies;
	}
skip_query:
	seq_printf(s, "=====[ IBDEV %s ]=============================\n",
		   rdev->ibdev.name);
	if (rdev->netdev)
		seq_printf(s, "\tlink state: %s\n",
			   bnxt_re_link_state_str(rdev));
	seq_printf(s, "\tMax QP:\t\t%d\n", rdev->dev_attr->max_qp);
	seq_printf(s, "\tMax SRQ:\t%d\n", rdev->dev_attr->max_srq);
	seq_printf(s, "\tMax CQ:\t\t%d\n", rdev->dev_attr->max_cq);
	seq_printf(s, "\tMax MR:\t\t%d\n", rdev->dev_attr->max_mr);
	seq_printf(s, "\tMax MW:\t\t%d\n", rdev->dev_attr->max_mw);
	seq_printf(s, "\tMax AH:\t\t%d\n", rdev->dev_attr->max_ah);
	seq_printf(s, "\tMax PD:\t\t%d\n", rdev->dev_attr->max_pd);
	seq_printf(s, "\tActive QP:\t%d\n",
		   atomic_read(&rdev->stats.rsors.qp_count));
	seq_printf(s, "\tActive RC QP:\t%d\n",
		   atomic_read(&rdev->stats.rsors.rc_qp_count));
	seq_printf(s, "\tActive UD QP:\t%d\n",
		   atomic_read(&rdev->stats.rsors.ud_qp_count));
	seq_printf(s, "\tActive SRQ:\t%d\n",
		   atomic_read(&rdev->stats.rsors.srq_count));
	seq_printf(s, "\tActive CQ:\t%d\n",
		   atomic_read(&rdev->stats.rsors.cq_count));
	seq_printf(s, "\tActive MR:\t%d\n",
		   atomic_read(&rdev->stats.rsors.mr_count));
	seq_printf(s, "\tActive MW:\t%d\n",
		   atomic_read(&rdev->stats.rsors.mw_count));
	seq_printf(s, "\tActive AH:\t%d\n",
		   atomic_read(&rdev->stats.rsors.ah_count));
	seq_printf(s, "\tActive PD:\t%d\n",
		   atomic_read(&rdev->stats.rsors.pd_count));
	seq_printf(s, "\tQP Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_qp_count));
	seq_printf(s, "\tRC QP Watermark: %d\n",
		   atomic_read(&rdev->stats.rsors.max_rc_qp_count));
	seq_printf(s, "\tUD QP Watermark: %d\n",
		   atomic_read(&rdev->stats.rsors.max_ud_qp_count));
	seq_printf(s, "\tSRQ Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_srq_count));
	seq_printf(s, "\tCQ Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_cq_count));
	seq_printf(s, "\tMR Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_mr_count));
	seq_printf(s, "\tMW Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_mw_count));
	seq_printf(s, "\tAH Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_ah_count));
	seq_printf(s, "\tPD Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_pd_count));
	seq_printf(s, "\tResize CQ count: %d\n",
		   atomic_read(&rdev->stats.rsors.resize_count));
	seq_printf(s, "\tRecoverable Errors: %lld\n",
		   rstat ? rstat->tx_bcast_pkts : 0);
	if (rdev->binfo)
		bnxt_re_print_bond_counters(rdev, s);
	else
		bnxt_re_print_normal_counters(rdev, s);

	seq_printf(s, "\tmax_retry_exceeded: %llu\n", errs->max_retry_exceeded);
	/* handle Thor2 & ext attr stats supporting nics here */
	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn) &&
	    _is_hw_retx_supported(rdev->dev_attr->dev_cap_flags)) {
		seq_printf(s, "\tto_retransmits: %llu\n", e_errs->to_retransmits);
		seq_printf(s, "\tseq_err_naks_rcvd: %llu\n", e_errs->seq_err_naks_rcvd);
		seq_printf(s, "\trnr_naks_rcvd: %llu\n", e_errs->rnr_naks_rcvd);
		seq_printf(s, "\tmissing_resp: %llu\n", e_errs->missing_resp);
		if (_is_hw_resp_retx_supported(rdev->dev_attr->dev_cap_flags))
			seq_printf(s, "\tdup_reqs: %llu\n", e_errs->dup_req);
		else
			seq_printf(s, "\tdup_reqs: %llu\n", errs->dup_req);
	} else {
		seq_printf(s, "\tto_retransmits: %llu\n", errs->to_retransmits);
		seq_printf(s, "\tseq_err_naks_rcvd: %llu\n", errs->seq_err_naks_rcvd);
		seq_printf(s, "\trnr_naks_rcvd: %llu\n", errs->rnr_naks_rcvd);
		seq_printf(s, "\tmissing_resp: %llu\n", errs->missing_resp);
		seq_printf(s, "\tdup_req: %llu\n", errs->dup_req);
	}
	seq_printf(s, "\tunrecoverable_err: %llu\n", errs->unrecoverable_err);
	seq_printf(s, "\tbad_resp_err: %llu\n", errs->bad_resp_err);
	seq_printf(s, "\tlocal_qp_op_err: %llu\n", errs->local_qp_op_err);
	seq_printf(s, "\tlocal_protection_err: %llu\n", errs->local_protection_err);
	seq_printf(s, "\tmem_mgmt_op_err: %llu\n", errs->mem_mgmt_op_err);
	seq_printf(s, "\tremote_invalid_req_err: %llu\n", errs->remote_invalid_req_err);
	seq_printf(s, "\tremote_access_err: %llu\n", errs->remote_access_err);
	seq_printf(s, "\tremote_op_err: %llu\n", errs->remote_op_err);
	seq_printf(s, "\tres_exceed_max: %llu\n", errs->res_exceed_max);
	seq_printf(s, "\tres_length_mismatch: %llu\n", errs->res_length_mismatch);
	seq_printf(s, "\tres_exceeds_wqe: %llu\n", errs->res_exceeds_wqe);
	seq_printf(s, "\tres_opcode_err: %llu\n", errs->res_opcode_err);
	seq_printf(s, "\tres_rx_invalid_rkey: %llu\n", errs->res_rx_invalid_rkey);
	seq_printf(s, "\tres_rx_domain_err: %llu\n", errs->res_rx_domain_err);
	seq_printf(s, "\tres_rx_no_perm: %llu\n", errs->res_rx_no_perm);
	seq_printf(s, "\tres_rx_range_err: %llu\n", errs->res_rx_range_err);
	seq_printf(s, "\tres_tx_invalid_rkey: %llu\n", errs->res_tx_invalid_rkey);
	seq_printf(s, "\tres_tx_domain_err: %llu\n", errs->res_tx_domain_err);
	seq_printf(s, "\tres_tx_no_perm: %llu\n", errs->res_tx_no_perm);
	seq_printf(s, "\tres_tx_range_err: %llu\n", errs->res_tx_range_err);
	seq_printf(s, "\tres_irrq_oflow: %llu\n", errs->res_irrq_oflow);
	seq_printf(s, "\tres_unsup_opcode: %llu\n", errs->res_unsup_opcode);
	seq_printf(s, "\tres_unaligned_atomic: %llu\n", errs->res_unaligned_atomic);
	seq_printf(s, "\tres_rem_inv_err: %llu\n", errs->res_rem_inv_err);
	seq_printf(s, "\tres_mem_error64: %llu\n", errs->res_mem_error);
	seq_printf(s, "\tres_srq_err: %llu\n", errs->res_srq_err);
	seq_printf(s, "\tres_cmp_err: %llu\n", errs->res_cmp_err);
	seq_printf(s, "\tres_invalid_dup_rkey: %llu\n", errs->res_invalid_dup_rkey);
	seq_printf(s, "\tres_wqe_format_err: %llu\n", errs->res_wqe_format_err);
	seq_printf(s, "\tres_cq_load_err: %llu\n", errs->res_cq_load_err);
	seq_printf(s, "\tres_srq_load_err: %llu\n", errs->res_srq_load_err);
	seq_printf(s, "\tres_tx_pci_err: %llu\n", errs->res_tx_pci_err);
	seq_printf(s, "\tres_rx_pci_err: %llu\n", errs->res_rx_pci_err);
	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn)) {
		seq_printf(s, "\tres_oos_drop_count: %llu\n",
			   e_errs->oos);
	} else {
		/* Display on function 0 as OOS counters are chip-wide */
		if (PCI_FUNC(pdev->devfn) == 0)
			seq_printf(s, "\tres_oos_drop_count: %llu\n",
					errs->res_oos_drop_count);
	}

	seq_printf(s, "\tnum_irq_started : %u\n", rdev->rcfw.num_irq_started);
	seq_printf(s, "\tnum_irq_stopped : %u\n", rdev->rcfw.num_irq_stopped);
	seq_printf(s, "\tpoll_in_intr_en : %u\n", rdev->rcfw.poll_in_intr_en);
	seq_printf(s, "\tpoll_in_intr_dis : %u\n", rdev->rcfw.poll_in_intr_dis);
	seq_printf(s, "\tcmdq_full_dbg_cnt : %u\n", rdev->rcfw.cmdq_full_dbg);
	if (!rdev->is_virtfn)
		seq_printf(s, "\tfw_service_prof_type_sup : %u\n",
			   is_qport_service_type_supported(rdev));
	if (rdev->dbr_pacing) {
		seq_printf(s, "\tdbq_int_recv: %llu\n", rdev->dbr_sw_stats->dbq_int_recv);
		if (!_is_chip_p7(rdev->chip_ctx))
			seq_printf(s, "\tdbq_int_en: %llu\n", rdev->dbr_sw_stats->dbq_int_en);
		seq_printf(s, "\tdbq_pacing_resched: %llu\n",
			   rdev->dbr_sw_stats->dbq_pacing_resched);
		seq_printf(s, "\tdbq_pacing_complete: %llu\n",
			   rdev->dbr_sw_stats->dbq_pacing_complete);
		seq_printf(s, "\tdbq_pacing_alerts: %llu\n",
			   rdev->dbr_sw_stats->dbq_pacing_alerts);
		seq_printf(s, "\tdbq_dbr_fifo_reg: 0x%x\n",
			   readl(rdev->en_dev->bar0 + rdev->dbr_db_fifo_reg_off));
	}

	if (rdev->dbr_drop_recov) {
		seq_printf(s, "\tdbr_drop_recov_epoch: %d\n",
			   rdev->dbr_evt_curr_epoch);
		seq_printf(s, "\tdbr_drop_recov_events: %lld\n",
			   rdev->dbr_sw_stats->dbr_drop_recov_events);
		seq_printf(s, "\tdbr_drop_recov_timeouts: %lld\n",
			   rdev->dbr_sw_stats->dbr_drop_recov_timeouts);
		seq_printf(s, "\tdbr_drop_recov_timeout_users: %lld\n",
			   rdev->dbr_sw_stats->dbr_drop_recov_timeout_users);
		seq_printf(s, "\tdbr_drop_recov_event_skips: %lld\n",
			   rdev->dbr_sw_stats->dbr_drop_recov_event_skips);
	}

	if (BNXT_RE_PPP_ENABLED(rdev->chip_ctx)) {
		seq_printf(s, "\tppp_enabled_contexts: %d\n",
			   rdev->ppp_stats.ppp_enabled_ctxs);
		seq_printf(s, "\tppp_enabled_qps: %d\n",
			   rdev->ppp_stats.ppp_enabled_qps);
	}

	for (i = 0; i < RCFW_MAX_LATENCY_SEC_SLAB_INDEX; i++) {
		if (rdev->rcfw.rcfw_lat_slab_sec[i])
			seq_printf(s, "\tlatency_slab [%d - %d] sec = %d\n",
				i, i + 1, rdev->rcfw.rcfw_lat_slab_sec[i]);
	}

	seq_printf(s, "\n");
err:
	return rc;
}

static int bnxt_re_perf_debugfs_show(struct seq_file *s, void *unused)
{
	u64 qp_create_total_msec = 0, qp_destroy_total_msec = 0;
	u64 mr_create_total_msec = 0, mr_destroy_total_msec = 0;
	int qp_create_total = 0, qp_destroy_total = 0;
	int mr_create_total = 0, mr_destroy_total = 0;
	u64 qp_modify_err_total_msec = 0;
	int qp_modify_err_total = 0;
	struct bnxt_re_dev *rdev;
	bool add_entry = false;
	int i;

	rdev = s->private;
	seq_printf(s, "bnxt_re perf stats: %s shadow qd %d Driver Version - %s\n",
		   rdev->rcfw.sp_perf_stats_enabled ? "Enabled" : "Disabled",
		   rdev->rcfw.curr_shadow_qd,
		   ROCE_DRV_MODULE_VERSION);

	if (!rdev->rcfw.sp_perf_stats_enabled)
		return -ENOMEM;

	for (i = 0; i < RCFW_MAX_LATENCY_MSEC_SLAB_INDEX; i++) {
		if (rdev->rcfw.rcfw_lat_slab_msec[i])
			seq_printf(s, "\tlatency_slab [%d - %d] msec = %d\n",
				i, i + 1, rdev->rcfw.rcfw_lat_slab_msec[i]);
	}

	if (!bnxt_re_is_rdev_valid(rdev))
		return -ENODEV;

	for (i = 0; i < RCFW_MAX_STAT_INDEX; i++) {
		if (rdev->rcfw.qp_create_stats[i] > 0) {
			qp_create_total++;
			qp_create_total_msec += rdev->rcfw.qp_create_stats[i];
			add_entry = true;
		}
		if (rdev->rcfw.qp_destroy_stats[i] > 0) {
			qp_destroy_total++;
			qp_destroy_total_msec += rdev->rcfw.qp_destroy_stats[i];
			add_entry = true;
		}
		if (rdev->rcfw.mr_create_stats[i] > 0) {
			mr_create_total++;
			mr_create_total_msec += rdev->rcfw.mr_create_stats[i];
			add_entry = true;
		}
		if (rdev->rcfw.mr_destroy_stats[i] > 0) {
			mr_destroy_total++;
			mr_destroy_total_msec += rdev->rcfw.mr_destroy_stats[i];
			add_entry = true;
		}
		if (rdev->rcfw.qp_modify_stats[i] > 0) {
			qp_modify_err_total++;
			qp_modify_err_total_msec += rdev->rcfw.qp_modify_stats[i];
			add_entry = true;
		}

		if (add_entry)
			seq_printf(s, "<qp_create> %lld <qp_destroy> %lld <mr_create> %lld "
				      "<mr_destroy> %lld <qp_modify_to_err> %lld\n",
				      rdev->rcfw.qp_create_stats[i],
				      rdev->rcfw.qp_destroy_stats[i],
				      rdev->rcfw.mr_create_stats[i],
				      rdev->rcfw.mr_destroy_stats[i],
				      rdev->rcfw.qp_modify_stats[i]);

		add_entry = false;
	}

	seq_printf(s, "Total qp_create %d in msec %lld\n",
		   qp_create_total, qp_create_total_msec);
	seq_printf(s, "Total qp_destroy %d in msec %lld\n",
		   qp_destroy_total, qp_destroy_total_msec);
	seq_printf(s, "Total mr_create %d in msec %lld\n",
		   mr_create_total, mr_create_total_msec);
	seq_printf(s, "Total mr_destroy %d in msec %lld\n",
		   mr_destroy_total, mr_destroy_total_msec);
	seq_printf(s, "Total qp_modify_err_total %d in msec %lld\n",
		   qp_modify_err_total, qp_modify_err_total_msec);
	seq_puts(s, "\n");

	return 0;
}

static int bnxt_re_drv_stats_debugfs_show(struct seq_file *s, void *unused)
{
	struct bnxt_re_dev *rdev = s->private;
	int rc = 0;

	seq_puts(s, "bnxt_re debug stats:\n");


	seq_printf(s, "=====[ IBDEV %s ]=============================\n",
		   rdev->ibdev.name);
	if (rdev->dbr_pacing) {
		seq_printf(s, "\tdbq_fifo_occup_slab_1: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_slab_1);
		seq_printf(s, "\tdbq_fifo_occup_slab_2: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_slab_2);
		seq_printf(s, "\tdbq_fifo_occup_slab_3: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_slab_3);
		seq_printf(s, "\tdbq_fifo_occup_slab_4: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_slab_4);
		seq_printf(s, "\tdbq_fifo_occup_water_mark: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_water_mark);
		seq_printf(s, "\tdbq_do_pacing_slab_1: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_1);
		seq_printf(s, "\tdbq_do_pacing_slab_2: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_2);
		seq_printf(s, "\tdbq_do_pacing_slab_3: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_3);
		seq_printf(s, "\tdbq_do_pacing_slab_4: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_4);
		seq_printf(s, "\tdbq_do_pacing_slab_5: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_5);
		seq_printf(s, "\tdbq_do_pacing_water_mark: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_water_mark);
		seq_printf(s, "\tdbq_do_pacing_retry: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_retry);
		seq_printf(s, "\tmad_consumed: %llu\n",
			   rdev->dbg_stats->mad.mad_consumed);
		seq_printf(s, "\tmad_processed: %llu\n",
			   rdev->dbg_stats->mad.mad_processed);
	}
	seq_printf(s, "\tReq retransmission: %s\n",
		   BNXT_RE_HW_REQ_RETX(rdev->dev_attr->dev_cap_flags) ?
		   "Hardware" : "Firmware");
	seq_printf(s, "\tResp retransmission: %s\n",
		   BNXT_RE_HW_RESP_RETX(rdev->dev_attr->dev_cap_flags) ?
		   "Hardware" : "Firmware");
	/* show wqe mode */
	seq_printf(s, "\tsq wqe mode: %d\n", rdev->chip_ctx->modes.wqe_mode);
	seq_puts(s, "\n");

	return rc;
}

static int bnxt_re_info_debugfs_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_info_debugfs_show, rdev);
}

static int bnxt_re_perf_debugfs_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_perf_debugfs_show, rdev);
}

static int bnxt_re_drv_stats_debugfs_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_drv_stats_debugfs_show, rdev);
}

static int bnxt_re_debugfs_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations bnxt_re_info_dbg_ops = {
	.owner		= THIS_MODULE,
	.open		= bnxt_re_info_debugfs_open,
	.read		= seq_read,
	.write		= bnxt_re_info_debugfs_clear,
	.llseek		= seq_lseek,
	.release	= bnxt_re_debugfs_release,
};

static const struct file_operations bnxt_re_perf_dbg_ops = {
	.owner		= THIS_MODULE,
	.open		= bnxt_re_perf_debugfs_open,
	.read		= seq_read,
	.write		= bnxt_re_perf_debugfs_clear,
	.llseek		= seq_lseek,
	.release	= bnxt_re_debugfs_release,
};

static const struct file_operations bnxt_re_drv_stats_dbg_ops = {
	.owner		= THIS_MODULE,
	.open		= bnxt_re_drv_stats_debugfs_open,
	.read		= seq_read,
	.write		= bnxt_re_drv_stats_debugfs_clear,
	.llseek		= seq_lseek,
	.release	= bnxt_re_debugfs_release,
};

void bnxt_re_add_dbg_files(struct bnxt_re_dev *rdev)
{
	rdev->pdev_qpinfo_dir = debugfs_create_dir("qp_info",
						   rdev->pdev_debug_dir);
}

static ssize_t bnxt_re_hdbr_dfs_read(struct file *filp, char __user *buffer,
				     size_t usr_buf_len, loff_t *ppos)
{
	struct bnxt_re_hdbr_dbgfs_file_data *data = filp->private_data;
	size_t len;
	char *buf;

	if (*ppos)
		return 0;
	if (!data)
		return -ENODEV;

	buf = bnxt_re_hdbr_dump(data->rdev, data->group, data->user);
	if (!buf)
		return -ENOMEM;
	len = strlen(buf);
	if (usr_buf_len < len) {
		kfree(buf);
		return -ENOSPC;
	}
	len = simple_read_from_buffer(buffer, usr_buf_len, ppos, buf, len);
	kfree(buf);
	return len;
}

static const struct file_operations bnxt_re_hdbr_dfs_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_hdbr_dfs_read,
};

#define HDBR_DEBUGFS_SUB_TYPES 2
static void bnxt_re_add_hdbr_knobs(struct bnxt_re_dev *rdev)
{
	char *dirs[HDBR_DEBUGFS_SUB_TYPES] = {"driver", "apps"};
	char *names[DBC_GROUP_MAX] = {"sq", "rq", "srq", "cq"};
	struct bnxt_re_hdbr_dfs_data *data = rdev->hdbr_dbgfs;
	struct dentry *sub_dir, *f;
	int i, j;

	if (!rdev->hdbr_enabled)
		return;

	if (data)
		return;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return;
	data->hdbr_dir = debugfs_create_dir("hdbr", rdev->pdev_debug_dir);
	if (IS_ERR_OR_NULL(data->hdbr_dir)) {
		dev_dbg(rdev_to_dev(rdev), "Unable to create debugfs hdbr");
		kfree(data);
		return;
	}
	rdev->hdbr_dbgfs = data;
	for (i = 0; i < HDBR_DEBUGFS_SUB_TYPES; i++) {
		sub_dir = debugfs_create_dir(dirs[i], data->hdbr_dir);
		if (IS_ERR_OR_NULL(sub_dir)) {
			dev_dbg(rdev_to_dev(rdev), "Unable to create debugfs %s", dirs[i]);
			return;
		}
		for (j = 0; j < DBC_GROUP_MAX; j++) {
			data->file_data[i][j].rdev = rdev;
			data->file_data[i][j].group = j;
			data->file_data[i][j].user = !!i;
			f = debugfs_create_file(names[j], 0600, sub_dir, &data->file_data[i][j],
						&bnxt_re_hdbr_dfs_ops);
			if (IS_ERR_OR_NULL(f)) {
				dev_dbg(rdev_to_dev(rdev), "Unable to create hdbr debugfs file");
				return;
			}
		}
	}
}

static void bnxt_re_rem_hdbr_knobs(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_hdbr_dfs_data *data = rdev->hdbr_dbgfs;

	if (!data)
		return;
	debugfs_remove_recursive(data->hdbr_dir);
	kfree(data);
	rdev->hdbr_dbgfs = NULL;
}

void bnxt_re_rename_debugfs_entry(struct bnxt_re_dev *rdev)
{
	struct dentry *port_debug_dir;

	if (!test_bit(BNXT_RE_FLAG_PER_PORT_DEBUG_INFO, &rdev->flags)) {
		strncpy(rdev->dev_name, dev_name(&rdev->ibdev.dev), IB_DEVICE_NAME_MAX);
		bnxt_re_debugfs_add_port(rdev, rdev->dev_name);
		set_bit(BNXT_RE_FLAG_PER_PORT_DEBUG_INFO, &rdev->flags);
		dev_info(rdev_to_dev(rdev), "Device %s registered successfully",
			 rdev->dev_name);
	} else if (strncmp(rdev->dev_name, dev_name(&rdev->ibdev.dev), IB_DEVICE_NAME_MAX)) {
		if (IS_ERR_OR_NULL(rdev->port_debug_dir))
			return;
		strncpy(rdev->dev_name, dev_name(&rdev->ibdev.dev), IB_DEVICE_NAME_MAX);
		port_debug_dir = debugfs_rename(bnxt_re_debugfs_root,
						rdev->port_debug_dir,
						bnxt_re_debugfs_root,
						rdev->dev_name);
		if (IS_ERR(port_debug_dir)) {
			dev_warn(rdev_to_dev(rdev), "Unable to rename debugfs %s",
				 rdev->dev_name);
			return;
		}
		rdev->port_debug_dir = port_debug_dir;
		dev_info(rdev_to_dev(rdev), "Device renamed to %s successfully",
			 rdev->dev_name);
	}
}

void bnxt_re_debugfs_add_pdev(struct bnxt_re_dev *rdev)
{
	const char *pdev_name;

	pdev_name = pci_name(rdev->en_dev->pdev);
	rdev->pdev_debug_dir = debugfs_create_dir(pdev_name,
						  bnxt_re_debugfs_root);
	if (IS_ERR_OR_NULL(rdev->pdev_debug_dir)) {
		dev_dbg(rdev_to_dev(rdev), "Unable to create debugfs %s",
			pdev_name);
		return;
	}
	rdev->en_qp_dbg = 1;
	bnxt_re_add_dbg_files(rdev);
	bnxt_re_add_hdbr_knobs(rdev);
}

void bnxt_re_debugfs_rem_pdev(struct bnxt_re_dev *rdev)
{
	bnxt_re_rem_hdbr_knobs(rdev);
	debugfs_remove_recursive(rdev->pdev_debug_dir);
	rdev->pdev_debug_dir = NULL;
}

void bnxt_re_debugfs_add_port(struct bnxt_re_dev *rdev, char *dev_name)
{
	if (!rdev->en_dev)
		return;

	rdev->port_debug_dir = debugfs_create_dir(dev_name,
						  bnxt_re_debugfs_root);
	rdev->info = debugfs_create_file("info", 0400,
					 rdev->port_debug_dir, rdev,
					 &bnxt_re_info_dbg_ops);
	rdev->sp_perf_stats = debugfs_create_file("sp_perf_stats", 0644,
						  rdev->port_debug_dir, rdev,
						  &bnxt_re_perf_dbg_ops);
	rdev->drv_dbg_stats = debugfs_create_file("drv_dbg_stats", 0644,
						  rdev->port_debug_dir, rdev,
						  &bnxt_re_drv_stats_dbg_ops);
}

void bnxt_re_rem_dbg_files(struct bnxt_re_dev *rdev)
{
	debugfs_remove_recursive(rdev->pdev_qpinfo_dir);
	rdev->pdev_qpinfo_dir = NULL;
}

void bnxt_re_debugfs_rem_port(struct bnxt_re_dev *rdev)
{
	debugfs_remove_recursive(rdev->port_debug_dir);
	rdev->port_debug_dir = NULL;
	rdev->info = NULL;
}

void bnxt_re_debugfs_remove(void)
{
	debugfs_remove_recursive(bnxt_re_debugfs_root);
	bnxt_re_debugfs_root = NULL;
}

void bnxt_re_debugfs_init(void)
{
	bnxt_re_debugfs_root = debugfs_create_dir(ROCE_DRV_MODULE_NAME, NULL);
	if (IS_ERR_OR_NULL(bnxt_re_debugfs_root)) {
		dev_dbg(NULL, "%s: Unable to create debugfs root directory ",
			ROCE_DRV_MODULE_NAME);
		dev_dbg(NULL, "with err 0x%lx", PTR_ERR(bnxt_re_debugfs_root));
		return;
	}
}
#endif
