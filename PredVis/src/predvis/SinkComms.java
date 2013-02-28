package predvis;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 *
 * @author Tim
 */
public class SinkComms {
    public static final String SERIALDUMP_LINUX = "./tools/sky/serialdump-linux";
    
    private String comPort = null;
    private Process serialDumpProcess = null;
    
    public SinkComms(String comPort) {
        this.comPort = comPort;
    }
    
    public void connect() {
        final String fullCommand = SERIALDUMP_LINUX + " " + "-b115200" + " " + comPort;
        final String[] cmd = fullCommand.split(" ");
        
        //Open streams from sink node.
        try {
            serialDumpProcess = Runtime.getRuntime().exec(cmd);
            final BufferedReader input = new BufferedReader(new InputStreamReader(serialDumpProcess.getInputStream()));
            final BufferedReader err = new BufferedReader(new InputStreamReader(serialDumpProcess.getErrorStream()));
            
            Thread readInput = new Thread(new Runnable() {
                @Override
                public void run() {
                    String line;

                    try {
                        while((line = input.readLine()) != null) {
                            assert(line != null);
                            String[] components = line.split(" ");
 
                            for(int i=0; i<components.length; i++) {
                                System.out.print(components[i] + " ");
                            }
                            
                            System.out.println();
                        }
                        input.close();
                        System.out.println("Serialdump process shut down, exiting");
                        System.exit(1);
                    } catch(IOException e) {
                        System.err.println("Exception when reading from serialdump");
                        System.exit(1);
                    }
                }
            }, "read input stream thread");

            readInput.start();
        } catch(Exception e) {
            System.err.println("Exception when executing '" + fullCommand + "'");
            System.exit(1);
        }
    }
}
