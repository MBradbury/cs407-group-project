package predvis;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 *
 * @author Tim
 */
public class NodeComms {
    public static final String SERIALDUMP_LINUX = "/home/user/contiki/tools/sky/serialdump-linux";
    
    private final String comPort;
    private Thread readInput = null;
    private volatile boolean stop = false;

    public NodeComms(String comPort) {
        this.comPort = comPort;
    }
    
    public void connect(final NodeCommsCallback callback) {
        final String fullCommand = SERIALDUMP_LINUX + " " + "-b115200" + " " + comPort;
        final String[] cmd = fullCommand.split(" ");
        
        //Open streams from sink node.
        try {
            final Process serialDumpProcess = Runtime.getRuntime().exec(cmd);
            final BufferedReader input = new BufferedReader(new InputStreamReader(serialDumpProcess.getInputStream()));
            final BufferedReader err = new BufferedReader(new InputStreamReader(serialDumpProcess.getErrorStream()));
            
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
    }
}
