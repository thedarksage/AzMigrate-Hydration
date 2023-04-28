using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace Com.Inmage.Wizard 
{
    // Any panel that has background images should use this 
	// instead of Panel to reduce flicker with refresh.
    //This calss is used just to set the DoubleBuffered property of a panel used on the left side of the wizard
    // Since the panel uses a background image, it flickers when we refresh.
    // In order to reduce the flicker we need to set the DoubleBuffered property of a panel
    // Since the DoubleBuffered property is protected only an inherited class can do that
    // So we need to to inherit from Panel and use BufferedPanel instead of Panel for "applicationPanel"
    // which uses background image.
    class BufferedPanel: Panel
    {
        public BufferedPanel()
        {
            this.DoubleBuffered = true;
        }
    }
}
