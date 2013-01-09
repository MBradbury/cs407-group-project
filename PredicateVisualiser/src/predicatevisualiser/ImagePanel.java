/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package predicatevisualiser;

import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import javax.imageio.ImageIO;
import javax.swing.JPanel;

/**
 *
 * @author Tim
 */
public class ImagePanel extends JPanel {
    private BufferedImage image;

    public ImagePanel(String path) {
       try {                
            image = ImageIO.read(new File(path));
       } catch (IOException ex) {
            //TODO handle ex
       }
    }

    @Override
    public void paintComponent(Graphics g) {
        super.paintComponent(g);
        g.drawImage(image, 0, 0, null); // see javadoc for more info on the parameters            
    }
}
