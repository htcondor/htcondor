package condor.gahp;

public interface GahpInterface {
    // Add result to the result queue
    public void addResult (int reqId, String[] result);
    
    public void requestExit();
    
    public String generateUniqueId ();
    
    // Store state in the Gahp server
    public Object storeObject (String key, Object object);
    public Object getObject (String key);
    public Object removeObject (String key);

    public String getVersion();
    
    public String [] getCommands();
    
    // Add an action item to be run when Gahp Server shuts down
    public void addCleanupStep (CleanupStep step);
}
