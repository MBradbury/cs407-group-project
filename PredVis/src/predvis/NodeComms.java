package predvis;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;

/**
 *
 * @author Tim
 */
public class NodeComms {
    public static final String SERIALDUMP_LINUX = "/home/user/contiki/tools/sky/serialdump-linux";
    
    private final String comPort;
    private Thread readInput = null;
    private volatile boolean stop = false;
    
    private BufferedWriter output = null;
    
    // This is the buffer size inside the contiki motes
    private static final int bufferSize = 127;

    public NodeComms(String comPort) {
        this.comPort = comPort;
    }
    
    public void connect(final NodeCommsCallback callback) {
        final String[] cmd = new String[] { SERIALDUMP_LINUX, "-b115200", comPort };
        
        //Open streams from sink node.
        try {
            final Process serialDumpProcess = Runtime.getRuntime().exec(cmd);
            final BufferedReader input = new BufferedReader(new InputStreamReader(serialDumpProcess.getInputStream()));
            final BufferedReader err = new BufferedReader(new InputStreamReader(serialDumpProcess.getErrorStream()));
            
            output = new BufferedWriter(new OutputStreamWriter(serialDumpProcess.getOutputStream()));
            
            readInput = new Thread(new Runnable() {
                @Override
                public void run() {
                    String line;

                    try {
                        while((line = input.readLine()) != null) {
                            if (stop) {
                                break;
                            }
                            
                            assert(line != null);
                            
                            // We have a line, now we need to send it to the callback
                            callback.receivedLine(line);
                        }
                        
                        err.close();
                        input.close();
                        callback.closedConnection();
                    } catch(IOException e) {
                        callback.lostConnection(e);
                    }
                }
            }, "read input stream thread");

            readInput.start();
        } catch(Exception e) {
            callback.lostConnection(e);
        }
    }
    
    public void writeln(String line) throws Exception
    {
       char[] characters = (line + '\n').toCharArray();
       
       if (characters.length > bufferSize)
       {
           throw new Exception("Message is too long.");
       }
        
        output.write(characters);
        output.flush();
    }
    
    public void close()
    {
        try {
            stop = true;
            if (readInput != null) {
                readInput.join(1000);
            }
            
        } catch (Exception e) {
            // Ignore
        }
        
        try {
            output.close();
            output = null;
        } catch (Exception e) {
            // Ignore
        }
    }
}
