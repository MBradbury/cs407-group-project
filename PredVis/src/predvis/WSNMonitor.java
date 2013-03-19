package predvis;

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
    
    public WSNMonitor(String comPort) {
        listeners = new ArrayList<NetworkUpdateListener>();
        
        states = new HashMap<Integer, NetworkState>();
        
        comms = new NodeComms(comPort);
        
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
