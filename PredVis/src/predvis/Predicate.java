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
    //Static parser so single instance only, call ReInit() between executions.
    private static Hoppy HOPPY = null;
    private static Dragon DRAGON = null;
    private static int NEXT_ID = 0;
    
    //Primary state
    private final int id;
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
    
    public Predicate() {
        id = nextId();
    }
    
    public Predicate(String name, File scriptFile) {
        id = nextId();
        this.name = name;
        this.scriptFile = scriptFile;
        compileScript();
        assembleScript();
    }
    
    public int getId() {
        return id;
    }
    
    public String getName() {
        return name;
    }
    
    public void setName(String name) {
        this.name = name;
    }
    
    public String getScript() {
        //Lazily load script from script file.
        if (script != null) {
            return script;
        }
        
        try {
            FileInputStream stream = new FileInputStream(scriptFile);
            FileChannel fc = stream.getChannel();
            MappedByteBuffer bb = fc.map(FileChannel.MapMode.READ_ONLY, 0, fc.size());
            script = Charset.forName("UTF-8").decode(bb).toString();
        } catch (IOException e) {
            //Ignore
        }
        
        return script;
    }
    
    public void setScript(String script) {
        //Compile and assemble the new script, and write it out to the script file.
        this.script = script;
        compileScript();
        assembleScript();
        
        try {
            BufferedWriter writer = new BufferedWriter(new FileWriter(scriptFile));
            writer.write(script);
            writer.close();
        } catch (IOException e) {
            //Ignore
        }
    }
    
    public boolean isMonitored() {
        return monitored;
    }
    
    public void setMonitored(boolean monitored) {
        this.monitored = monitored;
    }
    
    public String getAssembly() {
        return assembly;
    }
    
    public void setAssembly(String assembly) {
        //Need to re-assemble the code when it is set.
        this.assembly = assembly;
        assembleScript();
    }
    
    public String getTarget() {
        return target;
    }
    
    public VariableDetails[] getVariableDetails() {
        return vds;
    }
    
    public int[] getBytecode() {
        return bytecode;
    }
    
    private boolean compileScript() {
        String tgt;
        HashMap<Integer, Integer> vdMap = new HashMap<Integer, Integer>();
        
        InputStream in;
        OutputStream out;
        try {
            //Initialise Hoppy's I/O streams.
            in = new ByteArrayInputStream(getScript().getBytes(Charset.forName("UTF-8")));
            out = new ByteArrayOutputStream();
            
            //Ensure Hoppy is ready to parse.
            if (HOPPY == null) {
                HOPPY = new Hoppy(in);
            } else {
                Hoppy.ReInit(in);
            }
            
            //Run Hoppy.
            tgt = HOPPY.compile(out, vdMap);
        } catch (Exception e) {
            return false;
        }
        
        //Store predicate target.
        this.target = tgt;
        
        //Store generated predicate assembly code.
        assembly = out.toString();
        
        //Set up variable details structure
        vds = new VariableDetails[vdMap.size()];
        int i = 0;
        for (int n : vdMap.keySet()) {
            vds[i++] = new VariableDetails(vdMap.get(n), n);
        }
        
        return true;
    }
    
    private boolean assembleScript() {
        assert(assembly != null);   //compileScript() must have been successfully run already.
        
        InputStream in;
        ByteArrayOutputStream out;
        try {
            //Initialise Dragon's I/O streams.
            in = new ByteArrayInputStream(assembly.getBytes(Charset.forName("UTF-8")));
            out = new ByteArrayOutputStream();
            
            //Wrap output stream for conversion to little-endian.
            LittleEndianDataOutputStream leout = new LittleEndianDataOutputStream(out);
            
            //Ensure Dragon is ready to parse.
            if (DRAGON == null) {
                DRAGON = new Dragon(in);
            } else {
                Dragon.ReInit(in);
            }
            
            //Run Dragon.
            DRAGON.run(leout);
            
            
        } catch (Exception e) {
            return false;
        }
        
        //Store generated bytecode.
        byte[] bytes = out.toByteArray();
        bytecode = new int[bytes.length];
        for (int i = 0; i < bytes.length; ++i) {
            bytecode[i] = (int)(bytes[i] & 0xFF);
        }
        
        return true;
    }
    
    @Override
    public String toString() {
        return name;
    }
    
    @Override
    public int hashCode() {
        return id;
    }
    
    @Override
    public boolean equals(Object other) {
        if (other instanceof Predicate) {
    		Predicate p = (Predicate) other;
                return this.id == p.id;
    	}

    	return false;
    }
    
    private static int nextId() {
        return NEXT_ID++;
    }
}
