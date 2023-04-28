using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace com.InMage.Wizard
{

    abstract class RecoveryPanelHandler
    {
        public int _state = 0;


        public abstract bool Initialize(RecoveryForm allServersForm);

        public abstract bool ProcessPanel(RecoveryForm allServersForm);

        public abstract bool ValidatePanel(RecoveryForm allServersForm);

        public abstract bool CanGoToNextPanel(RecoveryForm allServersForm);

        public abstract bool CanGoToPreviousPanel(RecoveryForm allServersForm);


    }

}
