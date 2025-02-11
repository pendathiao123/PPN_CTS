from flask import Flask, render_template
import os
import pandas as pand
import json

app = Flask(__name__)

# Configuration pour trouver le template
app.config['TEMPLATE_FOLDER'] = os.path.dirname(os.path.abspath(__file__))
app.template_folder = app.config['TEMPLATE_FOLDER']

# Configuration fictive pour ASSETS_ROOT
app.config['ASSETS_ROOT'] = '/static'

@app.route('/', methods=['GET', 'POST'])
def index():
    table = pand.read_csv('../src/data/btc_sec_values.csv', encoding= 'unicode_escape')
    #transactions = pand.read_csv('../../log.csv', encoding= 'unicode_escape') ne marche pas encore, on doit extraire chaque type de données indépendemment

    # Combiner 'Day' et 'Second' pour créer la colonne 'Date' ainsi on aura 0 1 pour la seconde 1 a date du jour 0. 
    table['Date'] = table['Day'].astype(str) + ' ' + table['Second'].astype(str)

    dates = table['Date'].tolist()
    values = table['Value'].tolist()

    #on retourne le fichier html mais aussi les dates et valeurs pour plot le graph
    return render_template('index.html', dates=dates, values=values)

if __name__ == '__main__':
    app.run(debug=True)