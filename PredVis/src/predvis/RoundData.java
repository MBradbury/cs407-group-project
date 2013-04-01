package predvis;

import java.util.HashMap;
import java.util.Map;

/**
 *
 * @author Tim
 */
public class RoundData {
    private final Map<Predicate, PredicateData> predicateData;
    private final NetworkState networkState;
    
    public RoundData() {
        this.predicateData = new HashMap<Predicate, PredicateData>();
        this.networkState = new NetworkState();
    }
    
    public Map<Predicate, PredicateData> getPredicateData() {
        return predicateData;
    }
    
    public NetworkState getNetworkState() {
        return networkState;
    }
}
