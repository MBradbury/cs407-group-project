package predvis;

import edu.uci.ics.jung.algorithms.layout.CircleLayout;
import edu.uci.ics.jung.algorithms.layout.Layout;
import edu.uci.ics.jung.visualization.BasicVisualizationServer;
import edu.uci.ics.jung.visualization.decorators.ToStringLabeller;
import edu.uci.ics.jung.visualization.renderers.Renderer.VertexLabel.Position;
import java.awt.*;
import java.awt.event.*;
import java.awt.geom.Ellipse2D;
import java.io.File;
import java.io.IOException;
import java.util.Map;
import javax.swing.*;
import javax.swing.border.*;
import javax.swing.event.*;
import org.apache.commons.collections15.Transformer;

class PredicateListRenderer extends DefaultListCellRenderer  
{  
    @Override
    public Component getListCellRendererComponent( JList list,  
            Object value, int index, boolean isSelected,  
            boolean cellHasFocus )  
    {  
        super.getListCellRendererComponent( list, value, index,  
                isSelected, cellHasFocus );  
        
        Predicate p = (Predicate)value;
        switch(p.getStatus()) 
        {
            case UNMONITORED:
                setForeground(Color.WHITE);
                setBackground(Color.BLUE);
                break;
                
            case SATISFIED:
                setForeground(Color.BLACK);
                setBackground(Color.GREEN);
                break;

            case UNSATISFIED:
                setForeground(Color.WHITE);
                setBackground(Color.RED);
                break;

            default:
                setForeground(Color.BLACK);
                setBackground(Color.WHITE);
        }

        return this;  
    }  
}

/**
 *
 * @author Tim
 */
public class PredVis extends JFrame {
    public static final Font MONOSPACE_FONT = new Font(Font.MONOSPACED, Font.PLAIN, 12);
    public static final Font SANS_FONT = new Font(Font.SANS_SERIF, Font.PLAIN, 12);
    public static final Font SERIF_FONT = new Font(Font.SERIF, Font.PLAIN, 12);
    
    //Predicate data.
    private DefaultListModel/*<Predicate>*/ predicateListModel = null;
    private Predicate currentPredicate = null;
    
    //Network data.
    private Layout<NodeId, String> layout = null;
    private BasicVisualizationServer<NodeId, String> vv = null;
    
    //Monitoring data.
    private WSNMonitor wsnMonitor = null;
    
    //GUI widgets.
    private JMenuBar menuBar = null;
    
    private JTabbedPane tabbedPane = null;
    
    private JPanel predicatePanel = null;
    private JPanel predicateDetailPanel = null;
    private JList predicateList = null;
    private JTextArea predicateScriptEditor = null;
    private JTextArea predicateAssemblyEditor = null;
    private JPanel predicateStatsPanel = null;
    private JButton savePredicateScriptButton = null;
    private JButton savePredicateAssemblyButton = null;
    private JButton deployPredicateButton = null;
    private JButton rescindPredicateButton = null;
    
    private JPanel networkPanel = null;
    private JPanel graphPanel = null;
    private JSlider historySlider = null;
    
    private int currentRound = 0;
    
    public PredVis(String port) {
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
        initMenuBar();
        initTabbedPane();
        initPredicateViewer();
        initNetworkViewer();
        
        //Init monitoring.
        initMonitoring(port);
        
        pack();
    }
    
    private void initPredicates() {
        predicateListModel = new DefaultListModel/*<>*/();
        predicateListModel.addListDataListener(new ListDataListener() {
            @Override
            public void intervalAdded(ListDataEvent e) {
                
            }

            @Override
            public void intervalRemoved(ListDataEvent e) {
                
            }

            @Override
            public void contentsChanged(ListDataEvent e) {
                
            }
        });
    }
    
    private void initMenuBar() {
        final JFrame frame = this;
        
        menuBar = new JMenuBar();
        
        //File menu
        JMenu menu = new JMenu("File");
        menu.setMnemonic(KeyEvent.VK_F);
        menuBar.add(menu);
        
        JMenuItem menuItem = new JMenuItem("New Predicate", KeyEvent.VK_N);
        menuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                //Ask for predicate name
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
                
                //Pick directory
                File directory = null;
                final JFileChooser fileDialog = new JFileChooser("../PredicateLanguage/Hoppy-Tests/");
                fileDialog.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
                fileDialog.setDialogTitle("Predicate Directory");
                int retval = fileDialog.showOpenDialog(frame);
                if (retval == JFileChooser.APPROVE_OPTION) {
                    directory = fileDialog.getSelectedFile();
                } else {
                    //No script chosen, abort.
                    return;
                }
                
                //Create script file based on directory and predicate name
                File scriptFile = new File(directory.getAbsolutePath(), predicateName);

                //Set new predicate as current
                Predicate p = new Predicate(predicateName, scriptFile);
                predicateListModel.addElement(p);
                predicateList.setSelectedValue(p, true);
                setCurrentPredicate(p);
            }
        });
        menu.add(menuItem);
        
        menuItem = new JMenuItem("Load Predicate", KeyEvent.VK_L);
        menuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                //Get user to locate existing script file
                File scriptFile = null;
                final JFileChooser fileDialog = new JFileChooser("../PredicateLanguage/Hoppy-Tests/");
                fileDialog.setDialogTitle("Predicate Script");
                int retval = fileDialog.showOpenDialog(frame);
                if (retval == JFileChooser.APPROVE_OPTION) {
                    scriptFile = fileDialog.getSelectedFile();
                } else {
                    //No script chosen, abort.
                    return;
                }

                //Set new predicate as current
                Predicate p = new Predicate(scriptFile.getName(), scriptFile);
                predicateListModel.addElement(p);
                predicateList.setSelectedValue(p, true);
                setCurrentPredicate(p);
            }
        });
        menu.add(menuItem);
        
        menu.addSeparator();
        
        menuItem = new JMenuItem("Quit", KeyEvent.VK_Q);
        menuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                WindowEvent wev = new WindowEvent(frame, WindowEvent.WINDOW_CLOSING);
                Toolkit.getDefaultToolkit().getSystemEventQueue().postEvent(wev);
            }
        });
        menu.add(menuItem);
        
        //Help menu
        menu = new JMenu("Help");
        menu.setMnemonic(KeyEvent.VK_H);
        menuBar.add(menu);
        
        menuItem = new JMenuItem("About", KeyEvent.VK_A);
        //TODO
        menu.add(menuItem);
        
        setJMenuBar(menuBar);
    }

    private void initTabbedPane() {
        tabbedPane = new JTabbedPane();
        getContentPane().add(tabbedPane);
    }
    
    private void initPredicateViewer() {
        final JFrame frame = this;
        JPanel panel;
        JScrollPane scrollPane;
        Border loweredEtched = BorderFactory.createEtchedBorder(EtchedBorder.LOWERED);
        
        //Main panel init
        predicatePanel = new JPanel();
        predicatePanel.setLayout(new BoxLayout(predicatePanel, BoxLayout.X_AXIS));
        
        //List init
        panel = new JPanel();
        predicateList = new JList(predicateListModel);
        predicateList.setCellRenderer(new PredicateListRenderer());
        predicateList.setPreferredSize(new Dimension(200, 600));
        predicateList.setBorder(loweredEtched);
        predicateList.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        predicateList.addListSelectionListener(new ListSelectionListener() {
            @Override
            public void valueChanged(ListSelectionEvent le) {
                if (predicateList.getSelectedIndex() != -1) {
                    setCurrentPredicate((Predicate)predicateList.getSelectedValue());
                } else {
                    //Do nothing
                }
            }
        });
        
        panel.add(predicateList);
        predicatePanel.add(panel);
        
        //Detail panel
        predicateDetailPanel = new JPanel();
        predicateDetailPanel.setLayout(new BoxLayout(predicateDetailPanel, BoxLayout.Y_AXIS));
        
        //...Script editor
        predicateScriptEditor = new JTextArea();
        predicateScriptEditor.setFont(MONOSPACE_FONT);
        predicateScriptEditor.setLineWrap(true);
        predicateScriptEditor.setWrapStyleWord(true);
        predicateScriptEditor.setBorder(loweredEtched);
        scrollPane = new JScrollPane(predicateScriptEditor);
        scrollPane.setPreferredSize(new Dimension(400, 200));
        predicateDetailPanel.add(scrollPane);
        
        //...Assembly editor
        predicateAssemblyEditor = new JTextArea();
        predicateAssemblyEditor.setFont(MONOSPACE_FONT);
        predicateAssemblyEditor.setLineWrap(true);
        predicateAssemblyEditor.setWrapStyleWord(true);
        predicateAssemblyEditor.setBorder(loweredEtched);
        scrollPane = new JScrollPane(predicateAssemblyEditor);
        scrollPane.setPreferredSize(new Dimension(400, 200));
        predicateDetailPanel.add(scrollPane);
        
        //...Predicate statistics
        predicateStatsPanel = new JPanel(new FlowLayout());
        predicateStatsPanel.setPreferredSize(new Dimension(400, 200));
        predicateStatsPanel.setBorder(new TitledBorder(loweredEtched, "Statistics"));
        predicateDetailPanel.add(predicateStatsPanel);
        
        panel = new JPanel(new FlowLayout());
        
        //...Script editor save button
        savePredicateScriptButton = new JButton("Save Script");
        savePredicateScriptButton.setEnabled(false);
        savePredicateScriptButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                currentPredicate.setScript(predicateScriptEditor.getText());
            }
        });
        panel.add(savePredicateScriptButton);
        
        //...Assembly editor save button
        savePredicateAssemblyButton = new JButton("Save Assembly");
        savePredicateAssemblyButton.setEnabled(false);
        savePredicateAssemblyButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                currentPredicate.setAssembly(predicateAssemblyEditor.getText());
            }
        });
        panel.add(savePredicateAssemblyButton);
        
        predicateDetailPanel.add(panel);
        panel = new JPanel(new FlowLayout());
        
        //...Deploy predicate button
        deployPredicateButton = new JButton("Start Monitoring");
        deployPredicateButton.setEnabled(false);
        deployPredicateButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                //TODO deploy predicate.
                predicateScriptEditor.setEditable(false);
                predicateAssemblyEditor.setEditable(false);
                savePredicateScriptButton.setEnabled(false);
                savePredicateAssemblyButton.setEnabled(false);
                deployPredicateButton.setEnabled(false);
                rescindPredicateButton.setEnabled(true);
                currentPredicate.setMonitored(true);
                predicateList.repaint();
            }
        });
        panel.add(deployPredicateButton);
        
        //...Rescind predicate button
        rescindPredicateButton = new JButton("Stop Monitoring");
        rescindPredicateButton.setEnabled(false);
        rescindPredicateButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                //TODO rescind predicate.
                predicateScriptEditor.setEditable(true);
                predicateAssemblyEditor.setEditable(true);
                savePredicateScriptButton.setEnabled(true);
                savePredicateAssemblyButton.setEnabled(true);
                deployPredicateButton.setEnabled(true);
                rescindPredicateButton.setEnabled(false);
                currentPredicate.setMonitored(false);
                predicateList.repaint();
            }
        });
        panel.add(rescindPredicateButton);
        
        predicateDetailPanel.add(panel);
        
        //Fill panels
        predicatePanel.add(predicateDetailPanel);
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
                // it doesn't work anyway... still need to figure it out - tim
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
    
    private void initMonitoring(String port) {        
        //Listen for updates to network state.
        wsnMonitor = new WSNMonitor(port);
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
    
    private void setCurrentPredicate(Predicate p) {
        currentPredicate = p;
        predicateScriptEditor.setText(p.getScript());
        predicateAssemblyEditor.setText(p.getAssembly());
        
        //Set active states based on whether the predicate is being monitored.
        if (!p.getMonitored()) {
            predicateScriptEditor.setEditable(true);
            predicateAssemblyEditor.setEditable(true);
            savePredicateScriptButton.setEnabled(true);
            savePredicateAssemblyButton.setEnabled(true);
            deployPredicateButton.setEnabled(true);
            rescindPredicateButton.setEnabled(false);
        } else {
            predicateScriptEditor.setEditable(false);
            predicateAssemblyEditor.setEditable(false);
            savePredicateScriptButton.setEnabled(false);
            savePredicateAssemblyButton.setEnabled(false);
            deployPredicateButton.setEnabled(false);
            rescindPredicateButton.setEnabled(true);
        }
    }
    
    private void updateNetworkView(NetworkState ns) {
        graphPanel.removeAll();
        
        if (ns == null) {
            //ns = new NetworkState();
            JLabel label = new JLabel("No network state received.", SwingConstants.CENTER);
            label.setPreferredSize(new Dimension(600, 600));
            graphPanel.add(label);
            graphPanel.repaint();
            return;
        }
        
        
        //Initialise network viewer.
        layout = new CircleLayout<NodeId, String>(ns.getGraph());
        layout.setSize(new Dimension(550, 550));
        vv = new BasicVisualizationServer<NodeId, String>(layout);
        vv.setPreferredSize(new Dimension(600, 600));
        
        // Init. vertex painter.
        final Transformer<NodeId, Paint> vertexPaint = new Transformer<NodeId, Paint>() {
            @Override
            public Paint transform(NodeId i) {
                //Convert node id hash to colour.
                int hash = i.hashCode();
                return new Color(hash % 256, (hash % 128) * 2, (hash % 64) * 4);
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
