#!/usr/bin/python3

# This script generates a PNG image of a graph
# for user priorties and how they grown and
# shrink.  It emits the png files in docs/_images
# user-prio1.png and user-prio2.png

# If I were a better person, I'd setup sphinx
# to generate this at runtime, instead of checking
# in png files.

import math

import matplotlib.pyplot as plt

import numpy as np

def plot(initial_history, initial_usage, second_usage, second_usage_time, filename):
    range = 100 # in hours
    
    # Create inital x and y values
    t = np.arange(0, 3600 * range)
    history = np.empty(3600 * range)
    usage = np.empty(3600 * range)
    usage.fill(0)
    
    # Initial conditions
    history[0] = initial_history
    usage[0] = initial_usage
    
    # Loop invariants
    half_life = 3600 * 24
    time_increment = 1
    i = time_increment
    beta = math.pow(0.5, (1.0 / half_life))
    
    # Calculate history at all points
    while i < len(history):
        if i > second_usage_time * 3600:
            usage[i] = second_usage
        else:
            usage[i] = initial_usage
            
        history[i] = beta * history[i - time_increment] + (1 - beta) * usage[i]
        i = i + time_increment
    
    # I'm sure there's a better way to make the time axis hours
    t_in_hours = np.empty(range)
    history_in_hours = np.empty(range)
    usage_in_hours = np.empty(range)
    i = 0
    while i < len(t_in_hours):
        t_in_hours[i] = i
        usage_in_hours[i] = usage[3600 * i]
        history_in_hours[i] = history[3600 * i]
        i = i + 1
    
    # Now plot everything
    plt.figure(figsize=(16,4))
    plt.plot(t_in_hours, history_in_hours, label="Smoothed Historical Usage\n(Real User Priority)")
    plt.plot(t_in_hours, usage_in_hours, label="Actual Usage")
    plt.ylabel("cores")
    plt.xlabel("Time (hours)")
    plt.legend()
    plt.savefig(filename)
    
    
plot(initial_history = 0.0, initial_usage=0.0, second_usage=100.0, second_usage_time = 0, filename='../_images/user-prio1.png')
plot(initial_history = 0.0, initial_usage=100.0, second_usage=0.0, second_usage_time = 24, filename='../_images/user-prio2.png')

#initial_history = widgets.FloatSlider(value=100, min=0, max=500, step=10)
#initial_usage   = widgets.FloatSlider(value=100, min=0, max=500, step=10)
#second_usage   = widgets.FloatSlider(value=100, min=0, max=500, step=10)
#second_usage_time = widgets.IntSlider(value=50, min=0, max=100, step=1)
#
#interact(plot, 
#         initial_history = initial_history,
#         initial_usage = initial_usage,
#         second_usage = second_usage,
#         second_usage_time = second_usage_time)
