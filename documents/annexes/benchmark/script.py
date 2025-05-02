import re
import matplotlib.pyplot as plt

with open("Benchmark.txt", "r", encoding="utf-8") as f:
    raw_data = f.read()

# Nettoyer les nombres avec des espaces (ex: 1 000 => 1000)
raw_data = re.sub(r'(\d)\s+(\d)', r'\1\2', raw_data)

# Connexions (clients)
conn_pattern = re.compile(
    r"Résultat du benchmark pour (\d+) clients\s+"
    r".*?Durée : ([\d.]+) s\s+"
    r"Connexions par seconde \(CPS\) : ([\d.]+)",
    re.DOTALL
)
conn_data = [(int(n), float(d), float(cps)) for n, d, cps in conn_pattern.findall(raw_data)]

# Transactions
tx_pattern = re.compile(
    r"Résultat du benchmark pour (\d+) transactions\s+"
    r".*?Durée totale : ([\d.]+) s\s+"
    r"Transactions par seconde \(TPS\) : ([\d.]+)",
    re.DOTALL
)
tx_data = [(int(n), float(d), float(tps)) for n, d, tps in tx_pattern.findall(raw_data)]

# Durée en fonction du nombre de connexions
if conn_data:
    conn_counts, conn_durations, cps_values = zip(*conn_data)
    plt.figure(figsize=(10, 5))
    plt.plot(conn_counts, conn_durations, marker='^', color='orange', label="Durée totale (connexions)")
    plt.xlabel("Nombre de connexions")
    plt.ylabel("Durée (s)")
    plt.title("Durée totale en fonction du nombre de connexions")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig("duree_connexions.png")
    plt.show()

    plt.figure(figsize=(10, 5))
    plt.plot(conn_counts, cps_values, marker='o', label="CPS")
    plt.xlabel("Nombre de connexions")
    plt.ylabel("Connexions par seconde (CPS)")
    plt.title("Performance par nombre de connexions")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig("cps_plot.png")
    plt.show()

# Durée en fonction du nombre de transactions
if tx_data:
    tx_counts, tx_durations, tps_values = zip(*tx_data)
    plt.figure(figsize=(10, 5))
    plt.plot(tx_counts, tx_durations, marker='v', color='purple', label="Durée totale (transactions)")
    plt.xlabel("Nombre de transactions")
    plt.ylabel("Durée (s)")
    plt.title("Durée totale en fonction du nombre de transactions")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig("duree_transactions.png")
    plt.show()

    plt.figure(figsize=(10, 5))
    plt.plot(tx_counts, tps_values, marker='s', color='green', label="TPS")
    plt.xlabel("Nombre de transactions")
    plt.ylabel("Transactions par seconde (TPS)")
    plt.title("Performance par nombre de transactions")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig("tps_plot.png")
    plt.show()
