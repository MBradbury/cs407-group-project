package predvis;

import com.google.common.io.LittleEndianDataOutputStream; 
import java.io.*;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.Charset;
import java.util.HashMap;
import predvis.dragon.Dragon;
import predvis.hoppy.Hoppy; 

/**
 *
 * @author Tim
 */
public class Predicate {
    public enum PredicateStatus {
        UNMONITORED,
        SATISFIED,
        UNSATISFIED
    }
    
    //Static parser so single instance only, call ReInit() between executions.
    private static Hoppy hoppy = null;
    private static Dragon dragon = null;
    
    //Primary state
    private String name;
    private File scriptFile;
    private boolean monitored = false;
    
    //Secondary state
    private String script;
    private String assembly;
    private String target;
    private VariableDetails[] vds;
    
    //Tertiary state
    private int[] bytecode;
    
    //Return data
    private String data;
    
    public Predicate() {
        //Do nothing.
    }
    
    public Predicate(String name, File scriptFile) {
        this.name = name;
        this.scriptFile = scriptFile;
        compileScript();
        assembleScript();
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
        assembleScript();
        
        try {
            BufferedWriter writer = new BufferedWriter(new FileWriter(scriptFile));
            writer.write(script);
            writer.close();
        } catch (IOException e) {
            //TODO
        }
    }
    
    public boolean getMonitored() {
        return monitored;
    }
    
    public void setMonitored(boolean monitored) {
        this.monitored = monitored;
    }
    
    public String getAssembly() {
        return assembly;
    }
    
    public void setAssembly(String assembly) {
        this.assembly = assembly;
        assembleScript();
    }
    
    public String getTarget() {
        return target;
    }
    
    public int[] getBytecode() {
        return bytecode;
    }
    
    public VariableDetails[] getVariableDetails() {
        return vds;
    }
    
    public String getData() {
        return data;
    }
    
    public void setData(String data) {
        this.data = data;
    }
    
    private static int temp = 0;
    public PredicateStatus getStatus() {
        if (monitored) {
            //TODO: query network, is predicate satisfied?
            return temp++ % 2 == 0 ? PredicateStatus.SATISFIED : PredicateStatus.UNSATISFIED;
        } else {
            return PredicateStatus.UNMONITORED;
        }
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
            HashMap<Integer, Integer> vdMap = new HashMap<Integer, Integer>();
            target = hoppy.compile(out, vdMap);
            assembly = out.toString();
            
            //Set up variable details structure
            vds = new VariableDetails[vdMap.size()];
            int i = 0;
            for (int n : vdMap.keySet()) {
                vds[i++] = new VariableDetails(vdMap.get(n), n);
            }	
        } catch (Exception e) {
            //TODO
        }
    }
    
    private void assembleScript() {
        assert(assembly != null);
        
        try {
            //Run Dragon
            InputStream in = new ByteArrayInputStream(assembly.getBytes(Charset.forName("UTF-8")));
            OutputStream out = new ByteArrayOutputStream();
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
                bytecode[i] = (int)(bytes[i] & 0xFF);
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
