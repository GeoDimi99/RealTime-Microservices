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
    metrics_cols = ['em_tw_ms', 'task_ms', 'tw_em_ms', 'total_ms']

    # Dimensione uniforme per PDF e PNG
    PAGE_SIZE = (12, 8)

    with PdfPages(output_pdf) as pdf:
        
        # --- 1. GANTT CHARTS PER ITERATION ---
        iterations = sorted(df['iteration'].unique())
        for iter_id in iterations:
            iter_df = df[df['iteration'] == iter_id]
            
            fig, ax = plt.subplots(figsize=PAGE_SIZE)
            offset = iter_df['start_req'].min()
            
            for i, row in iter_df.iterrows():
                ax.barh(str(row['task_id']), row['end_req'] - row['start_req'], 
                        left=row['start_req'] - offset, color='skyblue', 
                        label='EM -> TW' if i == iter_df.index[0] else "")
                ax.barh(str(row['task_id']), row['start_res'] - row['end_req'], 
                        left=row['end_req'] - offset, color='orange', 
                        label='Task Time' if i == iter_df.index[0] else "")
                ax.barh(str(row['task_id']), row['end_res'] - row['start_res'], 
                        left=row['start_res'] - offset, color='lightgreen', 
                        label='TW -> EM' if i == iter_df.index[0] else "")

            ax.set_xlabel('Relative Time (Original Units)')
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