import time

from collections import defaultdict

import classad2
import htcondor2


# THE GLOBAL
ad_tables = defaultdict(dict)


def updateDaemonAd(daemonAd : classad2.ClassAd):
    daemonAd['Name'] = 'CHARMER'
    daemonAd['MyType'] = 'CHARMER'

    return daemonAd


def makeStartdAdHashKey(ad : classad2.ClassAd) -> str:
    name = ad.get("Name")
    if name is None:
        name = ad.get("Machine")

    slotID = ad.get("SlotID")
    if slotID is not None:
        name = f"{name}:{slotID}"

    addr = ad.get("MyAddress")
    if addr is None:
        addr = ad.get("StartdIPAddr")

    # In C++, we call getHostFromAddr() on the sinful `addr` first.
    return f"{name}\x1F{addr}"


def makeScheddAdHashKey(ad : classad2.ClassAd) -> str:
    name = ad.get("Name")
    if name is None:
        name = ad.get("Machine")

    scheddName = ad.get("ScheddName")
    if scheddname is not None:
        name = f"{name}:{scheddName}"

    addr = ad.get("MyAddress")
    if addr is None:
        addr = ad.get("ScheddIPAddr")

    # In C++, we call getHostFromAddr() on the sinful `addr` first.
    return f"{name}\x1F{addr}"


def makeMasterAdHashKey(ad : classad2.ClassAd) -> str:
    name = ad.get("Machine")

    addr = ""

    return f"{name}\x1F{addr}"


hashkey_table = {
    htcondor2.AdType.Startd: makeStartdAdHashKey,
    htcondor2.AdType.Schedd: makeScheddAdHashKey,
    htcondor2.AdType.Master: makeMasterAdHashKey,
}

update_command_to_ad_type_map = {
    0:  htcondor2.AdType.Startd,
    1:  htcondor2.AdType.Schedd,
    2:  htcondor2.AdType.Master,
}

query_command_to_ad_type_map = {
    5: htcondor2.AdType.Startd,
    6: htcondor2.AdType.Schedd,
    7: htcondor2.AdType.Master,
}


invalidate_command_to_ad_type_map = {
    13: htcondor2.AdType.Startd,
    14: htcondor2.AdType.Schedd,
    15: htcondor2.AdType.Master,
}


def handleUpdateCommand(command_int : int):
    classad_format = classad2.ClassAd()
    query = yield (None, None, (classad_format, end_of_message))

    updateAd = query[0]
    # htcondor2.log(htcondor2.LogLevel.Always, str(updateAd))

    global ad_tables
    adType = update_command_to_ad_type_map[command_int]
    hash_key = hashkey_table[adType](updateAd)
    # htcondor2.log(htcondor2.LogLevel.Always, f"hashkey: {hash_key}")
    ad_tables[adType][hash_key] = updateAd


def handleInvalidateCommand(command_int : int):
    # Specify and obtain the payload.
    classad_format = classad2.ClassAd()
    query = yield (None, None, (classad_format, end_of_message))
    invalidateAd = query[0]


    global ad_tables
    adType = invalidate_command_to_ad_type_map[command_int]
    hash_key = hashkey_table[adType](invalidateAd)
    # htcondor2.log(htcondor2.LogLevel.Always, f"hashkey: {hash_key}")
    if hash_key is not None:
        del ad_tables[adType][hash_key]
    else:
        for key, ad in ad_tables[adType].items():
            if ad.matches(queryAd):
                del ad_tables[adType][key]


    replyAd = classad2.ClassAd()
    yield ((1, replyAd), 0, None)
    yield ((0, end_of_message), 0, None)


def handleQueryCommand(command_int : int):
    # Specify and obtain the payload.
    classad_format = classad2.ClassAd()
    query = yield (None, None, (classad_format, end_of_message))
    queryAd = query[0]
    htcondor2.log(htcondor2.LogLevel.Always, str(queryAd))

    # Ignoring ATTR_LOCATION_QUERY.

    # Ignoring ATTR_PROJECTION.

    global ad_tables
    adType = query_command_to_ad_type_map[command_int]
    for ad in ad_tables[adType].values():
        htcondor2.log(htcondor2.LogLevel.Always, str(ad))
        if ad.matches(queryAd):
            # (Not the last message, a matching ad), no time-out, no reply expected.
            yield((1, ad), 0, None)


    # (The last message, end-of-message), no time-out, no reply expected.
    yield ((0, end_of_message), 0, None)
