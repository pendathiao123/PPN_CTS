from flask import Flask, render_template
import os

app = Flask(__name__)

# Configuration pour trouver le template
app.config['TEMPLATE_FOLDER'] = os.path.dirname(os.path.abspath(__file__))
app.template_folder = app.config['TEMPLATE_FOLDER']

# Configuration fictive pour ASSETS_ROOT
app.config['ASSETS_ROOT'] = '/static'

@app.route('/')
def index():
    return render_template('index.html')

if __name__ == '__main__':
    app.run(debug=True)