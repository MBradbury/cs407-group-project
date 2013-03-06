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
            List<NodeIdPair> pairs = new ArrayList<>();
        
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
public class WSNMonitor implements Runnable {
    private volatile boolean running = false;
    
    private final List<NetworkUpdateListener> listeners;
    
    private final Map<Integer, NetworkState> previousStates;
    
    private int round = 0;
    private NetworkState currentState = null;
    
    private final NodeComms comms;
    
    public WSNMonitor(String comPort) {
        listeners = new ArrayList<>();
        
        previousStates = new HashMap<>();
        
        comms = new NodeComms(comPort);
        
        currentState = new NetworkState();
        
        comms.connect(new NodeCommsResponder(this));
    }
    
    public void addListener(NetworkUpdateListener listener) {
        listeners.add(listener);
    }
    
    public void terminate() {
        running = false;
    }
    
    @Override
    public void run() {
        /*running = true;
        
        //Build test network
        currentState.addEdge(1, 2);
        currentState.addEdge(3, 4);
        currentState.addEdge(5, 6);
        currentState.addEdge(2, 4);
        currentState.addEdge(4, 6);
        
        int i = 0;
        NetworkState previousState = null;
        while (running) {
            i++;
            try {
                //Notify listeners of updated network state.
                if(previousState == null || !currentState.equals(previousState)) {
                    for(NetworkUpdateListener listener : listeners) {
                        listener.networkUpdated(currentState);
                    }
                    
                    previousState = currentState;
                }
                
                //Evaluate predicates, update network state.
                if(i >= 7) {
                    currentState.removeEdge(1, 2);
                }
                
                Thread.sleep((long)1000);
            } catch (InterruptedException e) {
                running = false;
            }
        }*/
    }
    
    /**
     * This function applies the received information to the gui
     */
    public void update(int round, List<NodeIdPair> edges) {
        // Rounds not equal, on a new round
        if (this.round != round) {
            previousStates.put(this.round, currentState);
            currentState = new NetworkState();
            this.round = round;
        }
        
        // New we need to add the new edges to the network
        for (NodeIdPair pair : edges) {
            currentState.addEdge(pair.getLeft(), pair.getRight());
        }
        
        for (NetworkUpdateListener listener : listeners) {
            listener.networkUpdated(currentState);
        }
    }
}
