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
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.io.File;
import java.io.IOException;
import java.util.Random;
import javax.swing.DefaultListModel;
import javax.swing.JButton;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JTabbedPane;
import javax.swing.SwingUtilities;
import javax.swing.WindowConstants;
import javax.swing.event.ListDataEvent;
import javax.swing.event.ListDataListener;
import org.apache.commons.collections15.Transformer;

/**
 *
 * @author Tim
 */
public class PredVis extends JFrame {
    //Predicate data.
    private DefaultListModel<Predicate> predicateListModel = null;
    
    //Network data.
    private NetworkState networkState = null;
    private Layout<Integer, String> layout = null;
    private BasicVisualizationServer<Integer, String> vv = null;
    
    //Monitoring data.
    WSNMonitor wsnMonitor = null;
    Thread monitoringThread = null;
    
    //GUI widgets.
    private JTabbedPane tabbedPane = null;
    private JPanel predicatePanel = null;
    private JList predicateList = null;
    private JButton addPredicateButton = null;
    private JPanel networkPanel = null;
    private JPanel graphPanel = null;
    
    public PredVis(String comPort) {
        super("Predicate Visualiser");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosing(WindowEvent ev) {
                killMonitoring();
                dispose();
            }
        });
        
        //Init base state.
        initPredicates();
        initNetwork();
        
        //Init gui.
        initTabbedPane();
        initPredicateViewer();
        initNetworkViewer();
        
        //Init monitoring.
        initMonitoring(comPort);
        
        pack();
    }
    
    private void initPredicates() {
        predicateListModel = new DefaultListModel<>();
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
    
     private void initNetwork() {
        networkState = new NetworkState();
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
    
    private void initNetworkViewer() {
        networkPanel = new JPanel();
        graphPanel = new JPanel(new BorderLayout());
        updateNetworkView(null);
        networkPanel.add(graphPanel);
        networkPanel.add(new JLabel("Monitoring " + predicateListModel.size() + " predicates. TODO update me"));
        
        tabbedPane.addTab("Network", null, networkPanel, "View Network Diagram");
    }
    
    private void initMonitoring(String comPort) {
        if(monitoringThread != null) {
            return;
        }
        
        //Listen for updates to network state.
        wsnMonitor = new WSNMonitor();
        wsnMonitor.addListener(new NetworkUpdateListener() {
            @Override
            public void networkUpdated(final NetworkState ns) {
                SwingUtilities.invokeLater(new Runnable() {
                    @Override
                    public void run() {
                        updateNetworkView(ns);
                        revalidate();
                        repaint();
                    }
                });
            }
        });
        
        monitoringThread = new Thread(wsnMonitor, "monitor predicates");
        monitoringThread.start();
    }
    
    private void killMonitoring() {
        if(monitoringThread != null) {
            wsnMonitor.terminate();
            
            try {
                monitoringThread.join();
            } catch(InterruptedException e) {
                //Do nothing.
            }
            
            monitoringThread = null;
            wsnMonitor = null;
        }
    }
    
    private void updateNetworkView(NetworkState ns) {
        if(ns == null) {
            ns = new NetworkState();
        }
        
        if(vv != null) {
            graphPanel.remove(vv);
        }
        
        //Initialise network viewer.
        layout = new CircleLayout(ns.getGraph());
        layout.setSize(new Dimension(550, 550));
        vv = new BasicVisualizationServer<>(layout);
        vv.setPreferredSize(new Dimension(600, 600));
        
        //Init. vertex painter.
        Transformer<Integer, Paint> vertexPaint = new Transformer<Integer, Paint>() {
            @Override
            public Paint transform(Integer i) {
                Random rng = new Random();
                return new Color(rng.nextInt(256), rng.nextInt(256), rng.nextInt(256));
            }
        };
        
        vv.getRenderContext().setVertexFillPaintTransformer(vertexPaint);
        vv.getRenderContext().setVertexLabelTransformer(new ToStringLabeller());
        vv.getRenderer().getVertexLabelRenderer().setPosition(Position.CNTR);
        
        graphPanel.add(vv);
        graphPanel.repaint();
    }
    
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws IOException {
        if(args.length != 1) {
            System.err.println("Must supply communication port.");
            return;
        }
        
        PredVis pv = new PredVis(args[0]);
        pv.setVisible(true); 
    }
}
