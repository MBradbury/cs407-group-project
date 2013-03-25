package predvis;

import java.io.*;
import java.nio.channels.FileChannel;
import java.nio.MappedByteBuffer;
import java.nio.charset.Charset;
import predvis.hoppy.Hoppy;

/**
 *
 * @author Tim
 */
public class Predicate {
    //Static parser so single instance only, call ReInit() between executions.
    private static Hoppy parser = null;
    
    //Primary state
    private String name;
    private File scriptFile;
    
    //Secondary state
    private String script;
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
    
    public String getScript() {
        if (script != null) {
            return script;
        }
        
        try {
            FileInputStream stream = new FileInputStream(scriptFile);
            FileChannel fc = stream.getChannel();
            MappedByteBuffer bb = fc.map(FileChannel.MapMode.READ_ONLY, 0, fc.size());
            script = Charset.defaultCharset().decode(bb).toString();
        } catch (IOException e) {
            //TODO
        }
        
        return script;
    }
    
    public void setScript(String script) {
        this.script = script;
        
        try {
            BufferedWriter writer = new BufferedWriter(new FileWriter(scriptFile));
            writer.write(script);
            writer.close();
        } catch (IOException e) {
            //TODO
        }
    }
    
    public boolean isSatisfied() {
        //TODO: query network, is predicate satisfied?
        return false;
    }
    
    private void compileScript() {
        try {
            if (parser != null) {
                parser = new Hoppy(new FileInputStream(scriptFile));
            } else {
                parser.ReInit(new FileInputStream(scriptFile));
            }
            parser.run(new FileInputStream(scriptFile), new FileOutputStream(scriptFile.getAbsolutePath() + ".compiled"));
        } catch (FileNotFoundException e) {
            //TODO
        } catch (Exception e) {
            //TODO
        }
    }
    
    @Override
    public String toString()
    {
        return name;
    }
}
