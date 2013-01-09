/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package predicatevisualiser;

import edu.uci.ics.jung.graph.Graph;
import edu.uci.ics.jung.graph.UndirectedSparseGraph;

/**
 *
 * @author csujbl
 */
public class NetworkState {
    private Graph<Integer, String> g = new UndirectedSparseGraph();
    
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
