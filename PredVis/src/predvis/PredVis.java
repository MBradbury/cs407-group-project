package predvis;

import edu.uci.ics.jung.algorithms.layout.CircleLayout;
import edu.uci.ics.jung.algorithms.layout.Layout;
import edu.uci.ics.jung.visualization.BasicVisualizationServer;
import edu.uci.ics.jung.visualization.decorators.ToStringLabeller;
import edu.uci.ics.jung.visualization.renderers.Renderer.VertexLabel.Position;
import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Paint;
import java.awt.Shape;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.geom.Ellipse2D;
import java.io.File;
import java.io.IOException;
import java.util.Map;
import java.util.Random;
import javax.swing.DefaultListModel;
import javax.swing.JButton;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JList;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JSlider;
import javax.swing.JTabbedPane;
import javax.swing.SwingUtilities;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.event.ListDataEvent;
import javax.swing.event.ListDataListener;
import org.apache.commons.collections15.Transformer;

/**
 *
 * @author Tim
 */
public class PredVis extends JFrame {
    //Predicate data.
    private DefaultListModel/*<Predicate>*/ predicateListModel = null;
    
    //Network data.
    private Layout<NodeId, String> layout = null;
    private BasicVisualizationServer<NodeId, String> vv = null;
    
    //Monitoring data.
    private WSNMonitor wsnMonitor = null;
    
    //GUI widgets.
    private JTabbedPane tabbedPane = null;
    private JPanel predicatePanel = null;
    private JList predicateList = null;
    private JButton addPredicateButton = null;
    private JPanel networkPanel = null;
    private JPanel graphPanel = null;
    private JSlider historySlider = null;
    
    private int currentRound = 0;
    
    public PredVis(String comPort) {
        super("Predicate Visualiser");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosing(WindowEvent ev) {
                dispose();
                wsnMonitor.close();
            }
        });
        
        //Init base state.
        initPredicates();
        
        //Init gui.
        initTabbedPane();
        initPredicateViewer();
        initNetworkViewer();
        
        //Init monitoring.
        initMonitoring(comPort);
        
        pack();
    }
    
    private void initPredicates() {
        predicateListModel = new DefaultListModel/*<>*/();
        predicateListModel.addListDataListener(new ListDataListener() {
            @Override
            public void intervalAdded(ListDataEvent e) {
                throw new UnsupportedOperationException("Not supported yet.");
            }

            @Override
            public void intervalRemoved(ListDataEvent e) {
                throw new UnsupportedOperationException("Not supported yet.");
            }

            @Override
            public void contentsChanged(ListDataEvent e) {
                
            }
        });
    }

    private void initTabbedPane() {
        tabbedPane = new JTabbedPane();
        getContentPane().add(tabbedPane);
    }
    
    private void initPredicateViewer() {
        predicatePanel = new JPanel();
        
        predicateList = new JList(predicateListModel);
        predicateList.setPreferredSize(new Dimension(600, 600));
        predicatePanel.add(predicateList);
        
        addPredicateButton = new JButton("Add Predicate");
        
        final JFrame frame = this;
        addPredicateButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                final String predicateName = (String)JOptionPane.showInputDialog(
                    frame,
                    "Give the predicate a name: ",
                    "New Predicate",
                    JOptionPane.PLAIN_MESSAGE
                );

                if ((predicateName == null) || (predicateName.length() <= 0)) {
                    //No predicate name given, abort.
                    return;
                }
                
                File scriptFile = null;
                final JFileChooser fileDialog = new JFileChooser();
                int retval = fileDialog.showOpenDialog(frame);
                if(retval == JFileChooser.APPROVE_OPTION) {
                    scriptFile = fileDialog.getSelectedFile();
                } else {
                    //No script chosen, abort.
                    return;
                }

                predicateListModel.addElement(new Predicate(predicateName, scriptFile));
            }
        });
        
        predicatePanel.add(addPredicateButton);
        
        tabbedPane.addTab("Predicates", null, predicatePanel, "View Predicate List");
    }
    
    private void handleNetworkUpdated(final Map<Integer, NetworkState> networkstates) {
        SwingUtilities.invokeLater(new Runnable() {
            @Override
            public void run() {
                if (networkstates.containsKey(currentRound)) {
                    updateNetworkView(networkstates.get(currentRound));
                }
                // TODO: Work out what the java 6 alternative of this is!
                //revalidate();
                repaint();
            }
        });
    }
    
    private void initNetworkViewer() {
        networkPanel = new JPanel();
        graphPanel = new JPanel(new BorderLayout());
        updateNetworkView(null);
        networkPanel.add(graphPanel);
        //networkPanel.add(new JLabel("Monitoring " + predicateListModel.size() + " predicates. TODO update me"));
        
        historySlider = new JSlider(JSlider.VERTICAL, 0, 0, 0);
        historySlider.addChangeListener(new ChangeListener() {
            @Override
            public void stateChanged(ChangeEvent ce) {
                if (!historySlider.getValueIsAdjusting()) {
                    currentRound = historySlider.getValue();
                    handleNetworkUpdated(wsnMonitor.getStates());
                }
            }
            
        });
        historySlider.setPaintLabels(true);
        historySlider.setPaintTicks(true);
        networkPanel.add(historySlider);
        
        tabbedPane.addTab("Network", null, networkPanel, "View Network Diagram");
    }
    
    private void initMonitoring(String comPort) {        
        //Listen for updates to network state.
        wsnMonitor = new WSNMonitor(comPort);
        wsnMonitor.addListener(new NetworkUpdateListener() {
            @Override
            public void networkUpdated(final Map<Integer, NetworkState> networkStates) {
                handleNetworkUpdated(networkStates);
                
                // Update the maximum tick of the history slider
                for (Integer i : networkStates.keySet()) {
                    if (i > historySlider.getMaximum()) {
                        historySlider.setMaximum(i);
                    }
                }
            }
        });
    }
    
    private void updateNetworkView(NetworkState ns) {
        if (ns == null) {
            ns = new NetworkState();
        }
        
        if (vv != null) {
            graphPanel.remove(vv);
        }
        
        //Initialise network viewer.
        layout = new CircleLayout<NodeId, String>(ns.getGraph());
        layout.setSize(new Dimension(550, 550));
        vv = new BasicVisualizationServer<NodeId, String>(layout);
        vv.setPreferredSize(new Dimension(600, 600));
        
        // Init. vertex painter.
        // TODO: Prevent colour changing on a resize
        // Ideally we want a function that will cap a node id to a colour
        final Transformer<NodeId, Paint> vertexPaint = new Transformer<NodeId, Paint>() {
            @Override
            public Paint transform(NodeId i) {
                Random rng = new Random();
                return new Color(rng.nextInt(256), rng.nextInt(256), rng.nextInt(256));
            }
        };
        
        final Transformer<NodeId, Shape> vertexSize = new Transformer<NodeId, Shape>(){
            @Override
            public Shape transform(NodeId i){
                return new Ellipse2D.Double(-20, -20, 40, 40);
            }
        };
        
        vv.getRenderContext().setVertexFillPaintTransformer(vertexPaint);
        vv.getRenderContext().setVertexShapeTransformer(vertexSize);
        vv.getRenderContext().setVertexLabelTransformer(new ToStringLabeller<NodeId>());
        vv.getRenderer().getVertexLabelRenderer().setPosition(Position.CNTR);
        
        graphPanel.add(vv);
        graphPanel.repaint();
    }
    
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("Must supply communication port.");
            return;
        }
        
        PredVis pv = new PredVis(args[0]);
        pv.setVisible(true); 
    }
}
