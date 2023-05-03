using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace Com.Inmage.Wizard
{

    abstract class PanelHandler
    {
        public int _state = 0;


        internal abstract bool Initialize(AllServersForm allServersForm);

        internal abstract bool ProcessPanel(AllServersForm allServersForm);

        internal abstract bool ValidatePanel(AllServersForm allServersForm);

        internal abstract bool CanGoToNextPanel(AllServersForm allServersForm);

        internal abstract bool CanGoToPreviousPanel(AllServersForm allServersForm);


    }

}
