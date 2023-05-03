//---------------------------------------------------------------
//  <copyright file="MySQLPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Get MySQL Root password and MySQL Database password.
//  </summary>
//
//  History:     25-Sep-2015   bhayachi     Created
//----------------------------------------------------------------

namespace UnifiedSetup
{
    using System;
    using System.Collections.Generic;
    using System.Net;
    using System.Security;
    using System.Text.RegularExpressions;
    using System.Threading;
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Threading;
    using System.Windows.Input;
    using System.Linq;
    using System.IO;
    using ASRResources;
    using ASRSetupFramework;

    /// <summary>
    /// Interaction logic for MySQLPage.xaml
    /// </summary>
    public partial class MySQLPage : BasePageForWpfControls
    {
        public MySQLPage(ASRSetupFramework.Page page)
            : base(page, StringResources.mysql_configuration, 2)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="MySQLPage"/> class.
        /// </summary>
        public MySQLPage()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();
            updateNextbuttonstate();
        }

        /// <summary>
        /// Returns true if both Rootpassword and Databasepassword
        /// succeeds validation.
        /// </summary>
        private bool validatePasswordboxPassword(PasswordBox pw)
        {
            bool ret_val = false;
            if (isPasswordNotEmpty(pw.SecurePassword))
            {
                if (isLetterExists(pw) && isNumberExists(pw) && isSplCharAvail(pw) && isReqLengthExist(pw) && !(isSpaceExists(pw)) && !(isUnsupportedSplCharAvail(pw)))
                {
                    ret_val = true;
                }
                else
                {
                    ret_val = false;
                }
            }
            else
            {
                ret_val = false;
            }
            return ret_val;
        }

        /// <summary>
        /// Overrides OnNext.
        /// </summary>
        /// <returns>True if next button is to be shown, false otherwise.</returns>
        public override bool OnNext()
        {
            try
            {
                string[] lines = { "[MySQLCredentials]", @"MySQLRootPassword = """ + this.MySQLRootPasswordPasswordbox.Password + "\"", @"MySQLUserPassword = """ + this.MySQLDatabasePasswordbox.Password + "\"" };
                string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);
                string path = sysDrive + @"Temp\ASRSetup\MySQLCredentialsFile.txt";
                string dir = sysDrive + @"Temp\ASRSetup";

                Directory.CreateDirectory(dir);
                File.WriteAllLines(path, lines);

                SetupParameters.Instance().MysqlCredsFilePath = path;
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Unable to create directory/file. Error: ", ex);
            }
            return this.ValidatePage(PageNavigation.Navigation.Next);
        }

        /// <summary>
        /// Exits the page.
        /// </summary>
        public override void ExitPage()
        {
            base.ExitPage();
        }

        /// <summary>
        /// Next button state handler.
        /// </summary>
        private void updateNextbuttonstate()
        {
            if (validatePasswordboxPassword(MySQLRootPasswordPasswordbox) && validatePasswordboxPassword(MySQLDatabasePasswordbox))
            {
                this.Page.Host.SetNextButtonState(true, true);
            }
            else
            {
                this.Page.Host.SetNextButtonState(true, false);
            }
        }

        private void MySQLRootPasswordPasswordbox_GotFocus(object sender, RoutedEventArgs e)
        {
            updateUI(MySQLRootPasswordPasswordbox);
        }

        private void MySQLDatabasePasswordbox_GotFocus(object sender, RoutedEventArgs e)
        {
            updateUI(MySQLDatabasePasswordbox);
        }

        /// <summary>
        /// Root Passwordbox password change handler.
        /// </summary>
        private void RootPasswordBoxChanged(object sender, RoutedEventArgs e)
        {
            updateUI(MySQLRootPasswordPasswordbox);
            updateNextbuttonstate();
        }

        /// <summary>
        /// Database Passwordbox password change handler.
        /// </summary>
        private void DatabasePasswordBoxChanged(object sender, RoutedEventArgs e)
        {
            updateUI(MySQLDatabasePasswordbox);
            updateNextbuttonstate();
        }

        /// <summary>
        /// Update UI based on user input.
        /// </summary>
        private bool updateUI(PasswordBox pw)
        {
            bool ret_val = false;
            int letteravail = 1;
            int numavail = 1;
            int charavail = 1;
            int lengthexists = 1;
            int spaceavail = 1;

            if (isPasswordNotEmpty(pw.SecurePassword))
            {
                if (isLetterExists(pw))
                {
                    this.PasswordRulesLetterErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                    this.PasswordRulesLetterSuccessImage.Visibility = System.Windows.Visibility.Visible;
                    letteravail = 0;
                    ret_val = true;
                }
                else
                {
                    this.PasswordRulesLetterErrorImage.Visibility = System.Windows.Visibility.Visible;
                    this.PasswordRulesLetterSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                    letteravail = 1;
                    ret_val = false;
                }

                if (isNumberExists(pw))
                {
                    this.PasswordRulesNumberErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                    this.PasswordRulesNumberSuccessImage.Visibility = System.Windows.Visibility.Visible;
                    numavail = 0;
                    ret_val = true;
                }
                else
                {
                    this.PasswordRulesNumberErrorImage.Visibility = System.Windows.Visibility.Visible;
                    this.PasswordRulesNumberSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                    numavail = 1;
                    ret_val = false;
                }

                if (isSplCharAvail(pw) && !(isUnsupportedSplCharAvail(pw)))
                {
                    this.PasswordRulesSplCharErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                    this.PasswordRulesSplCharSuccessImage.Visibility = System.Windows.Visibility.Visible;
                    charavail = 0;
                    ret_val = true;
                }
                else
                {
                    this.PasswordRulesSplCharErrorImage.Visibility = System.Windows.Visibility.Visible;
                    this.PasswordRulesSplCharSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                    charavail = 1;
                    ret_val = false;
                }

                if (isReqLengthExist(pw))
                {
                    this.PasswordRulesMinLengthErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                    this.PasswordRulesMinLengthSuccessImage.Visibility = System.Windows.Visibility.Visible;
                    lengthexists = 0;
                    ret_val = true;
                }
                else
                {
                    this.PasswordRulesMinLengthErrorImage.Visibility = System.Windows.Visibility.Visible;
                    this.PasswordRulesMinLengthSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                    lengthexists = 1;
                    ret_val = false;
                }

                if (isSpaceExists(pw))
                {
                    this.PasswordRulesNoSpaceErrorImage.Visibility = System.Windows.Visibility.Visible;
                    this.PasswordRulesNoSpaceSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                    spaceavail = 1;
                    ret_val = false;
                }
                else
                {
                    this.PasswordRulesNoSpaceErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                    this.PasswordRulesNoSpaceSuccessImage.Visibility = System.Windows.Visibility.Visible;
                    spaceavail = 0;
                    ret_val = true;
                }

                if ((letteravail == 0) && (numavail == 0) && (charavail == 0) && (lengthexists == 0) && (spaceavail == 0))
                {
                    if (pw == MySQLRootPasswordPasswordbox)
                    {
                        this.MainPasswordRulesRootPwdErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.MainPasswordRulesRootPwdSuccessImage.Visibility = System.Windows.Visibility.Visible;
                    }
                    else if (pw == MySQLDatabasePasswordbox)
                    {
                        this.MainPasswordRulesDBPwdErrorImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.MainPasswordRulesDBPwdSuccessImage.Visibility = System.Windows.Visibility.Visible;
                    }
                }
                else
                {
                    if (pw == MySQLRootPasswordPasswordbox)
                    {
                        this.MainPasswordRulesRootPwdSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.MainPasswordRulesRootPwdErrorImage.Visibility = System.Windows.Visibility.Visible;
                    }
                    else if (pw == MySQLDatabasePasswordbox)
                    {
                        this.MainPasswordRulesDBPwdSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                        this.MainPasswordRulesDBPwdErrorImage.Visibility = System.Windows.Visibility.Visible;
                    }
                }
            }
            else
            {
                if (pw == MySQLRootPasswordPasswordbox)
                {
                    this.MainPasswordRulesRootPwdSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                    this.MainPasswordRulesRootPwdErrorImage.Visibility = System.Windows.Visibility.Visible;
                }
                else if (pw == MySQLDatabasePasswordbox)
                {
                    this.MainPasswordRulesDBPwdSuccessImage.Visibility = System.Windows.Visibility.Collapsed;                  
                    this.MainPasswordRulesDBPwdErrorImage.Visibility = System.Windows.Visibility.Visible;
                }

                this.PasswordRulesLetterSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                this.PasswordRulesNumberSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                this.PasswordRulesSplCharSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                this.PasswordRulesMinLengthSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
                this.PasswordRulesNoSpaceSuccessImage.Visibility = System.Windows.Visibility.Collapsed;

                
                this.PasswordRulesLetterErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PasswordRulesNumberErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PasswordRulesSplCharErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PasswordRulesMinLengthErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PasswordRulesNoSpaceErrorImage.Visibility = System.Windows.Visibility.Visible;


                ret_val = false;
            }
            return ret_val;
        }

        /// <summary>
        /// Validates number in password.
        /// </summary>
        private bool isNumberExists(PasswordBox pw)
        {
            bool ret_val = false;

            char[] pwdarray = pw.Password.ToCharArray();
            int num_exists = 0;

            for (int i = 0; i < pwdarray.Length; i++)
            {
                char val = pwdarray[i];

                if (num_exists == 0)
                {
                    if (!char.IsNumber(val))
                    {
                        num_exists = 0;
                        ret_val = false;

                    }
                    else
                    {
                        num_exists = 1;
                        ret_val = true;
                    }
                }
            }
            return ret_val;
        }

        /// <summary>
        /// Validates letter in password.
        /// </summary>
        private bool isLetterExists(PasswordBox pw)
        {
            bool ret_val = false;

            char[] pwdarray = pw.Password.ToCharArray();
            int letter_exists = 0;

            for (int i = 0; i < pwdarray.Length; i++)
            {
                char val = pwdarray[i];
                if (letter_exists == 0)
                {
                    if (!char.IsLetter(val))
                    {
                        letter_exists = 0;
                        ret_val = false;
                    }
                    else
                    {
                        letter_exists = 1;
                        ret_val = true;
                    }
                }
            }
            return ret_val;
        }

        /// <summary>
        /// Validates special character in password.
        /// </summary>
        private bool isSplCharAvail(PasswordBox pw)
        {
            var splList = new[] { "!", "@", "#", "$", "%", "_" };
            return splList.Any(pw.Password.Contains);
        }

        /// <summary>
        /// Validates special character in password.
        /// </summary>
        private bool isUnsupportedSplCharAvail(PasswordBox pw)
        {
            var UnsupportedsplList = new[] { "~", "'", "^", "&", "*", "(", ")", "+", "=", "-", "/", @"\", ":", ";", "{", "}", "[", "]", "|", "?", "<", ">", ",", ".", "\"" };
            return UnsupportedsplList.Any(pw.Password.Contains);
        }


        /// <summary>
        /// Validates length of password.
        /// </summary>
        private bool isReqLengthExist(PasswordBox pw)
        {
            SecureString Spwd = pw.SecurePassword;
            if ((Spwd.Length > 16) || (Spwd.Length < 8))
            {
                return false;
            }
            else
            {
                return true;
            }
        }

        /// <summary>
        /// Validates whether space exists in password.
        /// </summary>
        private bool isSpaceExists(PasswordBox pw)
        {
            if (pw.Password.Contains(" "))
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        /// <summary>
        /// Validates password is empty.
        /// </summary>
        private bool isPasswordNotEmpty(SecureString Spwd)
        {
            if (Spwd.Length == 0)
            {
                return false;
            }
            else
            {
                return true;
            }
        }
    }
}
