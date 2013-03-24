package predvis;

import java.io.*;
import predvis.hoppy.Hoppy;

/**
 *
 * @author Tim
 */
public class Predicate {
    private String name;
    private File scriptFile;
    
    private int[] bytecode;
    
    public Predicate()
    {
        //Do nothing.
    }
    
    public Predicate(String name, File scriptFile) {
        this.name = name;
        this.scriptFile = scriptFile;
        
        compileScript();
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
    
    public boolean isSatisfied() {
        //TODO: query network, is predicate satisfied?
        return false;
    }
    
    private void compileScript() {
        try {
            
            Hoppy.run(new FileInputStream(scriptFile), new FileOutputStream(scriptFile.getAbsolutePath() + ".compiled"));
        }
        catch (FileNotFoundException e) {
            //TODO
        }
        catch (Exception e) {
            //TODO
        }
    }
    
    @Override
    public String toString()
    {
        return name + " (" + scriptFile.getAbsolutePath() + ")";
    }
}
