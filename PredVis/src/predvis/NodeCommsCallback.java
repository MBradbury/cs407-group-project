package predvis;

/**
 *
 * @author Matt
 */
public interface NodeCommsCallback {
    void receivedLine(String line);
    void closedConnection();
    void lostConnection(Exception e);
}
