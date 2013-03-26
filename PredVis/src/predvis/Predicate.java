package predvis;

import com.google.common.io.LittleEndianDataOutputStream; 
import java.io.*;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.Charset;
import predvis.dragon.Dragon;
import predvis.hoppy.Hoppy; 

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
    
    public Predicate() {
        //Do nothing.
    }
    
    public Predicate(String name, File scriptFile) {
        this.name = name;
        this.scriptFile = scriptFile;
        compileScript();
    }
    
    public String getName() {
        return name;
    }
    
    public void setName(String name) {
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
        compileScript();
        
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
            //Run Hoppy
            InputStream in = new ByteArrayInputStream(getScript().getBytes(Charset.forName("UTF-8")));
            OutputStream out = new ByteArrayOutputStream();
            if (hoppy == null) {
                hoppy = new Hoppy(in);
            } else {
                Hoppy.ReInit(in);
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
                Dragon.ReInit(in);
            }
            dragon.run(leout);
            
            //Store bytecode.
            byte[] bytes = ((ByteArrayOutputStream)out).toByteArray();
            bytecode = new int[bytes.length];
            for (int i = 0; i < bytes.length; ++i) {
                bytecode[i] = (int)bytes[i];
            }
        } catch (Exception e) {
            //TODO
        }
    }
    
    @Override
    public String toString() {
        return name;
    }
}
