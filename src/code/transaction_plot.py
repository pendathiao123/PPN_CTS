import pandas as pd
import matplotlib.pyplot as plt

# Charger les résultats
data = pd.read_csv("transaction_results.csv", names=["Transactions", "TPS", "AvgTime", "Success", "Fail"])

# Créer une figure avec plusieurs sous-graphiques
fig, axes = plt.subplots(2, 2, figsize=(12, 12), gridspec_kw={'hspace': 0.4})

# Tracer TPS
axes[0, 0].plot(data["Transactions"], data["TPS"], marker="o", color="blue", label="TPS (Transactions per Second)")
axes[0, 0].set_xlabel("Nombre de transactions")
axes[0, 0].set_ylabel("TPS")
axes[0, 0].set_title("Performance du serveur (TPS)")
axes[0, 0].legend()
axes[0, 0].grid()

# Tracer le temps moyen par transaction
axes[0, 1].plot(data["Transactions"], data["AvgTime"], marker="o", color="green", label="Temps moyen (ms)")
axes[0, 1].set_xlabel("Nombre de transactions")
axes[0, 1].set_ylabel("Temps moyen par transaction (ms)")
axes[0, 1].set_title("Temps moyen par transaction")
axes[0, 1].legend()
axes[0, 1].grid()

# Tracer le nombre de transactions réussies
axes[1, 0].plot(data["Transactions"], data["Success"], marker="o", color="orange", label="Transactions réussies")
axes[1, 0].set_xlabel("Nombre de transactions")
axes[1, 0].set_ylabel("Transactions réussies")
axes[1, 0].set_title("Transactions réussies")
axes[1, 0].legend()
axes[1, 0].grid()

# Tracer le nombre de transactions échouées
axes[1, 1].plot(data["Transactions"], data["Fail"], marker="o", color="red", label="Transactions échouées")
axes[1, 1].set_xlabel("Nombre de transactions")
axes[1, 1].set_ylabel("Transactions échouées")
axes[1, 1].set_title("Transactions échouées")
axes[1, 1].legend()
axes[1, 1].grid()

# Ajuster l'espacement entre les sous-graphiques
plt.tight_layout(rect=[0, 0, 1, 0.95])

# Sauvegarder l'image combinée
plt.savefig("transaction_benchmark.png")

# Afficher l'image
plt.show()