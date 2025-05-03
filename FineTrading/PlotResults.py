import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os

# --- Configuration ---
CSV_FILE_PATH = '/home/ark30/Files/PPN/FineTrading/results.csv'

# --- Options d'Amélioration de la Visibilité (Ajuste selon tes besoins) ---

# 1. Limites manuelles pour l'axe Y (en pourcentage)
#    Mets None pour laisser Seaborn/Matplotlib décider automatiquement
#    Exemple: (-50, 100) signifie afficher de -50% à +100%
PORTFOLIO_YLIM = None # ou par exemple (-30, 50)
BTC_YLIM = None       # ou par exemple (-20, 20)

# 2. Taille des graphiques
RELPLOT_HEIGHT = 4.5  # Hauteur de chaque facette dans relplot
RELPLOT_ASPECT = 1.3  # Ratio largeur/hauteur dans relplot
BOXPLOT_FIGSIZE = (13, 7) # Taille globale de la figure pour le boxplot (largeur, hauteur)

# 3. Type de graphique pour les plots par run ('line' ou 'scatter')
RUN_PLOT_KIND = 'line'
SCATTER_ALPHA = 0.6 # Transparence si RUN_PLOT_KIND = 'scatter'

# 4. Style Seaborn
sns.set_theme(style="whitegrid") # Autre style possible: "darkgrid", "whitegrid", "ticks"

# --- Lecture et Validation des Données ---
try:
    if not os.path.exists(CSV_FILE_PATH):
        raise FileNotFoundError(f"Le fichier spécifié n'a pas été trouvé : {CSV_FILE_PATH}")

    df = pd.read_csv(CSV_FILE_PATH)

    if df.empty:
        print(f"Le fichier CSV '{CSV_FILE_PATH}' est vide.")
        exit()

    required_columns = ['Run', 'Strategy', 'DataType', 'BTCChange%', 'PortfolioChange%']
    if not all(col in df.columns for col in required_columns):
        print(f"Erreur: Colonnes manquantes. Attendues: {required_columns}, Trouvées: {df.columns.tolist()}")
        exit()

    df.rename(columns={'BTCChange%': 'BTCChangePct', 'PortfolioChange%': 'PortfolioChangePct'}, inplace=True)
    df['BTCChangePct'] = pd.to_numeric(df['BTCChangePct'], errors='coerce')
    df['PortfolioChangePct'] = pd.to_numeric(df['PortfolioChangePct'], errors='coerce')
    df.dropna(subset=['BTCChangePct', 'PortfolioChangePct'], inplace=True)

    if df.empty:
        print("Aucune donnée valide après nettoyage.")
        exit()

    print(f"Données chargées avec succès depuis '{CSV_FILE_PATH}'.")

    # --- Graphique 1: Performance Finale du Portefeuille par Run ---
    print("\nGénération du graphique: Performance du Portefeuille par Run...")
    g1_args = {
        'data': df,
        'x': 'Run',
        'y': 'PortfolioChangePct',
        'hue': 'Strategy',
        'col': 'DataType',
        'kind': RUN_PLOT_KIND,
        'height': RELPLOT_HEIGHT,
        'aspect': RELPLOT_ASPECT,
        'legend': 'full',
        'facet_kws': {'sharey': False} # <<< === IMPORTANT: Axes Y indépendants ===
    }
    if RUN_PLOT_KIND == 'scatter':
        g1_args['alpha'] = SCATTER_ALPHA # Ajoute de la transparence pour scatter

    g1 = sns.relplot(**g1_args)

    g1.fig.suptitle('Performance Finale du Portefeuille (%) par Run et Type de Données', y=1.03)
    g1.set_axis_labels('Numéro du Run', 'Variation Portefeuille (%)')
    g1.set_titles("DataType: {col_name}")

    # Appliquer les limites Y manuelles SI elles sont définies
    if PORTFOLIO_YLIM:
        print(f"  -> Application des limites Y manuelles au graphique 1: {PORTFOLIO_YLIM}")
        g1.set(ylim=PORTFOLIO_YLIM)

    plt.tight_layout(rect=[0, 0.03, 1, 0.97])


    # --- Graphique 2: Variation du BTC par Run ---
    print("Génération du graphique: Variation du BTC par Run...")
    df_btc = df[['Run', 'DataType', 'BTCChangePct']].drop_duplicates().reset_index(drop=True)

    g2_args = {
        'data': df_btc,
        'x': 'Run',
        'y': 'BTCChangePct',
        'hue': 'DataType',
        'kind': RUN_PLOT_KIND,
        'height': RELPLOT_HEIGHT,
        'aspect': RELPLOT_ASPECT, # Ajusté aussi ici
        'legend': 'full',
        # 'facet_kws': {'sharey': False} # Moins utile ici car pas de 'col' ou 'row' par défaut, mais inoffensif
    }
    if RUN_PLOT_KIND == 'scatter':
        g2_args['alpha'] = SCATTER_ALPHA

    g2 = sns.relplot(**g2_args)

    g2.fig.suptitle('Variation du BTC (%) par Run et Type de Données', y=1.03)
    g2.set_axis_labels('Numéro du Run', 'Variation BTC (%)')

    if BTC_YLIM:
        print(f"  -> Application des limites Y manuelles au graphique 2: {BTC_YLIM}")
        g2.set(ylim=BTC_YLIM)

    plt.tight_layout(rect=[0, 0.03, 1, 0.97])


    # --- Graphique 3: Distribution des Performances Finales (Synthèse des Runs) ---
    print("Génération du graphique: Distribution des Performances...")
    plt.figure(figsize=BOXPLOT_FIGSIZE) # <<< === Taille de figure ajustable ===
    sns.boxplot(
        data=df,
        x='DataType',
        y='PortfolioChangePct',
        hue='Strategy'
    )
    plt.axhline(0, color='grey', linestyle='--', linewidth=0.8)
    plt.title('Distribution des Performances Finales du Portefeuille (%) sur les Runs')
    plt.xlabel('Type de Données')
    plt.ylabel('Variation Portefeuille (%)')

    # Appliquer les limites Y manuelles SI elles sont définies
    if PORTFOLIO_YLIM:
        print(f"  -> Application des limites Y manuelles au graphique 3: {PORTFOLIO_YLIM}")
        # Pour un axe matplotlib simple (pas FacetGrid), on utilise plt.ylim
        plt.ylim(PORTFOLIO_YLIM)

    plt.tight_layout()


    # --- Afficher les graphiques ---
    print("\nAffichage des graphiques...")
    plt.show()

except FileNotFoundError as e:
    print(f"Erreur: {e}")
except Exception as e:
    print(f"Une erreur inattendue est survenue: {e}")
    import traceback
    traceback.print_exc()