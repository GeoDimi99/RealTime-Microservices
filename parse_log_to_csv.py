import sys
import re
import csv
import statistics
import os
from collections import defaultdict

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import numpy as np


def parse_log(input_file):
    """Parse the log file and extract metrics + raw timestamps."""
    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: File '{input_file}' not found.")
        sys.exit(1)

    metrics = []

    # Regular expressions
    iter_re = re.compile(r'ITERATION\s+(\d+)\s+/')
    task_re = re.compile(r'║\s+Task\s+(\d+):\s+([a-zA-Z0-9_\-]+)')
    req_lat_re = re.compile(r'Request Latency\s*=\s*([0-9.]+)\s*ms')
    task_exec_re = re.compile(r'Task Execution\s*=\s*([0-9.]+)\s*ms')
    resp_lat_re = re.compile(r'Response Latency\s*=\s*([0-9.]+)\s*ms')
    total_re = re.compile(r'Total End-to-End\s*=\s*([0-9.]+)\s*ms')
    net_over_re = re.compile(r'Network Overhead\s*=\s*([0-9.]+)\s*ms')

    # Raw timestamps T1-T4
    t1_re = re.compile(r'T1\s*\(start_request\)\s*=\s*([0-9.]+)\s*ms')
    t2_re = re.compile(r'T2\s*\(end_request\)\s*=\s*([0-9.]+)\s*ms')
    t3_re = re.compile(r'T3\s*\(start_result\)\s*=\s*([0-9.]+)\s*ms')
    t4_re = re.compile(r'T4\s*\(end_result\)\s*=\s*([0-9.]+)\s*ms')

    current_iteration = 1
    current_task_id = None
    current_task_name = "Unknown"
    current_metrics = {}
    current_timestamps = {}

    for line in lines:
        iter_match = iter_re.search(line)
        if iter_match:
            current_iteration = int(iter_match.group(1))
            continue

        task_match = task_re.search(line)
        if task_match:
            current_task_id = int(task_match.group(1))
            current_task_name = task_match.group(2)
            continue

        # Raw timestamps
        t1_match = t1_re.search(line)
        if t1_match:
            current_timestamps['T1'] = float(t1_match.group(1))
            continue

        t2_match = t2_re.search(line)
        if t2_match:
            current_timestamps['T2'] = float(t2_match.group(1))
            continue

        t3_match = t3_re.search(line)
        if t3_match:
            current_timestamps['T3'] = float(t3_match.group(1))
            continue

        t4_match = t4_re.search(line)
        if t4_match:
            current_timestamps['T4'] = float(t4_match.group(1))
            continue

        m1 = req_lat_re.search(line)
        if m1:
            current_metrics['Request Latency'] = float(m1.group(1))
            continue

        m2 = task_exec_re.search(line)
        if m2:
            current_metrics['Task Execution'] = float(m2.group(1))
            continue

        m3 = resp_lat_re.search(line)
        if m3:
            current_metrics['Response Latency'] = float(m3.group(1))
            continue

        m4 = total_re.search(line)
        if m4:
            current_metrics['Total End-to-End'] = float(m4.group(1))
            continue

        m5 = net_over_re.search(line)
        if m5:
            current_metrics['Network Overhead'] = float(m5.group(1))

            # When we have all 5 metrics, save the block
            if len(current_metrics) == 5:
                entry = {
                    'Iteration': current_iteration,
                    'Task ID': current_task_id,
                    'Task Name': current_task_name,
                    'Request Latency': current_metrics['Request Latency'],
                    'Task Execution': current_metrics['Task Execution'],
                    'Response Latency': current_metrics['Response Latency'],
                    'Total End-to-End': current_metrics['Total End-to-End'],
                    'Network Overhead': current_metrics['Network Overhead']
                }
                # Attach raw timestamps if available
                if len(current_timestamps) == 4:
                    entry['T1'] = current_timestamps['T1']
                    entry['T2'] = current_timestamps['T2']
                    entry['T3'] = current_timestamps['T3']
                    entry['T4'] = current_timestamps['T4']
                metrics.append(entry)
                current_metrics = {}
                current_timestamps = {}

    return metrics


def write_csv(metrics, output_file):
    """Write parsed metrics to CSV with averages (original behavior)."""
    if not metrics:
        print("Nessuna metrica trovata nel file di log.")
        return

    tasks_data = defaultdict(list)
    for m in metrics:
        tasks_data[m['Task Name']].append(m)

    with open(output_file, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)

        writer.writerow([
            'Iteration', 'Task',
            'Request Latency (ms)', 'Task Execution (ms)',
            'Response Latency (ms)', 'Total End-to-End (ms)',
            'Network Overhead (ms)'
        ])

        for m in metrics:
            t_name = f"Task {m['Task ID']}: {m['Task Name']}" if m['Task ID'] is not None else m['Task Name']
            writer.writerow([
                m['Iteration'], t_name,
                m['Request Latency'], m['Task Execution'],
                m['Response Latency'], m['Total End-to-End'],
                m['Network Overhead']
            ])

        writer.writerow([])
        writer.writerow(['---', 'AVERAGES (excl. iter 1)', '---', '---', '---', '---', '---'])

        for task_name, task_metrics in tasks_data.items():
            t_id = task_metrics[0]['Task ID']
            t_label = f"Task {t_id}: {task_name}" if t_id is not None else task_name

            valid_metrics = [m for m in task_metrics if m['Iteration'] > 1]
            if not valid_metrics:
                valid_metrics = task_metrics

            avg_req_lat = statistics.mean([m['Request Latency'] for m in valid_metrics])
            avg_task_exec = statistics.mean([m['Task Execution'] for m in valid_metrics])
            avg_resp_lat = statistics.mean([m['Response Latency'] for m in valid_metrics])
            avg_total = statistics.mean([m['Total End-to-End'] for m in valid_metrics])
            avg_net_over = statistics.mean([m['Network Overhead'] for m in valid_metrics])

            writer.writerow([
                'AVERAGE', t_label,
                f"{avg_req_lat:.3f}", f"{avg_task_exec:.3f}",
                f"{avg_resp_lat:.3f}", f"{avg_total:.3f}",
                f"{avg_net_over:.3f}"
            ])

    print(f"CSV scritto con successo: {output_file}")
    print(f"Totale misurazioni: {len(metrics)}")


def generate_pdf_report(metrics, pdf_file, csv_name):
    """Generate a PDF report with timeline charts and summary tables."""
    if not metrics:
        print("Nessuna metrica trovata, report PDF non generato.")
        return

    has_timestamps = 'T1' in metrics[0]

    # Organize by iteration
    iterations = defaultdict(list)
    for m in metrics:
        iterations[m['Iteration']].append(m)

    # Organize by task ID
    tasks_data = defaultdict(list)
    for m in metrics:
        tasks_data[m['Task ID']].append(m)

    with PdfPages(pdf_file) as pdf:

        # =============================================
        # 1) TIMELINE CHARTS (one per iteration)
        # =============================================
        if has_timestamps:
            for iter_num in sorted(iterations.keys()):
                iter_tasks = iterations[iter_num]
                # Sort by Task ID
                iter_tasks.sort(key=lambda x: x['Task ID'])

                fig, ax = plt.subplots(figsize=(12, 8))

                # Find the minimum T1 in this iteration to compute relative times
                min_t1 = min(t['T1'] for t in iter_tasks)

                task_ids = [t['Task ID'] for t in iter_tasks]
                bar_height = 0.6

                for i, t in enumerate(iter_tasks):
                    # Relative timestamps (in ms, from the first T1 of this iteration)
                    rel_t1 = t['T1'] - min_t1
                    rel_t2 = t['T2'] - min_t1
                    rel_t3 = t['T3'] - min_t1
                    rel_t4 = t['T4'] - min_t1

                    # EM -> TW (Request Latency): T1 to T2
                    ax.barh(t['Task ID'], rel_t2 - rel_t1, left=rel_t1,
                            height=bar_height, color='#87CEEB', edgecolor='none',
                            label='EM -> TW' if i == 0 else '')

                    # Task Time (Task Execution): T2 to T3
                    ax.barh(t['Task ID'], rel_t3 - rel_t2, left=rel_t2,
                            height=bar_height, color='#FFA500', edgecolor='none',
                            label='Task Time' if i == 0 else '')

                    # TW -> EM (Response Latency): T3 to T4
                    ax.barh(t['Task ID'], rel_t4 - rel_t3, left=rel_t3,
                            height=bar_height, color='#90EE90', edgecolor='none',
                            label='TW -> EM' if i == 0 else '')

                ax.set_xlabel('Relative Time (ms)')
                ax.set_ylabel('Task ID')
                ax.set_title(f'Iteration {iter_num - 1}: Task Timeline')
                ax.set_yticks(task_ids)
                ax.set_yticklabels([str(tid) for tid in task_ids])
                ax.legend(loc='upper right')
                ax.grid(axis='x', linestyle='--', alpha=0.5)

                plt.tight_layout()
                pdf.savefig(fig)
                plt.close(fig)

        def _add_avg_per_task_page(pdf, data, title):
            td = defaultdict(list)
            for m in data:
                td[m['Task ID']].append(m)
            fig2, ax2 = plt.subplots(figsize=(12, 8))
            ax2.axis('off')
            ax2.set_title(title, fontsize=16, fontweight='bold', pad=20)
            col_labels2 = ['', 'em_tw_ms', 'task_ms', 'tw_em_ms', 'total_ms']
            tdata2 = []
            for task_id in sorted(td.keys()):
                tm = td[task_id]
                tdata2.append([
                    str(task_id),
                    f"{statistics.mean([m['Request Latency'] for m in tm]):.4f}",
                    f"{statistics.mean([m['Task Execution'] for m in tm]):.4f}",
                    f"{statistics.mean([m['Response Latency'] for m in tm]):.4f}",
                    f"{statistics.mean([m['Total End-to-End'] for m in tm]):.4f}",
                ])
            t2 = ax2.table(cellText=tdata2, colLabels=col_labels2, loc='center', cellLoc='center')
            t2.auto_set_font_size(False)
            t2.set_fontsize(12)
            t2.scale(1, 2.0)
            for j in range(len(col_labels2)):
                t2[0, j].set_facecolor('#E8E8E8')
                t2[0, j].set_text_props(fontweight='bold')
            plt.tight_layout()
            pdf.savefig(fig2)
            plt.close(fig2)

        def _add_global_summary_page(pdf, data, title, footer):
            fig3, ax3 = plt.subplots(figsize=(12, 8))
            ax3.axis('off')
            ax3.set_title(title, fontsize=16, fontweight='bold', pad=20)
            col_labels3 = ['', 'Mean', 'Std Dev', 'Min', 'Median', 'Max']
            metric_names3 = [
                ('em_tw_ms', 'Request Latency'),
                ('task_ms', 'Task Execution'),
                ('tw_em_ms', 'Response Latency'),
                ('total_ms', 'Total End-to-End'),
            ]
            tdata3 = []
            for display_name, key in metric_names3:
                values = [m[key] for m in data]
                tdata3.append([
                    display_name,
                    f"{statistics.mean(values):.4f}",
                    f"{(statistics.stdev(values) if len(values) > 1 else 0):.4f}",
                    f"{min(values):.3f}",
                    f"{statistics.median(values):.4f}",
                    f"{max(values):.3f}",
                ])
            t3 = ax3.table(cellText=tdata3, colLabels=col_labels3, loc='center', cellLoc='center')
            t3.auto_set_font_size(False)
            t3.set_fontsize(12)
            t3.scale(1, 2.0)
            for j in range(len(col_labels3)):
                t3[0, j].set_facecolor('#E8E8E8')
                t3[0, j].set_text_props(fontweight='bold')
            ax3.text(0.5, 0.08, footer, transform=ax3.transAxes, ha='center', fontsize=12)
            plt.tight_layout()
            pdf.savefig(fig3)
            plt.close(fig3)

        # =============================================
        # 2) AVERAGE PERFORMANCE PER TASK ID TABLE
        # =============================================
        _add_avg_per_task_page(pdf, metrics, 'Average Performance per Task ID (ms)')

        # =============================================
        # 3) GLOBAL PERFORMANCE SUMMARY TABLE
        # =============================================
        base_csv = os.path.basename(csv_name)
        _add_global_summary_page(
            pdf, metrics,
            'Global Performance Summary (ms)',
            f"Dataset: {base_csv} | Total Samples: {len(metrics)}"
        )

        # =============================================
        # 4) AVERAGE PER TASK (excl. iter 1)
        # =============================================
        metrics_excl1 = [m for m in metrics if m['Iteration'] > 1]
        if metrics_excl1:
            _add_avg_per_task_page(
                pdf, metrics_excl1,
                'Average Performance per Task ID — excl. iter 1 (ms)'
            )

            # =============================================
            # 5) GLOBAL SUMMARY (excl. iter 1)
            # =============================================
            _add_global_summary_page(
                pdf, metrics_excl1,
                'Global Performance Summary — excl. iter 1 (ms)',
                f"Dataset: {base_csv} | Samples (excl. iter 1): {len(metrics_excl1)}"
            )

    print(f"Report PDF generato con successo: {pdf_file}")


def main():
    if len(sys.argv) < 3:
        print("Uso: python parse_log_to_csv.py <file_input.log> <file_output.csv> [--no-pdf]")
        sys.exit(1)

    input_file = sys.argv[1]
    output_csv = sys.argv[2]
    generate_pdf = '--no-pdf' not in sys.argv

    # Parse log
    metrics = parse_log(input_file)

    if not metrics:
        print("Nessuna metrica trovata nel file di log.")
        return

    # Write CSV
    write_csv(metrics, output_csv)

    # Generate PDF report
    if generate_pdf:
        pdf_file = os.path.splitext(output_csv)[0] + '_full_report.pdf'
        generate_pdf_report(metrics, pdf_file, output_csv)


if __name__ == '__main__':
    main()