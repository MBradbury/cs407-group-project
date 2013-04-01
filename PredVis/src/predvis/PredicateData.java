package predvis;

/**
 * 
 * @author Tim
 */
public class PredicateData {
    public enum PredicateStatus {
        UNMONITORED,
        UNEVALUATED,
        SATISFIED,
        UNSATISFIED
    }
    
    private PredicateStatus status;
    private String details;

    public PredicateData(PredicateStatus status, String details) {
    	super();
    	this.status = status;
        this.details = details;
    }
    
    public PredicateStatus getStatus() {
        return status;
    }
    
    public String getDetails() {
        return details;
    }

    @Override
    public int hashCode() {
    	int hashStatus = status != null ? status.hashCode() : 0;
    	int hashDetails = details != null ? details.hashCode() : 0;

    	return (hashStatus + hashDetails) * hashDetails + hashStatus;
    }

    @Override
    public boolean equals(Object other) {
    	if (other instanceof PredicateData) {
    		PredicateData pd = (PredicateData) other;
                boolean statusEqual = 
                        (this.status == null && pd.status == null) ||
                        this.status == pd.status;
                boolean detailsEqual = 
                        (this.details == null && pd.details == null) || 
                        this.details.equals(pd.details);
    		return statusEqual && detailsEqual;
    	}

    	return false;
    }

    @Override
    public String toString()
    { 
           return "(" + status + ", " + details + ")"; 
    }
}