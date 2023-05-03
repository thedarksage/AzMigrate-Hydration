using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace com.InMage.Wizard
{

    abstract class OfflineSyncPanelHandler
    {
       


        public abstract bool Initialize(ImportOfflineSyncForm importOfflineSyncForm);

        public abstract bool ProcessPanel(ImportOfflineSyncForm importOfflineSyncForm);

        public abstract bool ValidatePanel(ImportOfflineSyncForm importOfflineSyncForm);

        public abstract bool CanGoToNextPanel(ImportOfflineSyncForm importOfflineSyncForm);

        public abstract bool CanGoToPreviousPanel(ImportOfflineSyncForm importOfflineSyncForm);


    }

}
