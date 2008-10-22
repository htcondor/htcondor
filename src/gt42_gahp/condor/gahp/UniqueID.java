package condor.gahp;

public class UniqueID  {
    private static long time_seed=System.currentTimeMillis();
    private static long id_seed=0;
    private final Object id;
    
    public UniqueID() {
        this.id=generate();
    }
    
    public int hashCode() {
        return id.hashCode();
    }
    
    public String toString () {
        return id.toString();
    }
     
    public boolean equals (Object o) {
        if (!(o instanceof UniqueID)) {
            return false;
        }
        return this.id.equals (((UniqueID)o).id);
    }
    
    private static synchronized Object generate() {
        return new String ("" + time_seed +"."+(++id_seed));
    }
}
