using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;

namespace Com.Inmage.Wizard
{
    class CommonTools
    {
        internal static bool GetCredentials(AllServersForm allServersForm, ref string inDomain, ref string inUserName, ref string inPassword)
        {
            try
            {

                AddCredentialsPopUpForm addCredentialsForm = new AddCredentialsPopUpForm();
                addCredentialsForm.domainText.Text = allServersForm.cachedDomain;
                addCredentialsForm.userNameText.Text = allServersForm.cachedUsername;
                addCredentialsForm.passWordText.Text = allServersForm.cachedPassword;
                addCredentialsForm.useForAllCheckBox.Visible = false;
                if (allServersForm.osTypeSelected == OStype.Linux)
                {
                    addCredentialsForm.domainText.ReadOnly = true;
                }
                addCredentialsForm.credentialsHeadingLabel.Text = "Provide credentials for Master target";
                addCredentialsForm.ShowDialog();


                if (addCredentialsForm.canceled == true)
                {
                    Debug.WriteLine("GetCredentials: Returning false");
                    return false;
                }
                else
                {

                    inDomain = addCredentialsForm.domain;
                    inUserName = addCredentialsForm.userName;
                    inPassword = addCredentialsForm.passWord;
                    allServersForm.cachedDomain = addCredentialsForm.domain;
                    allServersForm.cachedUsername = addCredentialsForm.userName;
                    allServersForm.cachedPassword = addCredentialsForm.passWord;

                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }

            return true;
        }
    }
}
