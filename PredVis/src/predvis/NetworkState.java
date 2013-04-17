package predvis;

import edu.uci.ics.jung.graph.Graph;
import edu.uci.ics.jung.graph.UndirectedSparseGraph;

/**
 *
 * @author Tim
 */
public class NetworkState {
    private Graph<NodeId, String> g = new UndirectedSparseGraph<NodeId, String>();
    
    /**
     * Return a handle to the internal graph.
     * @return graph
     */
    public Graph<NodeId, String> getGraph() {
        //return g;
        
        //Pre constructed graph for testing
        Graph<NodeId, String> _g = new UndirectedSparseGraph<NodeId, String>();
        NodeId n1 = new NodeId(new String[]{"1"});
        NodeId n2 = new NodeId(new String[]{"2"});
        NodeId n3 = new NodeId(new String[]{"3"});
        NodeId n4 = new NodeId(new String[]{"4"});
        NodeId n5 = new NodeId(new String[]{"5"});
        NodeId n6 = new NodeId(new String[]{"6"});
        _g.addVertex(n1);
        _g.addEdge(nodePairToEdgeName(n1, n2), n1, n2);
        _g.addEdge(nodePairToEdgeName(n2, n3), n1, n3);
        _g.addEdge(nodePairToEdgeName(n3, n4), n3, n4);
        _g.addEdge(nodePairToEdgeName(n4, n5), n4, n5);
        _g.addEdge(nodePairToEdgeName(n5, n6), n4, n6);
        return _g;
    }
    
    public void clearGraph() {
        for(NodeId vertex : g.getVertices()) {
            g.removeVertex(vertex);
        }
    }
    
    /**
     * Specify a communication channel between two nodes.
     * @param node1
     * @param node2 
     */
    public void addEdge(NodeId node1, NodeId node2) {
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
    public void removeEdge(NodeId node1, NodeId node2) {
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
    public static String nodePairToEdgeName(NodeId node1, NodeId node2) {
        assert(node1 != node2);
        
        String n1 = node1.toString();
        String n2 = node2.toString();
        
        if (node1.compareTo(node2) < 0) {
            return n1 + " <-> " + n2;
        }
        else {
            return n2 + " <-> " + n1;
        }
    }
}
