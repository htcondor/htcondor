#!/usr/bin/python3

# This script generate a graph for the manual
# that describes how many cores two competing users
# get

import math

import matplotlib.pyplot as plt
import numpy as np

def plot(a_start_time, a_end_time, a_requested,
         b_start_time, b_end_time, b_requested,
         filename):
    
    range= 100 # in hours
    a_start_time = 3600 * a_start_time
    a_end_time = 3600 * a_end_time
    b_start_time = 3600 * b_start_time
    b_end_time = 3600 * b_end_time
    
    pool_size = 100 # in cores
    
    # Create inital x and y values
    t = np.arange(0, 3600 * range)
    
    # The smoothed RUP
    a_history = np.empty(3600 * range)
    b_history = np.empty(3600 * range)
    
    # The Actual usage
    a_usage = np.empty(3600 * range)
    b_usage = np.empty(3600 * range)
    
    # Initial conditions
    a_history[0] = 0.5
    b_history[0] = 0.5
    
    a_usage[0] = 0.0
    b_usage[0] = 0.0
    
    # Loop invariants
    half_life = 3600 * 24
    time_increment = 1
    i = time_increment
    beta = math.pow(0.5, (1.0 / half_life))
    
    # Calculate history and usage at all points
    while i < len(t):
        a_requested_now = 0
        b_requested_now = 0
        if (i > a_start_time) & (i < a_end_time):
            a_requested_now = a_requested
        else:
            a_requested_now = 0
        
        if (i > b_start_time) & (i < b_end_time):
            b_requested_now =  b_requested
        else:
            b_requested_now = 0
            
        a_ratio = 1 - (a_history[i - 1] / (a_history[i - 1] + b_history[i - 1]))
        b_ratio = 1 - (b_history[i - 1] / (a_history[i - 1] + b_history[i - 1]))
        
        
        # Assume 100 cores
        a_limit = 100 * a_ratio
        b_limit = 100 * b_ratio
        
        a_usage[i] = min(a_requested_now, a_limit)
        b_usage[i] = min(b_requested_now, b_limit)
        
        surplus = 100 - (a_usage[i] + b_usage[i])
        if (surplus > 0):
            if (a_requested_now > a_usage[i]):
                a_usage[i] = a_usage[i] + min(surplus, a_requested_now - a_usage[i])
            if (b_requested_now > b_usage[i]):
                b_usage[i] = b_usage[i] + min(surplus, b_requested_now - b_usage[i])
                
        
        a_history[i] = beta * a_history[i - time_increment] + (1 - beta) * a_usage[i - 1]
        b_history[i] = beta * b_history[i - time_increment] + (1 - beta) * b_usage[i - 1]
        
        # RUP has a floor of 0.5
        a_history[i] = max(0.5, a_history[i])
        b_history[i] = max(0.5, b_history[i])
        
        i = i + time_increment
    
    # I'm sure there's a better way to make the time axis hours
    t_in_hours = np.empty(range)
    
    a_history_in_hours = np.empty(range)
    a_usage_in_hours = np.empty(range)
    b_history_in_hours = np.empty(range)
    b_usage_in_hours = np.empty(range)
    
    i = 0
    while i < len(t_in_hours):
        t_in_hours[i] = i
        a_usage_in_hours[i] = a_usage[3600 * i]
        a_history_in_hours[i] = a_history[3600 * i]
        
        b_usage_in_hours[i] = b_usage[3600 * i]
        b_history_in_hours[i] = b_history[3600 * i]
        i = i + 1
    
    # Now plot everything
    plt.figure(figsize=(16,4))
    #plt.plot(t_in_hours, a_history_in_hours, label="A RUP")
    plt.plot(t_in_hours, a_usage_in_hours, label="A Usage")
    
    #plt.plot(t_in_hours, b_history_in_hours, label="B RUP")
    plt.plot(t_in_hours, b_usage_in_hours, label="B Usage")
    
    plt.ylabel("cores")
    plt.xlabel("Time (hours)")
    plt.legend()
    plt.savefig(filename)
    
plot(a_start_time = 0,
     a_end_time = 100,
     a_requested = 100.0,
     b_start_time = 48.0,
     b_end_time = 100.0,
     b_requested = 100.0,
     filename = '../_images/fair-share.png')
