from filelock import FileLock
import hashlib
import time
import os
from flask import request, make_response, Response
import datetime
from wsgiref.handlers import format_date_time

app = None  # Placeholder for the Flask app instance

def set_flask_app(flask_app):
    global app
    app = flask_app

# Implementation of method decorator on a flask response function
def cache_response_to_disk(seconds_to_cache: int = None, file_name: str = None):
    """
    A decorator to cache the response of a function to disk for a specified duration.
    Args:
        seconds_to_cache (int, optional): The number of seconds to cache the response. 
            If not provided, defaults to the value of `CE_DASHBOARD_SERVER_CACHE_MINUTES` 
            from the application configuration multiplied by 60.
        file_name (str, optional): The name of the cache file. If not provided, a unique 
            cache file name is generated based on the query string.
    Returns:
        function: A wrapper function that caches the response of the decorated function 
        to a file on disk.
    Notes:
        - The cache directory is determined by the `CE_DASHBOARD_CACHE_DIRECTORY` 
          configuration in the application.
        - If the cache file does not exist or is expired, the decorated function is 
          executed, and its response is cached.
        - If the cache file exists and is valid, the cached response is returned.
        - A file lock is used to ensure thread-safe access to the cache file.
        - If an error occurs while writing to the cache file (e.g., disk full), the 
          cache file is removed.
    Returns:
        tuple: A tuple containing:
            - response (str): The response from the decorated function or the cached response.
            - cached_response (bool): A flag indicating whether the response was retrieved 
              from the cache (True) or generated by the function (False).
    """
    def decorator(func):
        def wrapper(*args, **kwargs):
            CACHE_DIR = app.config['CE_DASHBOARD_CACHE_DIRECTORY']
            effective_seconds_to_cache = seconds_to_cache
            if effective_seconds_to_cache is None:
                effective_seconds_to_cache = app.config['CE_DASHBOARD_SERVER_CACHE_MINUTES'] * 60
            if file_name is not None:
                cache_file = os.path.join(CACHE_DIR, file_name)
            else:
                # Create a unique cache file name based on the query string
                query_string = request.query_string.decode('utf-8')
                query_hash = hashlib.md5(query_string.encode()).hexdigest()
                cache_file = os.path.join(CACHE_DIR, query_hash)

            cached_response = True

            # Create cache directory if it does not exist
            os.makedirs(os.path.dirname(cache_file), exist_ok=True)

            # if filename ends with '.bin', use binary mode
            open_mode = 'a+'
            if cache_file.endswith('.bin'):
                open_mode = 'ab+'

            lock = FileLock(cache_file + ".lock",is_singleton=False)
            with lock:
                with open(cache_file, open_mode) as f:
                    fsize = os.path.getsize(cache_file)
                    fmtime = os.path.getmtime(cache_file)
                    if (fsize == 0) or \
                       (fmtime < (time.time() - effective_seconds_to_cache)):
                        cached_response = False
                        response = func(*args, **kwargs)
                        try:
                            f.seek(0)
                            f.truncate()
                            f.write(response)
                        except Exception as e:
                            # Remove cache file if any errors writing it (i.e. disk full)
                            os.remove(cache_file)
                            print(f"ERROR writing cache file {cache_file}: {e}")
                    else:
                        f.seek(0)
                        response = f.read()
            return response, cached_response
        return wrapper
    return decorator


def make_data_response(response_body: str, cached_response: bool, browser_cache_minutes: int = None) -> Response:
    """
    Creates an HTTP response for with specified caching headers and metadata.

    Args:
        response_body (str): The body of the HTTP response.
        cached_response (bool): Indicates whether the response is from the server cache.
        browser_cache_minutes (int, optional): The number of minutes the browser should cache the response.

    Returns:
        Response: A Flask Response object with the specified headers and content.

    Notes:
        - The 'Content-Type' header is set to "text/plain; charset=UTF-8".
        - The 'Expires' header specifies when the cached response should expire.
        - The 'Cache-Control' header defines the maximum age for browser caching.
        - A custom header 'X-HTCondorView-Cached-Response' indicates if the response is cached.
    """
    if (browser_cache_minutes is None):
        browser_cache_minutes = app.config['CE_DASHBOARD_BROWSER_CACHE_MINUTES']
    response = make_response(response_body)
    response.headers['Content-Type'] = "text/plain; charset=UTF-8"
    # Tell the browser to cache this data for a specified number of minutes
    browser_cache_seconds = browser_cache_minutes * 60
    now = datetime.datetime.now()
    expires = now + datetime.timedelta(minutes=browser_cache_minutes)
    expires_str = format_date_time(time.mktime(expires.timetuple()))
    response.headers['Expires'] = f"{expires_str}"
    response.headers['Cache-Control'] =  f"max-age={browser_cache_seconds}, public"
    # In a custom HTTP header, state if the response is from the cache or not
    response.headers['X-HTCondorView-Cached-Response'] = f"{cached_response}"
    return response
