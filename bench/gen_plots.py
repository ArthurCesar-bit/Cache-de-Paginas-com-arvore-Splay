#!/usr/bin/env python3
# ===========================================================
# gen_plots.py — gera os graficos do relatorio a partir dos CSVs
# produzidos por build/bench (rode antes: ./build/bench).
#
#   build/bench_compare.csv  -> hit ratio e profundidade media
#   build/bench_threads.csv  -> vazao x numero de threads
#
# Saidas PNG em build/ (ignoradas pelo Git).
# ===========================================================
import csv
import os
import sys

try:
    import matplotlib
    matplotlib.use("Agg")  # backend sem display (serve em servidor/CI)
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib nao instalado: pip install matplotlib")

BUILD = "build"
CMP = os.path.join(BUILD, "bench_compare.csv")
THR = os.path.join(BUILD, "bench_threads.csv")


def read_csv(path):
    if not os.path.exists(path):
        sys.exit(f"nao encontrei {path} — rode ./build/bench (ou 'make bench') antes")
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def plot_compare(rows):
    workloads = ["uniform", "zipfian"]

    def cell(metric, wl, pol):
        for r in rows:
            if r["workload"] == wl and r["policy"] == pol:
                return float(r[metric])
        return 0.0

    x = range(len(workloads))
    width = 0.35

    # --- hit ratio ---
    fig, ax = plt.subplots()
    ax.bar([i - width / 2 for i in x], [cell("hit_ratio", w, "splay") for w in workloads],
           width, label="splay")
    ax.bar([i + width / 2 for i in x], [cell("hit_ratio", w, "lru") for w in workloads],
           width, label="lru")
    ax.set_xticks(list(x))
    ax.set_xticklabels(workloads)
    ax.set_ylabel("hit ratio")
    ax.set_title("Hit ratio: splay vs LRU")
    ax.legend()
    out = os.path.join(BUILD, "plot_hit_ratio.png")
    fig.savefig(out, dpi=120, bbox_inches="tight")
    print("escrito:", out)

    # --- profundidade media (so faz sentido na splay; LRU nao tem arvore) ---
    depths = [cell("avg_depth", w, "splay") for w in workloads]
    fig, ax = plt.subplots()
    ax.bar(list(x), depths, width=0.6, color=["tab:blue", "tab:orange"])
    ax.set_xticks(list(x))
    ax.set_xticklabels(workloads)
    ax.set_ylabel("profundidade media de acesso")
    ax.set_title("Profundidade media de acesso na splay")
    out = os.path.join(BUILD, "plot_avg_depth.png")
    fig.savefig(out, dpi=120, bbox_inches="tight")
    print("escrito:", out)


def plot_threads(rows):
    fig, ax = plt.subplots()
    for pol in ["splay", "lru"]:
        pts = [(int(r["threads"]), float(r["ops_per_sec"]))
               for r in rows if r["policy"] == pol]
        pts.sort()
        if pts:
            ax.plot([p[0] for p in pts], [p[1] for p in pts],
                    marker="o", label=pol)
    ax.set_xlabel("threads")
    ax.set_ylabel("ops/s")
    ax.set_title("Escalabilidade (carga zipfiana)")
    ax.legend()
    out = os.path.join(BUILD, "plot_throughput.png")
    fig.savefig(out, dpi=120, bbox_inches="tight")
    print("escrito:", out)


def main():
    plot_compare(read_csv(CMP))
    plot_threads(read_csv(THR))
    print("ok — graficos em build/*.png")


if __name__ == "__main__":
    main()
