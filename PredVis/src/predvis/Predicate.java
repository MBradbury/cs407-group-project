package predvis;

import java.io.File;

/**
 *
 * @author Tim
 */
public class Predicate {
    private String name;
    private File scriptFile;
    
    public Predicate()
    {
        //Do nothing.
    }
    
    public Predicate(String name, File scriptFile) {
        this.name = name;
        this.scriptFile = scriptFile;
    }
    
    public String getName()
    {
        return name;
    }
    
    public void setName(String name)
    {
        this.name = name;
    }
    
    public File getScriptFile() {
        return scriptFile;
    }
    
    public void setScriptFile(File scriptFile) {
        this.scriptFile = scriptFile;
    }
    
    @Override
    public String toString()
    {
        return name + " (" + scriptFile.getAbsolutePath() + ")";
    }
}
