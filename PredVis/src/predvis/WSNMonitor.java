package predvis;

import java.util.ArrayList;
import java.util.List;

/**
 *
 * @author Tim
 */
public class WSNMonitor implements Runnable {
    private volatile boolean running = false;
    
    private List<NetworkUpdateListener> listeners = null;
    private NetworkState currentState = null;
    
    public WSNMonitor() {
        listeners = new ArrayList<>();
    }
    
    public void addListener(NetworkUpdateListener listener) {
        listeners.add(listener);
    }
    
    public void terminate() {
        running = false;
    }
    
    @Override
    public void run() {
        running = true;
        currentState = new NetworkState();
        
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
        }
    }
}
