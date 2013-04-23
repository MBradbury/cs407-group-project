package predvis;

import cern.colt.Arrays;
import com.google.common.base.Strings;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 *
 * @author Tim
 */
public class NodeComms {
    public static final String SERIALDUMP_LINUX = "/home/user/contiki/tools/sky/serialdump-linux";
    private static final int MOTE_BUFFER_SIZE = 127;
    
    private final WSNInterface wsnInterface;
    
    private final String device;
    private Thread readInput = null;
    private volatile boolean stop = false;
    
    private BufferedOutputStream output = null;
    private BufferedReader input = null, err = null;
    
    private Process serialDumpProcess = null;
    
    public NodeComms(WSNInterface wsnInterface, String device) {
        this.wsnInterface = wsnInterface;
        this.device = device;
    }
    
    public void connect() {
        final String[] cmd = new String[] { SERIALDUMP_LINUX, "-b115200", device };
        
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
                            receivedLine(line);
                        }
                        
                        closedConnection();
                    } catch(Exception e) {
                        lostConnection(e);
                    } finally {
                        close();
                    }
                }
            }, "read input stream thread");

            readInput.start();
        } catch(Exception e) {
            lostConnection(e);
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
    
    public void receivedLine(String line) {
        // Only parse lines that contain neighbour information
        if (line.startsWith("R=")) {
            //Round and network state.
            int round = 0;
            List<NodeIdPair> pairs = new ArrayList<NodeIdPair>();
        
            String[] results = line.split("\\|");
            
            round = Integer.valueOf(results[0].split("=")[1]);
            
            String[] results1 = results[1].split("~");
            
            for (String pair : results1) {
                String[] currentpair = pair.split(",");
                pairs.add(new NodeIdPair(
                        new NodeId(currentpair[0].split("\\.")),
                        new NodeId(currentpair[1].split("\\."))
                        ));
            }
            
            wsnInterface.receiveNeighbourData(round, pairs);
        } else if (line.startsWith("PF")) {
            //Predicate data.
            String stripped = line
                .substring(
                    "PF *".length(),
                    line.length() - "*".length());
            
            System.out.println(stripped);
            
            String[] splitFirst = stripped.split(":");
            
            String from = splitFirst[0];
            
            int predicateId = Integer.parseInt(splitFirst[1]);
            
            HashMap<VariableDetails, Integer> vds = new HashMap<VariableDetails, Integer>();
            
            for (String det : splitFirst[2].split(",")) {
                String[] detSplit = det.split("#");
                
                int hops = Integer.parseInt(detSplit[0]);
                int id = Integer.parseInt(detSplit[1]);
                int length = Integer.parseInt(detSplit[2]);
                
                VariableDetails vd = new VariableDetails(id, hops);
                
                vds.put(vd, length);
            }
            
            String[] dataSplit = splitFirst[3].split("\\|");
            
            // Each hash map contains the data for a single node
            // The key in a hash map is the function id, the value
            // is what that function returned on the node data.
            @SuppressWarnings("unchecked")
            HashMap<Integer, Object>[] nodeData = new HashMap[dataSplit.length];
            int i = 0;
            for (String data : dataSplit) {
                nodeData[i] = new HashMap<Integer, Object>();
                
                String[] commaSplit = data.split(",");
                
                for (String comma : commaSplit) {
                    String[] kvSplit = comma.split("=");
                    
                    int key = Integer.parseInt(kvSplit[0]);
                    
                    Object value;
                    try {
                        value = Integer.parseInt(kvSplit[1]);
                    } catch (NumberFormatException e) {
                        value = Double.parseDouble(kvSplit[1]);
                    }
                    
                    nodeData[i].put(key, value);
                }
                
                ++i;
            }
            
            int clockTime = Integer.parseInt(splitFirst[4]);
            
            boolean result = Integer.parseInt(splitFirst[5]) == 1;
            
            StringBuilder sb = new StringBuilder();
            sb.append("For predicate ").append(predicateId).append(" on ").append(from).append(":\n");
            sb.append("\tResult ").append(result).append("\n");
            sb.append("\tTime ").append(clockTime).append("\n");
            sb.append("\tGot variable details ").append(vds).append("\n");
            sb.append("\tGot variable data ").append(Arrays.toString(nodeData)).append("\n");
            
            wsnInterface.receivePredicateData(predicateId, sb.toString());
        }
    }
    
    public void closedConnection() {
        System.out.println("Serial Connection to node lost!");
    }

    public void lostConnection(Exception e) {
        System.err.println("Lost connection to node (" + e + ")");
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
    
    /**
     * Ensure serialdump process gets killed.
     * @throws Throwable
     */
    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        
        // Make sure we have killed the process we spawned
        if (serialDumpProcess != null) {
            serialDumpProcess.destroy();
        }
    }
}
