package editor.portrait;

import java.awt.image.BufferedImage;

public class PortraitLoader {

    public static Portrait load(BufferedImage img) {

        Portrait p = new Portrait();

        p.full = img;

        p.face = img.getSubimage(0, 0, 72, 96);
        p.anim = img.getSubimage(72, 0, 24, 96);

        return p;
    }
}