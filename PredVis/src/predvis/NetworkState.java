package predvis;

import edu.uci.ics.jung.graph.Graph;
import edu.uci.ics.jung.graph.UndirectedSparseGraph;

/**
 *
 * @author Tim
 */
public class NetworkState {
    private Graph<Integer, String> g = new UndirectedSparseGraph();
    
    /**
     * Return a handle to the internal graph.
     * @return graph
     */
    public Graph<Integer, String> getGraph() {
        return g;
    }
    
    public void clearGraph() {
        for(Integer vertex : g.getVertices()) {
            g.removeVertex(vertex);
        }
    }
    
    /**
     * Specify a communication channel between two nodes.
     * @param node1
     * @param node2 
     */
    public void addEdge(int node1, int node2) {
        assert(node1 != node2);
        
        String edgeName = nodePairToEdgeName(node1, node2);
        
        //Ensure nodes exist.
        if(!g.containsVertex(node1)) { 
            g.addVertex(node1);
        }
        
        if(!g.containsVertex(node2)) {
            g.addVertex(node2);
        }
        
        //Add edge.
        if(!g.containsEdge(edgeName)) {
            g.addEdge(edgeName, node1, node2);
        }
    }
    
    /**
     * Remove a communication channel between two nodes.
     * @param node1
     * @param node2 
     */
    public void removeEdge(int node1, int node2) {
        assert(node1 != node2);
        
        String edgeName = nodePairToEdgeName(node1, node2);
        g.removeEdge(edgeName);
    }
    
    @Override
    public boolean equals(Object other) {
        if(this == other) {
            return true;
        }
        
        if(!(other instanceof NetworkState)) {
            return false;
        }
        
        NetworkState otherState = (NetworkState)other;
        return g.equals(otherState.g);
    }
    
    @Override
    public int hashCode() {
        return g.hashCode();
    }
    
    /**
     * 
     * @param node1
     * @param node2
     * @return A string that is invariant for any pair of nodes regardless of ordering.
     */
    public static String nodePairToEdgeName(int node1, int node2) {
        assert(node1 != node2);
        
        String n1 = ((Integer)node1).toString();
        String n2 = ((Integer)node2).toString();
        
        if(node1 < node2) {
            return n1 + " <-> " + n2;
        }
        else {
            return n2 + " <-> " + n1;
        }
    }
}
