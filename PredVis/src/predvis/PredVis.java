package predvis;

import edu.uci.ics.jung.algorithms.layout.CircleLayout;
import edu.uci.ics.jung.algorithms.layout.DAGLayout;
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
import java.util.*;
import org.apache.commons.collections15.Transformer;

/**
 * 
 * @author Tim
 */
class PredicateListRenderer extends DefaultListCellRenderer {
    private static final Map<PredicateData.PredicateStatus, Color> FOREGROUND_MAPPING;
    private static final Map<PredicateData.PredicateStatus, Color> BACKGROUND_MAPPING;
    static {
        //Initialise mapping of status to foreground colour.
        Map<PredicateData.PredicateStatus, Color> fg = new EnumMap<PredicateData.PredicateStatus, Color>(PredicateData.PredicateStatus.class);
        fg.put(PredicateData.PredicateStatus.UNMONITORED, Color.WHITE);
        fg.put(PredicateData.PredicateStatus.UNEVALUATED, Color.BLACK);
        fg.put(PredicateData.PredicateStatus.SATISFIED, Color.BLACK);
        fg.put(PredicateData.PredicateStatus.UNSATISFIED, Color.WHITE);
        FOREGROUND_MAPPING = Collections.unmodifiableMap(fg);
        
        //Initialise mapping of status to background colour.
        Map<PredicateData.PredicateStatus, Color> bg = new EnumMap<PredicateData.PredicateStatus, Color>(PredicateData.PredicateStatus.class);
        bg.put(PredicateData.PredicateStatus.UNMONITORED, Color.BLUE);
        bg.put(PredicateData.PredicateStatus.UNEVALUATED, Color.YELLOW);
        bg.put(PredicateData.PredicateStatus.SATISFIED, Color.GREEN);
        bg.put(PredicateData.PredicateStatus.UNSATISFIED, Color.RED);
        BACKGROUND_MAPPING = Collections.unmodifiableMap(bg);
    }
    
    private Map<Predicate, java.util.List<PredicateData>> predicateData = null;
    public void setPredicateData(Map<Predicate, java.util.List<PredicateData>> predicateData) {
        this.predicateData = predicateData;
    }
    
    @Override
    public Component getListCellRendererComponent(JList list, Object value, int index, 
            boolean isSelected, boolean cellHasFocus) {  
        super.getListCellRendererComponent(list, value, index, isSelected, cellHasFocus);  
        
        Predicate p = (Predicate)value;
        java.util.List<PredicateData> l = predicateData.get(p);
        PredicateData pd = null;
        if(!l.isEmpty()) {
            pd = l.get(l.size() - 1);
        }
        
        if (p.isMonitored()) {
            if (pd != null) {
                setForeground(FOREGROUND_MAPPING.get(pd.getStatus()));
                setBackground(BACKGROUND_MAPPING.get(pd.getStatus()));
            } else {
                //Not yet evaluated.
                setForeground(FOREGROUND_MAPPING.get(PredicateData.PredicateStatus.UNEVALUATED));
                setBackground(BACKGROUND_MAPPING.get(PredicateData.PredicateStatus.UNEVALUATED));
            }
        } else {
            //Not being monitored.
            setForeground(FOREGROUND_MAPPING.get(PredicateData.PredicateStatus.UNMONITORED));
            setBackground(BACKGROUND_MAPPING.get(PredicateData.PredicateStatus.UNMONITORED));
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
    public static final Border WIDGET_BORDER = BorderFactory.createEtchedBorder(EtchedBorder.LOWERED);
    
    public static final String NO_PREDICATE_SELECTED_MESSAGE = "No predicate selected.";
    
    //Round data.
    private Map<Integer, NetworkState> rounds = new HashMap<Integer, NetworkState>();
    private int visibleRound = 0;
    private DefaultBoundedRangeModel roundSliderModel = new DefaultBoundedRangeModel(0, 0, 0, 0);
    
    //Predicate data.
    private DefaultListModel predicateListModel = new DefaultListModel();
    private PredicateListRenderer predicateListRenderer = new PredicateListRenderer();
    private Predicate currentPredicate = null;
    private Map<Predicate, java.util.List<PredicateData>> predicateData = new HashMap<Predicate, java.util.List<PredicateData>>();
    
    //Network visualisation data.
    private Layout<NodeId, String> layout = null;
    private BasicVisualizationServer<NodeId, String> vv = null;
    
    //Monitoring data.
    private WSNInterface wsnInterface = null;
    
    //GUI widgets.
    private JMenuBar menuBar = null;
    private JPanel parentPanel = null;
    private JTabbedPane tabbedPane = null;
    private JSlider roundSlider = null;
    private JLabel visibleRoundNumber = null;
    private JPanel predicatePanel = null;
    private JPanel predicateDetailPanel = null;
    private JList predicateList = null;
    private JTextArea predicateScriptEditor = null;
    private JTextArea predicateAssemblyEditor = null;
    private JPanel predicateStatsPanel = null;
    private JLabel predicateDetails = null;
    private JButton savePredicateScriptButton = null;
    private JButton savePredicateAssemblyButton = null;
    private JButton deployPredicateButton = null;
    private JButton rescindPredicateButton = null;
    private JPanel networkPanel = null;
    private JPanel graphPanel = null;
    
    public PredVis(String device) {
        super("Predicate Visualiser");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosing(WindowEvent ev) {
                dispose();
                wsnInterface.close();
                try { Runtime.getRuntime().exec("pkill -KILL " + NodeComms.SERIALDUMP_LINUX).destroy(); } catch(Exception e) {}
            }
        });
        
        //Init gui.
        initMenuBar();
        initParentPanel();
        initPredicateViewer();
        initNetworkViewer();
        
        //Init monitoring.
        initMonitoring(device);
        
        pack();
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
                File directory;
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
                
                //Add data history.
                predicateData.put(p, new ArrayList<PredicateData>());
            }
        });
        menu.add(menuItem);
        
        menuItem = new JMenuItem("Load Predicate", KeyEvent.VK_L);
        menuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                //Get user to locate existing script file
                File scriptFile;
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
                
                //Add data history.
                predicateData.put(p, new ArrayList<PredicateData>());
            }
        });
        menu.add(menuItem);
        
        menu.addSeparator();
        
        menuItem = new JMenuItem("Quit", KeyEvent.VK_Q);
        menuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                //Imitate user clicking cross on window corner.
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
        menuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                //TODO
            }
        });
        menu.add(menuItem);
        
        setJMenuBar(menuBar);
    }

    private void initParentPanel() {
        parentPanel = new JPanel();
        parentPanel.setLayout(new BoxLayout(parentPanel, BoxLayout.Y_AXIS));
        
        //Tabbed pane
        tabbedPane = new JTabbedPane();
        parentPanel.add(tabbedPane);
        
        //Round slider
        JPanel panel = new JPanel();
        panel.setLayout(new BoxLayout(panel, BoxLayout.X_AXIS));
        panel.add(new JLabel("Rounds: "));
        roundSlider = new JSlider(roundSliderModel);
        roundSlider.addChangeListener(new ChangeListener() {
            @Override
            public void stateChanged(ChangeEvent e) {
                //Wait for user to stop messing with slider and then display the round
                //if it is not the one we're already on.
                JSlider source = (JSlider)e.getSource();
                if (!source.getValueIsAdjusting()) {
                    visibleRound = (int)source.getValue();
                }
            }
        });
        
        panel.add(roundSlider);
        visibleRoundNumber = new JLabel("1 / 1");
        panel.add(visibleRoundNumber);
        parentPanel.add(panel);
        
        //Attach to frame
        getContentPane().add(parentPanel);
    }
    
    private void initPredicateViewer() {
        //final JFrame frame = this;
        
        //Temporaries
        JPanel panel;
        JScrollPane scrollPane;
        
        /*
         * LAYOUT HIERARCHY
         * 
         * horizontal box layout
         *      flow layout
         *          predicate list
         *      vertical box layout
         *          script editor
         *          assembly editor
         *          flow layout
         *              predicate details
         *          flow layout
         *              save buttons
         *          flow layout
         *              monitoring buttons
         *      
         */
        
        //Main panel init
        predicatePanel = new JPanel();
        predicatePanel.setLayout(new BoxLayout(predicatePanel, BoxLayout.X_AXIS));
        
        //List init
        panel = new JPanel();
        predicateList = new JList(predicateListModel);
        predicateListRenderer.setPredicateData(predicateData);
        predicateList.setCellRenderer(predicateListRenderer);
        predicateList.setPreferredSize(new Dimension(200, 550));
        predicateList.setBorder(WIDGET_BORDER);
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
        predicateScriptEditor.setBorder(WIDGET_BORDER);
        scrollPane = new JScrollPane(predicateScriptEditor);
        scrollPane.setPreferredSize(new Dimension(400, 200));
        predicateDetailPanel.add(scrollPane);
        
        //...Assembly editor
        predicateAssemblyEditor = new JTextArea();
        predicateAssemblyEditor.setFont(MONOSPACE_FONT);
        predicateAssemblyEditor.setLineWrap(true);
        predicateAssemblyEditor.setWrapStyleWord(true);
        predicateAssemblyEditor.setBorder(WIDGET_BORDER);
        scrollPane = new JScrollPane(predicateAssemblyEditor);
        scrollPane.setPreferredSize(new Dimension(400, 200));
        predicateDetailPanel.add(scrollPane);
        
        //...Predicate statistics
        predicateStatsPanel = new JPanel(new FlowLayout());
        predicateStatsPanel.setPreferredSize(new Dimension(400, 200));
        predicateStatsPanel.setBorder(new TitledBorder(WIDGET_BORDER, "Statistics"));
        predicateDetails = new JLabel(NO_PREDICATE_SELECTED_MESSAGE);
        predicateStatsPanel.add(predicateDetails);
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
                //Deploy predicate.
                deployPredicate(currentPredicate);
                
                //Update gui elements.
                predicateScriptEditor.setEditable(false);
                predicateAssemblyEditor.setEditable(false);
                savePredicateScriptButton.setEnabled(false);
                savePredicateAssemblyButton.setEnabled(false);
                deployPredicateButton.setEnabled(false);
                rescindPredicateButton.setEnabled(true);
                currentPredicate.setMonitored(true);
                refreshPredicateDetails();
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
                //Rescind predicate.
                rescindPredicate(currentPredicate);
                
                //Update gui elements.
                predicateScriptEditor.setEditable(true);
                predicateAssemblyEditor.setEditable(true);
                savePredicateScriptButton.setEnabled(true);
                savePredicateAssemblyButton.setEnabled(true);
                deployPredicateButton.setEnabled(true);
                rescindPredicateButton.setEnabled(false);
                currentPredicate.setMonitored(false);
                refreshPredicateDetails();
                predicateList.repaint();
            }
        });
        panel.add(rescindPredicateButton);
        
        predicateDetailPanel.add(panel);
        
        //Fill panels
        predicatePanel.add(predicateDetailPanel);
        tabbedPane.addTab("Predicates", null, predicatePanel, "View Predicate List");
    }
    
    private void initNetworkViewer() {
        networkPanel = new JPanel();
        graphPanel = new JPanel(new BorderLayout());
        updateNetworkView(null);
        networkPanel.add(graphPanel);
        
        tabbedPane.addTab("Network", null, networkPanel, "View Network Diagram");
    }
    
    private void initMonitoring(String port) {        
        //Listen for updates to network state.
        wsnInterface = new WSNInterface(this, port);
    }
    
    private void setCurrentPredicate(Predicate p) {
        currentPredicate = p;
        
        if (p != null) {
            predicateScriptEditor.setText(p.getScript());
            predicateAssemblyEditor.setText(p.getAssembly());

            refreshPredicateDetails();

            //Set active states based on whether the predicate is being monitored.
            if (!p.isMonitored()) {
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
        } else {
            predicateScriptEditor.setEditable(false);
            predicateScriptEditor.setText("");
            predicateAssemblyEditor.setEditable(false);
            predicateAssemblyEditor.setText("");
            savePredicateScriptButton.setEnabled(false);
            savePredicateAssemblyButton.setEnabled(false);
            deployPredicateButton.setEnabled(false);
            rescindPredicateButton.setEnabled(false);
            predicateDetails.setText(NO_PREDICATE_SELECTED_MESSAGE);
        }
    }
    
    private void refreshPredicateDetails() {
        java.util.List<PredicateData> pd = predicateData.get(currentPredicate);
        if (!pd.isEmpty()) {
            StringBuilder sd = new StringBuilder();
            for (PredicateData pdd : pd) {
                sd.append(pdd.getDetails()).append("\n");
            }
            predicateDetails.setText(sd.toString());
        } else {
            predicateDetails.setText("No data available for this predicate.");
        }
    }
    
    private void showRound(int round) {
        //Display network state.
        updateNetworkView(rounds.get(round));
        
        //Show round # in bottom right.
        visibleRoundNumber.setText((round + 1) + " / " + rounds.size());
        visibleRound = round;
    }
    
    private void deployPredicate(Predicate p) {
        //wsnInterface.deployPredicate(p);
    }
    
    private void rescindPredicate(Predicate p) {
        //wsnInterface.rescindPredicate(p);
    }
    
    private void updateNetworkView(NetworkState ns) {
        graphPanel.removeAll();
        
        if (ns == null) {
            /*JLabel label = new JLabel("No network state received.", SwingConstants.CENTER);
            label.setPreferredSize(new Dimension(600, 600));
            graphPanel.add(label);
            graphPanel.repaint();
            return;*/
            ns = new NetworkState();
        }
        
        //Initialise network viewer.
        layout = new DAGLayout<NodeId, String>(ns.getGraph());
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
    
    public void receiveNetworkState(int round, NetworkState ns) {
        System.out.println("Hello");
        if (rounds.get(round) != null) {
            rounds.put(round, ns);
        } else {
            rounds.put(round, ns);
            //Update slider values and rerender visible round.
            int min = 0;
            int max = 0;
            for (Integer i : rounds.keySet()) {
                if (i < min) min = i;
                if (i > max) max = i;
            }
            roundSliderModel.setMinimum(min);
            roundSliderModel.setMaximum(max);
        }
        
        showRound(visibleRound);
    }
    
    public void receivePredicateData(int id, PredicateData pd) {
        //Store data with predicate.
        for (Predicate p : predicateData.keySet()) {
            if (p.getId() == id) {
                java.util.List<PredicateData> l = predicateData.get(p);
                l.add(pd);
            }
        }
        
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
