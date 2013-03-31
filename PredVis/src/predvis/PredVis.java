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
import java.util.*;
import org.apache.commons.collections15.Transformer;

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

    private RoundData visibleRound = null;
    
    public void setVisibleRound(RoundData visibleRound) {
        this.visibleRound = visibleRound;
    }
    
    @Override
    public Component getListCellRendererComponent(JList list, Object value, int index, 
            boolean isSelected, boolean cellHasFocus) {  
        super.getListCellRendererComponent(list, value, index, isSelected, cellHasFocus);  
        
        Predicate p = (Predicate)value;
        
        if (p.isMonitored()) {
            PredicateData pd = visibleRound.getPredicateData().get(p);
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
    private java.util.List<RoundData> previousRounds = new ArrayList<RoundData>();
    private RoundData currentRound = new RoundData();
    private RoundData visibleRound = currentRound;
    private DefaultBoundedRangeModel roundSliderModel = new DefaultBoundedRangeModel(0, 0, 0, 0);
    
    //Predicate data.
    private DefaultListModel predicateListModel = new DefaultListModel();
    private PredicateListRenderer predicateListRenderer = new PredicateListRenderer();
    private Predicate currentPredicate = null;
    
    //Network visualisation data.
    private Layout<NodeId, String> layout = null;
    private BasicVisualizationServer<NodeId, String> vv = null;
    
    //Monitoring data.
    private WSNMonitor wsnMonitor = null;
    
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
    private JSlider historySlider = null;
    
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
        
        //Init gui.
        initMenuBar();
        initParentPanel();
        initPredicateViewer();
        initNetworkViewer();
        
        //Init monitoring.
        initMonitoring(port);
        
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
                //Must be on current round to add new predicates.
                showRound(currentRound);
                
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
                currentRound.getPredicateData().put(p, null);
                predicateList.setSelectedValue(p, true);
                setCurrentPredicate(p);
            }
        });
        menu.add(menuItem);
        
        menuItem = new JMenuItem("Load Predicate", KeyEvent.VK_L);
        menuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                //Must be on current round to load new predicates.
                showRound(currentRound);
                
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
                currentRound.getPredicateData().put(p, null);
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
        //TODO
        menuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                newRound();
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
                    int round = (int)source.getValue();
                    if (round == previousRounds.size()) {
                        if (visibleRound == currentRound) {
                            return;
                        } else {
                            showRound(currentRound);
                        }
                    } else {
                        RoundData newRound = previousRounds.get(round);
                        if (currentRound != newRound) {
                            showRound(newRound);
                        }
                    }
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
        predicateList.setCellRenderer(predicateListRenderer);
        predicateList.setPreferredSize(new Dimension(200, 600));
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
                //TODO deploy predicate.
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
                //TODO rescind predicate.
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
    
    private void handleNetworkUpdated(final Map<Integer, NetworkState> networkstates) {
        SwingUtilities.invokeLater(new Runnable() {
            @Override
            public void run() {
                /*if (networkstates.containsKey(currentRound)) {
                    updateNetworkView(networkstates.get(currentRound));
                }*/
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
        
        historySlider = new JSlider(JSlider.VERTICAL, 0, 0, 0);
        historySlider.addChangeListener(new ChangeListener() {
            @Override
            public void stateChanged(ChangeEvent ce) {
                if (!historySlider.getValueIsAdjusting()) {
                    //currentRound = historySlider.getValue();
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
        
        if (p != null) {
            predicateScriptEditor.setText(p.getScript());
            predicateAssemblyEditor.setText(p.getAssembly());

            refreshPredicateDetails();

            if (visibleRound == currentRound) {
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
                //Can't edit expired rounds.
                predicateScriptEditor.setEditable(false);
                predicateAssemblyEditor.setEditable(false);
                savePredicateScriptButton.setEnabled(false);
                savePredicateAssemblyButton.setEnabled(false);
                deployPredicateButton.setEnabled(false);
                rescindPredicateButton.setEnabled(false);
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
        if (currentPredicate.isMonitored()) {
            PredicateData pd = visibleRound.getPredicateData().get(currentPredicate);
            if (pd != null) {
                predicateDetails.setText(pd.getDetails());
            } else {
                //Not yet evaluated.
                predicateDetails.setText("No data available for this predicate.");
            }
        } else {
            //Not being monitored.
            predicateDetails.setText("This predicate is not being monitored.");
        }
    }
    
    private void showRound(RoundData round) {
        //Displaying a new round deselects all predicates.
        setCurrentPredicate(null);
        
        //Load new set of predicates into list.
        predicateListModel.clear();
        for (Predicate p : round.getPredicateData().keySet()) {
            predicateListModel.addElement(p);
        }
        
        //Show round # in bottom right.
        int roundNumber;
        if (round == currentRound) {
            roundNumber = previousRounds.size() + 1;
        } else {
            roundNumber = previousRounds.indexOf(round) + 1;
        }
        visibleRoundNumber.setText(roundNumber + " / " + (previousRounds.size() + 1));
        
        predicateListRenderer.setVisibleRound(round);
        visibleRound = round;
    }
    
    private void newRound() {
        //Move current round onto previous list, create new current with all the predicates.
        previousRounds.add(currentRound);
        RoundData oldCurrent = currentRound;
        currentRound = new RoundData();
        for (Predicate p : oldCurrent.getPredicateData().keySet()) {
            currentRound.getPredicateData().put(p, null);
        }
        
        //Update slider values and rerender visible round.
        roundSliderModel.setMaximum(roundSliderModel.getMaximum() + 1);
        showRound(visibleRound);
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
