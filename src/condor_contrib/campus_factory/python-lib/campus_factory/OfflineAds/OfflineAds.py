
# I'm lazy
MINUTE = 60
HOUR = MINUTE * 60

from campus_factory.util.ExternalCommands import RunExternal
from campus_factory.OfflineAds.ClassAd import ClassAd, SortClassAdsByElement
import time
import random
import logging

from GlideinWMS.condorMonitor import CondorStatus

class OfflineAds:
    
    def __init__(self, siteunique="GLIDEIN_Site", timekeep=HOUR*24, numclassads=10, lastmatchtime=MINUTE*5):
        """
        Initialize function
        
        @param siteunique: Classad attributed that uniquely identifies a site
        @param timekeep: Seconds to keep the classad
        @param numclassads: Maximum number of classads to keep for each Site
        @param lastmatchtime: Only consider matches that occured between now and now-lastmachtime
        
        """
        
        # Classad attributed that uniquely identifies a site
        self.siteunique = siteunique
        
        # Seconds to keep the classad
        self.timekeep = timekeep
        
        # Maximum number of classads to keep for each Site
        self.numclassads = numclassads
        
        # Only consider matches that occured between now and now-lastmachtime
        self.lastmatchtime = lastmatchtime
        
        # Hold the last time we ran the update.
        self.lastupdatetime = 0
        

    def _Initialize(self):
        pass
    
    
    def Update(self, available_sites):
        """
        Main function used in the OfflineAds class.  This function will
        calculate all of the offline ad needs of the class, and return
        what it believes should be submitted.
        
        @param available_sites: List of site names that we should look for.
        @return list - Sites that should have a glidein matched to it.
        """
        
        # Query the status
        self.condor_status = CondorStatus()
        try:
            self.condor_status.load()
        except:
            return {}
        
        # Check last match times for an recent match
        matched_sites = self.GetLastMatchedSites()

        # Check for expired classads, delete them (OFFLINE_EXPIRE_ADS_AFTER should do this)
        #self.RemoveExpiredClassads()
        
        # Check for new startd's reporting, save them while deleting the older ones (max numclassads)
        for site in self.GetUniqueAliveSites():
            new_ads = self.GetNewStartdAds(site)
            logging.debug("New Ads = %i", len(new_ads))
            
            # Limit new_ads to the number of ads we care about
            new_ads = new_ads[:self.numclassads]
            offline_ads = self.GetOfflineAds(site)
            if (self.numclassads - (len(offline_ads) + len(new_ads))) < 0:
                # Remove old ads
                sorted_offline = SortClassAdsByElement(offline_ads, "LastHeardFrom")
                self.DeAdvertiseAds(sorted_offline[:len(offline_ads) - (self.numclassads - len(new_ads))])
            
            for ad in new_ads:
                ad.ConvertToOffline(self.timekeep)   
            self.AdvertiseAds(new_ads)
        
        
        self.lastupdatetime = int(time.time())
        return matched_sites
    
    
    def GetDelinquentSites(self, available_sites):
        """
        Get the sites that have less than self.numclassads offline ads
        
        @param available_sites: list of sites to look for
        @return: list of lists - [ ["site", num_delinquent], ... ]
        """
        def Delinquent(data):
            if data.has_key("Offline"):
                if data["Offline"] == True:
                    return True
            return False
        
        fetched = self.condor_status.fetchStored(Delinquent)
        
        sites = {}
        
        # Count the number of unique sites
        for key in fetched.keys():
            if fetched[key][self.siteunique] not in sites:
                sites[fetched[key][self.siteunique]] = 0
            sites[fetched[key][self.siteunique]] += 1
            
        # Set the number of classads we need
        final_sites = {}  
        for site in sites:
            final_sites[site] = max(self.numclassads - sites[site], 0)
        
        # Fill in the missing sites
        for site in available_sites:
            if site not in final_sites.keys():
                final_sites[site] = self.numclassads
                
        return final_sites
            
        
        
    
    def AdvertiseAds(self, ads):
        """
        Advertise ads to the collector
        
        @param ads: List of ClassAd objects to advertise
        
        """
        cmd = "condor_advertise UPDATE_STARTD_AD"
        for ad in ads:
            (stdout, stderr) = RunExternal(cmd, str(ad))
            logging.debug("stdin: %s", str(ad))
            logging.debug("stdout: %s", stdout)
            logging.debug("stderr: %s", stderr)
        
        
    def DeAdvertiseAds(self, ads):
        """
        DeAdvertise ads to the collector
        
        @param ads: List of ClassAd objects to deadvertise
        
        """
        cmd = "condor_advertise INVALIDATE_STARTD_ADS"
        for ad in ads:
            str_query = "MyType = \"Query\"\n"
            str_query += "TargetType = \"Machine\"\n"
            str_query += "Requirements = Name == %s\n\n" % ad["Name"]
            RunExternal(cmd, str_query)


    def GetLastMatchedSites(self):
        """
        Return the last matched sites as configured with lastmatchtime
        
        @return: list of sites with last match
        """
        
        def Matched(data):
            if data.has_key("Offline") and data.has_key("MachineLastMatchTime"):
                if data["Offline"] == True and int(data["MachineLastMatchTime"]) > (int(time.time()) - self.lastmatchtime):
                    return True
            return False
        
        fetched = self.condor_status.fetchStored(Matched)
        
        sites = []
        for key in fetched.keys():
            if fetched[key][self.siteunique] not in sites:
                sites.append(fetched[key][self.siteunique])
        
        return sites
        
        
    
    def RemoveExpiredClassads(self):
        """
        THIS NEEDS TO BE IMPLEMENTED ?
        
        Use condor_advertise to de-advertise expired offline ads.
        It is possible to do this inside condor with OFFLINE_EXPIRE_ADS_AFTER (probably should be done in condor)
        
        
        """
    
    def GetNewStartdAds(self, site):
        """
        Get the startd's that reported since last checked
        
        @return list: List of ClassAd objects
        
        """
        #def NewAds(data):
        #    if data.has_key("Offline") == False:
                #if data["Offline"] == True and \
                #   int(data["DaemonStartTime"]) > (int(time.time()) - (int(time.time()) - self.lastupdatetime)) and \
                #   data[self.siteunique] == site:
        #        return True
        #    return False
            
        #fetched = self.condor_status.fetchStored(NewAds)
        cmd = "condor_status -l -const '(IsUndefined(Offline) == TRUE) && (DaemonStartTime > %(lastupdate)i) && (%(uniquesite)s =?= %(sitename)s)'"
        query_opts = {"lastupdate": int(time.time()) - (int(time.time()) - self.lastupdatetime),
                      "uniquesite": self.siteunique,
                      "sitename": site}
        new_cmd = cmd % query_opts
        (stdout, stderr) = RunExternal(new_cmd)
        
        ad_list = []
        for str_classad in stdout.split('\n\n'):
            if len(str_classad) > 0:
                ad_list.append(ClassAd(str_classad))
            
        return ad_list

        #for name in fetched.keys():
        #    fetched[name]["Name"] = "\"" + name + "\""

        #ads = []
        #for ad in fetched.values():
        #    ads.append(ClassAd(ad))

        #return ads
        
        
    
    def GetOfflineAds(self, site):
        """
        Get the full classads of the offline startds
        
        @param site: The site to restrict the offline ads
        
        @return: list of ClassAd objects
        """
        #def OfflineAds(data):
        #    if data.has_key("Offline"):
        #        if data["Offline"] == True and data[self.siteunique] == site:
        #            return True
        #    return False
        
        
        #fetched = self.condor_status.fetchStored(OfflineAds)
        
        cmd = "condor_status -l -const '(IsUndefined(Offline) == FALSE) && (Offline == true) && (%(uniquesite)s =?= %(sitename)s)'"
        query_opts = {"uniquesite": self.siteunique, "sitename": site}
        new_cmd = cmd % query_opts
        (stdout, stderr) = RunExternal(new_cmd)
                
        ad_list = []
        for str_classad in stdout.split('\n\n'):
            if len(str_classad) > 0:
                ad_list.append(ClassAd(str_classad))
                   
        return ad_list

        #for name in fetched.keys():
        #    fetched[name]["Name"] = "\"" + name + "\""

        #ads = []
        #for ad in fetched.values():
        #    ads.append(ClassAd(ad))

        #return ads
        
        
    
    def GetUniqueAliveSites(self):
        """
        Get the unique sites that we see right now
        
        @return: list of sites
        
        """
        def AliveSites(data):
            if not data.has_key("Offline"):
                return True
        
        fetched = self.condor_status.fetchStored(AliveSites)
        
        sites = []
        for key in fetched.keys():
            if fetched[key][self.siteunique] not in sites:
                sites.append(fetched[key][self.siteunique])
        
        return sites
        

