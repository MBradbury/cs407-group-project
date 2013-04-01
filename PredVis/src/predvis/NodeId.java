package predvis;

import java.util.Arrays;

/**
 *
 * @author Matt
 */
public class NodeId implements Comparable<NodeId> {
    private final String[] id;
    
    public NodeId(String[] id) {
        this.id = id;
    }
    
    @Override
    public String toString() {
        StringBuilder result = new StringBuilder();
        for (int i = 0; i != id.length; ++i) {
            result.append(id[i]);
            
            if (i + 1 != id.length) {
                result.append(".");
            }
        }
        return result.toString();
    }
    
    @Override
    public int hashCode() {
        int hash = 0;
        for (String s : id) {
            hash += s.hashCode();
        }
        
        return hash;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null) {
            return false;
        }
        if (getClass() != obj.getClass()) {
            return false;
        }
        final NodeId other = (NodeId) obj;
        if (!Arrays.deepEquals(this.id, other.id)) {
            return false;
        }
        return true;
    }

    @Override
    public int compareTo(NodeId t) {
        return this.toString().compareTo(t.toString());
    }
}

class NodeIdPair {
    private final NodeId left, right;
    
    public NodeIdPair(NodeId left, NodeId right) {
        this.left = left;
        this.right = right;
    }
    
    public NodeId getLeft() {
        return left;
    }
    
    public NodeId getRight() {
        return right;
    }
    
    @Override
    public String toString() {
        return "(" + left + ", " + right + ")";
    }
}