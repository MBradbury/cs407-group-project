package predvis;

/**
 *
 * @author Matt
 */
public final class VariableDetails {
    private int id;
    private int hops;
    
    public VariableDetails(int id, int hops) {
        setId(id);
        setHops(hops);
    }

    /**
     * @return the id
     */
    public int getId() {
        return id;
    }

    /**
     * @param id the id to set
     */
    public void setId(int id) {
        if (id < 0 || id > 255)
        {
            throw new RuntimeException("ID must be in the range of 0-255");
        }
        
        this.id = id;
    }

    /**
     * @return the hops
     */
    public int getHops() {
        return hops;
    }

    /**
     * @param hops the hops to set
     */
    public void setHops(int hops) {
        if (hops < 0 || hops > 255)
        {
            throw new RuntimeException("Hops must be in the range of 0-255");
        }
        
        this.hops = hops;
    }
    
    @Override
    public String toString()
    {
        return "(id=" + id + ", hops=" + hops + ")";
    } 
}
