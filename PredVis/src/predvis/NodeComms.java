package predvis;

import com.google.common.base.Strings;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 *
 * @author Tim
 */
public class NodeComms {
    public static final String SERIALDUMP_LINUX = "/home/user/contiki/tools/sky/serialdump-linux";
    private static final int MOTE_BUFFER_SIZE = 127;
    
    private final String port;
    private Thread readInput = null;
    private volatile boolean stop = false;
    
    private BufferedOutputStream output = null;
    private BufferedReader input = null, err = null;
    
    private Process serialDumpProcess = null;
    
    public NodeComms(String port) {
        this.port = port;
    }
    
    public void connect(final NodeCommsCallback callback) {
        final String[] cmd = new String[] { SERIALDUMP_LINUX, "-b115200", port };
        
        //Open streams from sink node.
        try {
            serialDumpProcess = Runtime.getRuntime().exec(cmd);
            input = new BufferedReader(new InputStreamReader(serialDumpProcess.getInputStream()));
            err = new BufferedReader(new InputStreamReader(serialDumpProcess.getErrorStream()));            
            output = new BufferedOutputStream(serialDumpProcess.getOutputStream());
            
            readInput = new Thread(new Runnable() {
                @Override
                public void run() {
                    String line;

                    try {
                        while((line = input.readLine()) != null) {
                            if (stop) {
                                break;
                            }
                            
                            //Act on received line.
                            assert(line != null);
                            callback.receivedLine(line);
                        }
                        
                        callback.closedConnection();
                    } catch(Exception e) {
                        callback.lostConnection(e);
                    } finally {
                        close();
                    }
                }
            }, "read input stream thread");

            readInput.start();
        } catch(Exception e) {
            callback.lostConnection(e);
            close();
        }
        
        // DEBUG PREDICATE CREATION
        // not valid bytecode
        /*try {
            writePredicate(2, "10.99", new int[]{0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC, 0xFF, 0x01, 0x00, 0xAC},
                new VariableDetails[]{ new VariableDetails(1, 2) });
        } catch (Exception e) {
            System.err.println(e);
            e.printStackTrace();
        }*/
    }
    
    public void writeln(String line) throws RuntimeException, IOException {
       byte[] characters = (line + '\n').getBytes("ISO-8859-1");
       
       if (characters.length > MOTE_BUFFER_SIZE) {
           throw new RuntimeException("Message (" + line + ") is too long (" + characters.length + ").");
       }
       
       output.write(characters);
       output.flush();
    }
    
    public void writePredicate(int id, String target, int[] bytecode, VariableDetails[] vars) 
            throws RuntimeException, IOException {
        writeln("[");
        writeln(Integer.toString(id));
        writeln(target);
        
        String toWrite = "b";
        int written = 1;
        
        for (int i = 0; i < bytecode.length; ++i) {
            String inHex = Integer.toHexString(bytecode[i]);
            String padded = Strings.padStart(inHex, 2, '0');
            toWrite += padded;
            written += 2;
            
            if ((written + 2) >= MOTE_BUFFER_SIZE || (i + 1) == bytecode.length) {
                writeln(toWrite);
                written = 1;
                toWrite = "b";
            }
        }
        
        for (VariableDetails vd : vars) {
            writeln("v" + vd.getHops() + "." + vd.getId());
        }
        
        writeln("]");
    }
    
    public void writeCancelPredicate(int id, String target)
            throws RuntimeException, IOException {
        writeln("[");       
        writeln(Integer.toString(id));
        writeln(target);
        writeln("]");
    }
    
    public void close()
    {
        stop = true;
        
        try {
            if (output != null) {
                output.close();
                output = null;
            }
        } catch (Exception e) {
            // Ignore
        }
        
        try {
            if (input != null) {
                input.close();
                input = null;
            }
        } catch (Exception e) {
            // Ignore
        }
        
        try {
            if (err != null) {
                err.close();
                err = null;
            }
        } catch (Exception e) {
            // Ignore
        }
        
        try {
            if (serialDumpProcess != null) {
                serialDumpProcess.destroy();
                serialDumpProcess = null;
            }
        } catch (Exception e) {
            // Ignore
        }
        
        try {
            if (readInput != null) {
                readInput.join(1000);
                
            }
        } catch (Exception e) {
            // Ignore
        }
    }
    
    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        
        // Make sure we have killed the process we spawned
        if (serialDumpProcess != null) {
            serialDumpProcess.destroy();
        }
    }
}
