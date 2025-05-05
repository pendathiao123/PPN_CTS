import pandas as pd
import matplotlib.pyplot as plt

# Charger les résultats
data = pd.read_csv("benchmark_results.csv", names=["Clients", "CPS", "AvgTime", "Success", "Fail"])

# Tracer CPS
plt.figure(figsize=(10, 6))
plt.plot(data["Clients"], data["CPS"], marker="o", label="CPS (Connections per Second)")
plt.xlabel("Nombre de clients")
plt.ylabel("CPS")
plt.title("Performance du serveur (CPS)")
plt.legend()
plt.grid()
plt.savefig("cps_plot.png")
plt.show()

# Tracer le temps moyen par connexion
plt.figure(figsize=(10, 6))
plt.plot(data["Clients"], data["AvgTime"], marker="o", label="Temps moyen (ms)")
plt.xlabel("Nombre de clients")
plt.ylabel("Temps moyen par connexion (ms)")
plt.title("Temps moyen par connexion")
plt.legend()
plt.grid()
plt.savefig("avg_time_plot.png")
plt.show()