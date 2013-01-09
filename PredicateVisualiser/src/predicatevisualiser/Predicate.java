/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package predicatevisualiser;

/**
 *
 * @author Tim
 */
public class Predicate {
    private String name;
    
    public Predicate()
    {
        //Do nothing.
    }
    
    public String getName()
    {
        return name;
    }
    
    public void setName(String name)
    {
        this.name = name;
    }
    
    public String toString()
    {
        return name;
    }
}
