package predvis;

import java.util.Map;

/**
 *
 * @author Tim
 */
public interface NetworkUpdateListener {
    void networkUpdated(Map<Integer, NetworkState> states);
}
