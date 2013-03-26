package predvis;

import java.io.*;
import java.nio.channels.FileChannel;
import java.nio.MappedByteBuffer;
import java.nio.charset.Charset;
import predvis.hoppy.Hoppy;
import predvis.dragon.Dragon;
import com.google.common.io.LittleEndianDataOutputStream; 

/**
 *
 * @author Tim
 */
public class Predicate {
    //Static parser so single instance only, call ReInit() between executions.
    private static Hoppy hoppy = null;
    private static Dragon dragon = null;
    
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
            script = Charset.forName("UTF-8").decode(bb).toString();
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
            String script = getScript();
            
            //Run Hoppy
            InputStream in = new ByteArrayInputStream(script.getBytes(Charset.forName("UTF-8")));
            OutputStream out = new ByteArrayOutputStream();
            if (hoppy == null) {
                hoppy = new Hoppy(in);
            } else {
                hoppy.ReInit(in);
            }
            hoppy.run(out);
            
            String intermediate = out.toString();
            System.out.println("Intermediate code: " + intermediate + "\n\n");
            
            //Run Dragon
            in = new ByteArrayInputStream(out.toString().getBytes(Charset.forName("UTF-8")));
            out = new ByteArrayOutputStream();
            LittleEndianDataOutputStream leout = new LittleEndianDataOutputStream(out);
            
            if (dragon == null) {
                dragon = new Dragon(in);
            } else {
                dragon.ReInit(in);
            }
            dragon.run(leout);
            
            //Store bytecode.
            System.out.println("Bytecode: " + out.toString());
        } catch (FileNotFoundException e) {
            //TODO
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    @Override
    public String toString()
    {
        return name;
    }
}
