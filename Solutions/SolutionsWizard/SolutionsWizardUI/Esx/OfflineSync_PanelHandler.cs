using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace Com.Inmage.Wizard
{

    abstract class OfflineSyncPanelHandler
    {



        internal abstract bool Initialize(ImportOfflineSyncForm importOfflineSyncForm);

        internal abstract bool ProcessPanel(ImportOfflineSyncForm importOfflineSyncForm);

        internal abstract bool ValidatePanel(ImportOfflineSyncForm importOfflineSyncForm);

        internal abstract bool CanGoToNextPanel(ImportOfflineSyncForm importOfflineSyncForm);

        internal abstract bool CanGoToPreviousPanel(ImportOfflineSyncForm importOfflineSyncForm);


    }

}
