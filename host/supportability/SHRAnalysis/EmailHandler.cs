using System;
using System.Collections.Generic;
using System.IO;

using Outlook = Microsoft.Office.Interop.Outlook;
namespace Microsoft.Internal.SiteRecovery.SHRAnalysis
{
    public class EmailHandler
    {
        public static void SendAutomatedSHRAnalysis(IList<string> recipients, string message, string subject, IList<string> attachments)
        {
            try
            {
                Outlook.Application oApp = new Outlook.Application();
                Outlook.MailItem oMsg = (Outlook.MailItem)oApp.CreateItem(Outlook.OlItemType.olMailItem);

                oMsg.HTMLBody = string.Format(
                    @"<h2>This is an automated SHR analysis.</h2>{0}", message);

                oMsg.Subject = subject;

                foreach(var attachment in attachments)
                {
                    oMsg.Attachments.Add(
                   attachment,
                   Outlook.OlAttachmentType.olByValue,
                   1,
                   Path.GetFileName(attachment));
                }

                Outlook.Recipients oRecims = (Outlook.Recipients)oMsg.Recipients;

                Console.WriteLine("Sending shr analysis to {0}", string.Join(",", recipients));
                foreach (var recipient in recipients)
                {
                    Outlook.Recipient oRecip = (Outlook.Recipient)oRecims.Add(recipient);
                    oRecip.Resolve();
                }

                oMsg.Send();

                oRecims = null;
                oMsg = null;
                oApp = null;
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex);
            }

        }
    }
};