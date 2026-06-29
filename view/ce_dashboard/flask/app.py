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

# CE_DASHBOARD_METRICSD_URL is the URL of the MetricsD server to query for CE Dashboard data.
# Default to 'https://display.ospool.osg-htc.org'.
app.config['CE_DASHBOARD_METRICSD_URL'] = os.getenv('CE_DASHBOARD_METRICSD_URL', 'https://display.ospool.osg-htc.org')

# CE_DASHBOARD_FAVICON_URL is the URL of the favicon to use for all pages.
# Default to 'static/favicon.ico'.
app.config['CE_DASHBOARD_FAVICON_URL'] = os.getenv('CE_DASHBOARD_FAVICON_URL', 'static/favicon.png')

# CE_DASHBOARD_TITLE is the default title to use for all pages when no page-specific title is provided.
# Default to 'HTCondorView'.
app.config['CE_DASHBOARD_TITLE'] = os.getenv('CE_DASHBOARD_TITLE', 'HTCondorView')


# CE_DASHBOARD_NAVBAR_LOGO_URL is the URL of the logo image displayed in the navbar.
# Default to 'static/navbar_logo.svg'.
app.config['CE_DASHBOARD_NAVBAR_LOGO_URL'] = os.getenv('CE_DASHBOARD_NAVBAR_LOGO_URL', 'static/navbar_logo.svg')


################################################
# Initialize my utils and any other modules
################################################

utils.set_flask_app(app)

@app.context_processor
def inject_globals():
    return {
        'favicon_url': app.config['CE_DASHBOARD_FAVICON_URL'],
        'site_title': app.config['CE_DASHBOARD_TITLE'],
        'navbar_logo_url': app.config['CE_DASHBOARD_NAVBAR_LOGO_URL'],
    }

################################################
# Create routes in common for multiple blueprints
################################################

common_linkmap = {
    'Overview': 'overview.html',
    'Contributed': 'contributed.html',
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
    app.run()
