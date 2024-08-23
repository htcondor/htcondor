def readable_time(seconds, abbrev=False):
    """Takes seconds and produces a readable time, for example:
    2 seconds
    13 minutes
    5 hours
    1 day 6 hours"""

    if seconds < 60:  # seconds
        value = seconds
        unit = "second"
        if abbrev:
            unit = "s"
        elif value > 1:
            unit += "s"
        return f"{value} {unit}"

    elif seconds < 60*60:  # minutes
        value = round(seconds/60)
        unit = "minute"
        if abbrev:
            unit = "m"
        elif value > 1:
            unit += "s"
        return f"{value} {unit}"

    elif seconds < 60*60*24:  # hours
        value = round(seconds/(60*60))
        units = "hour"
        if abbrev:
            units = "h"
        elif value > 1:
            units += "s"
        return f"{value} {units}"

    else:  # days, including hours
        day_value = round(seconds/(60*60*24))
        day_unit = "day"
        hour_value = round((seconds % (60*60*24))/(60*60))
        hour_unit = "hour"
        if abbrev:
            day_unit = "d"
            hour_unit = "h"
        else:
            if day_value > 1:
                day_unit += "s"
            if hour_value > 1:
                hour_unit += "h"
        return f"{day_value} {day_unit} {hour_value} {hour_unit}"


def readable_size(bytes, decimals=3, abbrev=True):
    """Takes bytes and produces a readable size, for example:
    256 B
    1.456 MB
    34.983 GB"""

    if bytes < 10**3:
        value = bytes
        unit = "Bytes"
        if abbrev:
            unit = "B"

    elif bytes < 10**6:
        value = f"{round(bytes/10**3, decimals):.3f}"
        unit = "Kilobytes"
        if abbrev:
            unit = "KB"

    elif bytes < 10**9:
        value = f"{round(bytes/10**6, decimals):.3f}"
        unit = "Megabytes"
        if abbrev:
            unit = "MB"

    elif bytes < 10**12:
        value = f"{round(bytes/10**9, decimals):.3f}"
        unit = "Gigabytes"
        if abbrev:
            unit = "GB"

    elif bytes < 10**15:
        value = f"{round(bytes/10**12, decimals):.3f}"
        unit = "Terabytes"
        if abbrev:
            unit = "TB"

    else:
        value = f"{round(bytes/10**15, decimals):.3f}"
        unit = "Petabytes"
        if abbrev:
            unit = "PB"

    return f"{value} {unit}"


def s(n):
    """Pluralizing function, returns "s" if n != 1, otherwise returns empty string"""
    if isinstance(n, int) and n == 1:
        return ""
    else:
        return "s"
