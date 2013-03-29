package predvis;

import cern.colt.Arrays;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;


class NodeCommsResponder implements NodeCommsCallback {
    private final WSNMonitor monitor;
    
    public NodeCommsResponder(WSNMonitor monitor) {
        this.monitor = monitor;
    }

    @Override
    public void receivedLine(String line) {
        // Only parse lines that contain neighbour information
        if (line.startsWith("R=")) {
            
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
            
            monitor.update(round, pairs);
        }
        else if (line.startsWith("PF")) {
            String stripped = line
                .substring(
                    "PF *".length(),
                    line.length() - "*".length());
            
            System.out.println(stripped);
            
            String[] splitFirst = stripped.split(":");
            
            String from = splitFirst[0];
            
            int predicateId = Integer.parseInt(splitFirst[1]);
            
            HashMap<VariableDetails, Integer> vds = new HashMap<VariableDetails, Integer>();
            
            for (String det : splitFirst[2].split(","))
            {
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
            for (String data : dataSplit)
            {
                nodeData[i] = new HashMap<Integer, Object>();
                
                String[] commaSplit = data.split(",");
                
                for (String comma : commaSplit)
                {
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
            
            System.out.println("For predicate " + predicateId + " on " + from + ":");
            System.out.println("\tGot variable details " + vds);
            System.out.println("\tGot variable data " + Arrays.toString(nodeData));

        }
    }

    @Override
    public void closedConnection() {
        System.out.println("Serial Connection to node lost!");
    }

    @Override
    public void lostConnection(Exception e) {
        System.err.println("Lost connection to node (" + e + ")");
    }
    
}

/**
 *
 * @author Tim
 */
public class WSNMonitor {
    private final List<NetworkUpdateListener> listeners;
    
    private final Map<Integer, NetworkState> states;
    
    private final NodeComms comms;
    
    public WSNMonitor(String port) {
        listeners = new ArrayList<NetworkUpdateListener>();
        
        states = new HashMap<Integer, NetworkState>();
        
        comms = new NodeComms(port);
        
        comms.connect(new NodeCommsResponder(this));
    }
    
    public void close()
    {
        comms.close();
    }
    
    public Map<Integer, NetworkState> getStates() {
        return states;
    }
    
    public void addListener(NetworkUpdateListener listener) {
        listeners.add(listener);
    }
    
    /**
     * This function applies the received information to the gui
     */
    public void update(int round, List<NodeIdPair> edges) {
        
        NetworkState roundState;
        
        if (!states.containsKey(round)) {
            roundState = new NetworkState();
            states.put(round, roundState);
        }
        else {
            roundState = states.get(round);
        }
        
        // New we need to add the new edges to the network
        for (NodeIdPair pair : edges) {
            roundState.addEdge(pair.getLeft(), pair.getRight());
        }
        
        for (NetworkUpdateListener listener : listeners) {
            listener.networkUpdated(states);
        }
    }
}
