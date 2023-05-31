#!/bin/bash
# Flask needs a UTF locale; if one is not set, find one.
# Prefer C, then en_US, then just pick one.
# Depending on distro, a locale may have a ".utf8" or a ".UTF-8" suffix.
if [[ $LANG != *[Uu][Tt][Ff]* ]]; then
    utf_locales=$(locale -a | grep -i utf)
    c_utf_locale=$(printf "%s\n" "$utf_locales" | grep '^C\.' | head -n 1)
    en_us_utf_locale=$(printf "%s\n" "$utf_locales" | grep '^en_US\.' | head -n 1)
    last_resort_utf_locale=$(printf "%s\n" "$utf_locales" | head -n 1)

    if [[ $c_utf_locale ]]; then
        LANG=$c_utf_locale
    elif [[ $en_us_utf_locale ]]; then
        LANG=$en_us_utf_locale
    elif [[ $last_resort_utf_locale ]]; then
        LANG=$last_resort_utf_locale
    else
        echo 1>&2 "No UTF locale found - bailing."
        exit 1
    fi
fi
LC_ALL=$LANG

export LC_ALL LANG

export FLASK_APP=condor_restd
export FLASK_ENV=dev

# The location and name of the flask executable is also distro-dependent
for flask in {/usr/bin,/usr/local/bin}/{flask,flask-3} NOTFOUND; do
    if [[ -x $flask ]]; then
        break
    fi
done

if [[ $flask == NOTFOUND ]]; then
    echo 1>&2 "No flask executable found - bailing"
    exit 127
fi

exec "$flask" run -h 0.0.0.0 -p 8080
