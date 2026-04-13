package editor.portrait;

import javax.swing.*;
import java.awt.*;

public class PortraitPanel extends JPanel {

    private Portrait portrait;

    public void setPortrait(Portrait p) {
        this.portrait = p;
        repaint();
    }

    @Override
    protected void paintComponent(Graphics g) {
        super.paintComponent(g);

        if (portrait == null) {
            g.drawString("Load portrait...", 50, 50);
            return;
        }

        g.drawImage(portrait.face, 50, 50, null);
        g.drawImage(portrait.anim, 140, 50, null);

        g.setColor(Color.GREEN);
        g.drawRect(50, 50, 72, 96);

        g.setColor(Color.CYAN);
        g.drawRect(140, 50, 24, 96);
    }
}