from flask import Flask, render_template
from blueprints.landing import landing_bp
from blueprints.overview import overview_bp
import os
import utils

################################################
# Main application setup
################################################

# Create Flask app
app = Flask(__name__)

################################################
# Configuration parameters (all settable via environment variables)
################################################

# CE_DASHBOARD_CACHE_DIRECTORY is the directory where the cache files will be stored.
# This directory should be writable by the web server user. If it does not exist, it will be created.
# Default to '/tmp/ce_dashboard_cache'.
app.config['CE_DASHBOARD_CACHE_DIRECTORY'] = os.getenv('CE_DASHBOARD_CACHE_DIRECTORY', '/tmp/ce_dashboard_cache')  

# CE_DASHBOARD_SERVER_CACHE_MINUTES is the number of minutes to cache data on this server.
# Default to 20 minutes.
app.config['CE_DASHBOARD_SERVER_CACHE_MINUTES'] = int(os.getenv('CE_DASHBOARD_SERVER_CACHE_MINUTES', '20'))

# CE Dashboard browser cache minutes is the number of minutes to cache data in the browser.
# Default to 30 minutes.
app.config['CE_DASHBOARD_BROWSER_CACHE_MINUTES'] = int(os.getenv('CE_DASHBOARD_BROWSER_CACHE_MINUTES', '30'))


################################################
# Initialize my utils and any other modules
################################################

utils.set_flask_app(app)

################################################
# Create routes in common for multiple blueprints
################################################

common_linkmap = {
    'Help': {
        'Query Syntax': 'query_syntax.html',
        'Customization': 'customization.html'
    }
}

@app.route('/fullscreen.html')
def handle_fullscreen():
    return render_template('fullscreen.html', linkmap=common_linkmap)  

@app.route('/edit.html')
def handle_edit():
    return render_template('edit.html', linkmap=common_linkmap)

# Register blueprints
app.register_blueprint(landing_bp)
app.register_blueprint(overview_bp)

if __name__ == '__main__':
    app.run(debug=True)
