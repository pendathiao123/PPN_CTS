import pandas as pd
import matplotlib.pyplot as plt

# Charger les résultats
data = pd.read_csv("benchmark_results.csv", names=["Clients", "CPS", "AvgTime", "Success", "Fail"])

# Ajouter 1 à la colonne "Clients" pour commencer à 1
data["Clients"] = data["Clients"] + 1

# Déterminer le CPS max pour tracer une ligne horizontale
cps_max = data["CPS"].max()

# Déterminer la meilleure latence (temps moyen le plus bas)
best_latency = data["AvgTime"].min()

# Créer une figure avec plusieurs sous-graphiques et ajuster l'espacement
fig, axes = plt.subplots(2, 2, figsize=(12, 12), gridspec_kw={'hspace': 0.4})  # Augmenter hspace pour plus d'espace vertical

# Tracer CPS
axes[0, 0].plot(data["Clients"], data["CPS"], marker="o", color="blue", label="CPS (Connections per Second)")
axes[0, 0].axhline(y=cps_max, color="red", linestyle="--", label=f"CPS Max ({cps_max:.2f})")  # Ligne horizontale
axes[0, 0].text(data["Clients"].min(), cps_max, f"{cps_max:.2f}", color="red", fontsize=10, ha="left", va="center")  # Ajouter le texte sur l'axe des ordonnées
axes[0, 0].set_xlabel("Nombre de clients")
axes[0, 0].set_ylabel("CPS")
axes[0, 0].set_title("Performance du serveur (CPS)")
axes[0, 0].legend()
axes[0, 0].grid()

# Tracer le temps moyen par connexion
axes[0, 1].plot(data["Clients"], data["AvgTime"], marker="o", color="green", label="Temps moyen (ms)")
axes[0, 1].axhline(y=best_latency, color="orange", linestyle="--", label=f"Meilleure latence ({best_latency:.2f} ms)")  # Ligne horizontale
axes[0, 1].text(data["Clients"].min(), best_latency, f"{best_latency:.2f} ms", color="orange", fontsize=10, ha="left", va="center")  # Ajouter le texte sur l'axe des ordonnées
axes[0, 1].set_xlabel("Nombre de clients")
axes[0, 1].set_ylabel("Temps moyen par connexion (ms)")
axes[0, 1].set_title("Temps moyen par connexion")
axes[0, 1].legend()
axes[0, 1].grid()

# Tracer le nombre de connexions réussies
axes[1, 0].plot(data["Clients"], data["Success"], marker="o", color="orange", label="Connexions réussies")
axes[1, 0].set_xlabel("Nombre de clients")
axes[1, 0].set_ylabel("Connexions réussies")
axes[1, 0].set_title("Connexions réussies")
axes[1, 0].legend()
axes[1, 0].grid()

# Tracer le nombre de connexions échouées
axes[1, 1].plot(data["Clients"], data["Fail"], marker="o", color="red", label="Connexions échouées")
axes[1, 1].set_xlabel("Nombre de clients")
axes[1, 1].set_ylabel("Connexions échouées")
axes[1, 1].set_title("Connexions échouées")
axes[1, 1].legend()
axes[1, 1].grid()

# Ajuster l'espacement entre les sous-graphiques
plt.tight_layout(rect=[0, 0, 1, 0.95])  # Ajuster l'espace global si nécessaire

# Sauvegarder l'image combinée
plt.savefig("benchmark_with_best_latency.png")

# Afficher l'image 
plt.show()