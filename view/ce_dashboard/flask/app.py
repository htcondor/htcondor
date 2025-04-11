from flask import Flask, render_template, request
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

# CE_DEFAULT_CE_DOMAIN is the default CE domain to use if the host parameter is not fully qualified.
app.config['CE_DASHBOARD_DEFAULT_CE_DOMAIN'] = os.getenv('CE_DASHBOARD_DEFAULT_CE_DOMAIN', 'svc.opensciencegrid.org')


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
    'Overview': 'overview.html',
    'Contributed': 'contributed.html',
}

@app.route('/fullscreen.html')
def handle_fullscreen():
    return render_template('fullscreen.html.j2', linkmap=common_linkmap)  

@app.route('/edit.html')
def handle_edit():
    return render_template('edit.html.j2', linkmap=common_linkmap)

@app.route('/allocated_graph_<resource>.html')
def handle_allocated_graph(resource):
    return render_template('allocated_graph.html.j2', linkmap=common_linkmap, resource=resource)

@app.route('/contributed.html')
def handle_contributed():
    time_range = request.args.get('r','week')
    if time_range not in ['hour','day','week','month','year']:
        time_range = 'week'
    return render_template('contributed.html.j2', host=request.args.get('host'), linkmap=common_linkmap, time_range=time_range)

# Register blueprints
app.register_blueprint(landing_bp)
app.register_blueprint(overview_bp)

if __name__ == '__main__':
    app.run(debug=True)
