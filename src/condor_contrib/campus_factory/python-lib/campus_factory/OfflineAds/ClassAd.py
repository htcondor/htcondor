


import time
import random

class ClassAd(dict):
    """
    This class acts as a dictionary object.
    
    """
    def __init__(self, str_classad):
        self.ParseClassad(str_classad)
        
        
    def ParseClassad(self, str_classad):
        ad_dict = {}
        if len(str_classad) == 0:
            return
       
        for line in str_classad.split('\n'):
            (key, value) = line.split('=', 1)
            self[key.strip()] = value.strip()

    def ConvertToOffline(self, classadlifetime):
        self["Offline"] = "true"
        self["PreviousName"] = self["Name"]
        self["Name"] = "\"offline@%s.%s\"" % (str(int(time.time())), random.randint(1, 10000000))
        self["MyCurrentTime"] = self["LastHeardFrom"] = str(int(time.time()))
        self["ClassAdLifetime"] = str(classadlifetime)
        self["State"] = "\"Unclaimed\""
        self["Activity"] = "\"Idle\""
        self["Rank"] = 0
        
        
    
    def __str__(self):
        str_toreturn = ""
        for key in self.keys():
            str_toreturn += key + " = " + str(self[key]) + "\n"
        return str_toreturn
    


def SortClassAdsByElement(classads, element_name):
    """
    Sort the classads (or any dictionary really) by an attribute
    
    @param classads: list of classad objects
    @param element_name: string of element name
    """
    sorted_ads = sorted(classads, key=lambda ad: int(ad[element_name]))
    return sorted_ads
    
    
    
