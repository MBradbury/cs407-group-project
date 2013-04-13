package predvis;

import java.util.List;
import java.io.IOException;

/**
 *
 * @author Tim
 */
public class WSNInterface {
    private final PredVis predVis;
    private final NodeComms comms;
    
    public WSNInterface(PredVis predVis, String port) {
        this.predVis = predVis;
        
        comms = new NodeComms(this, port);
        comms.connect();
    }
    
    /**
     * Shut down the node communications.
     */
    public void close() {
        comms.close();
    }
    
    /**
     * Write a predicate to the WSN.
     * @param p
     */
    public void deployPredicate(Predicate p) {
        try {
            comms.writePredicate(p.getId(), p.getTarget(), p.getBytecode(), p.getVariableDetails());
        } catch (IOException e) {
            //TODO
        }
    }
    
    /**
     * Instruct the WSN to stop monitoring a predicate.
     * @param p 
     */
    public void rescindPredicate(Predicate p) {
        try {
            comms.writeCancelPredicate(p.getId(), p.getTarget());
        } catch (IOException e) {
            //TODO
        }
    }
    
    /**
     * Neighbour data from WSN used to construct NetworkState.
     * @param round
     * @param edges
     */
    public void receiveNeighbourData(int round, List<NodeIdPair> edges) {
        //Build NetworkState from neighbour data.
        NetworkState roundState = new NetworkState();
        for (NodeIdPair pair : edges) {
            roundState.addEdge(pair.getLeft(), pair.getRight());
        }
        
        //Attach network state to specified round.
        //TODO
    }
    
    /**
     * Predicate data from WSN sent to GUI.
     * @param id
     * @param data 
     */
    public void receivePredicateData(int id, String data) {
        //Build PredicateData from predicate data.
        PredicateData predicateData = new PredicateData(PredicateData.PredicateStatus.UNSATISFIED, data);
        
        //Attach data to specified predicate.
        //TODO
    }
}
