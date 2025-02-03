from flask import Flask, render_template
import os
import pandas as pand

app = Flask(__name__)

# Configuration pour trouver le template
app.config['TEMPLATE_FOLDER'] = os.path.dirname(os.path.abspath(__file__))
app.template_folder = app.config['TEMPLATE_FOLDER']

# Configuration fictive pour ASSETS_ROOT
app.config['ASSETS_ROOT'] = '/static'

@app.route('/', methods=['GET', 'POST'])
def index():
    table = pand.read_csv('../data/btc_sec_values.csv', encoding= 'unicode_escape')
    return render_template('index.html', data=table.to_html())

if __name__ == '__main__':
    app.run(debug=True)