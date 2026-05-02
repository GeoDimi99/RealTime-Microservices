import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import sys
import os

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 comprehensive_performance_report.py <filename.csv>")
        sys.exit(1)

    input_file = sys.argv[1]
    if not os.path.exists(input_file):
        print(f"Error: File '{input_file}' not found.")
        sys.exit(1)

    df = pd.read_csv(input_file)
    output_pdf = input_file.replace('.csv', '_full_report.pdf')
    # CSV columns: iteration,task_id,scheduled_ns,activated_ns,exec_start_ns,result_ns,jitter_ms,dep_wait_ms,exec_ms,total_ms
    metrics_cols = ['jitter_ms', 'dep_wait_ms', 'exec_ms', 'total_ms']

    # Dimensione uniforme per PDF e PNG
    PAGE_SIZE = (12, 8)

    with PdfPages(output_pdf) as pdf:
        
        # --- 1. GANTT CHARTS PER ITERATION ---
        iterations = sorted(df['iteration'].unique())
        for iter_id in iterations:
            iter_df = df[df['iteration'] == iter_id]
            
            fig, ax = plt.subplots(figsize=PAGE_SIZE)
            offset_ns = iter_df['scheduled_ns'].min()

            for i, row in iter_df.iterrows():
                t_sched  = (row['scheduled_ns']  - offset_ns) / 1e6
                t_act    = (row['activated_ns']  - offset_ns) / 1e6
                t_exec   = (row['exec_start_ns'] - offset_ns) / 1e6
                t_result = (row['result_ns']      - offset_ns) / 1e6
                label = i == iter_df.index[0]
                ax.barh(str(row['task_id']), t_act    - t_sched,  left=t_sched,  color='grey',       alpha=0.6, label='Jitter'    if label else "")
                ax.barh(str(row['task_id']), t_exec   - t_act,    left=t_act,    color='orange',               label='Dep Wait'  if label else "")
                ax.barh(str(row['task_id']), t_result - t_exec,   left=t_exec,   color='lightgreen',           label='Execution' if label else "")

            ax.set_xlabel('Time (ms, relative to first scheduled start)')
            ax.set_ylabel('Task ID')
            ax.set_title(f'Iteration {iter_id}: Task Timeline', fontweight='bold')
            ax.legend(loc='upper right')
            ax.grid(axis='x', linestyle='--', alpha=0.5)
            
            plt.tight_layout()
            
            # Salva nel PDF
            pdf.savefig(fig)
            # Salva come PNG individuale
            fig.savefig(f'iteration_{iter_id}.png', dpi=300)
            
            plt.close()

        # --- 2. STATISTICS PER TASK ID ---
        task_stats = df.groupby('task_id')[metrics_cols].mean()
        
        fig, ax = plt.subplots(figsize=PAGE_SIZE)
        ax.axis('tight')
        ax.axis('off')
        plt.title('Average Performance per Task ID (ms)', pad=20, fontweight='bold')

        table = ax.table(cellText=task_stats.values.round(4), 
                         colLabels=metrics_cols, 
                         rowLabels=task_stats.index, 
                         cellLoc='center', loc='center')
        table.auto_set_font_size(False)
        table.set_fontsize(10)
        table.scale(1, 2.2)
        
        pdf.savefig(fig, bbox_inches='tight')
        fig.savefig('task_stats.png', dpi=300, bbox_inches='tight')
        plt.close()

        # --- 3. TOTAL STATISTICS ---
        total_stats = df[metrics_cols].describe().T[['mean', 'std', 'min', '50%', 'max']]
        total_stats.columns = ['Mean', 'Std Dev', 'Min', 'Median', 'Max']

        fig, ax = plt.subplots(figsize=PAGE_SIZE)
        ax.axis('off')
        plt.title('Global Performance Summary (ms)', pad=30, fontweight='bold')

        table_total = ax.table(cellText=total_stats.values.round(4), 
                               colLabels=total_stats.columns, 
                               rowLabels=total_stats.index, 
                               cellLoc='center', 
                               loc='center')

        table_total.auto_set_font_size(False)
        table_total.set_fontsize(11)
        table_total.scale(1, 3.5)
        
        plt.annotate(f'Dataset: {input_file} | Total Samples: {len(df)}', 
                     xy=(0.5, 0.1), xycoords='figure fraction', ha='center', fontsize=10)

        pdf.savefig(fig, bbox_inches='tight') 
        fig.savefig('global_stats.png', dpi=300, bbox_inches='tight')
        plt.close()

    print(f"Report generated: {output_pdf}")
    print("Individual PNG images have been saved in the current directory.")

if __name__ == "__main__":
    main()