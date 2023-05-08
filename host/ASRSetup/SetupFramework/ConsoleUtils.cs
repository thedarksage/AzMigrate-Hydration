using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;

namespace ASRSetupFramework
{
    /// <summary>
    /// Enables command line support on WPF application.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1400:AccessModifierMustBeDeclared",
        Justification = "Methods imported from dll.")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1600:FieldMustHaveADocumentationHeader",
        Justification = "DLL imported methods")]
    public class ConsoleUtils
    {
        /// <summary>
        /// Hide console window.
        /// </summary>
        private const int HideConsole = 0;

        /// <summary>
        /// Show console window.
        /// </summary>
        private const int ShowConsole = 5;

        /// <summary>
        /// Logs message to Console with color based on LogLevel.
        /// </summary>
        /// <param name="loglevel">Logging level.</param>
        /// <param name="message">Message to be sent written on console.</param>
        public static void Log(LogLevel loglevel, string message)
        {
            switch (loglevel)
            {
                case LogLevel.Always:
                    Console.ForegroundColor = ConsoleColor.Cyan;
                    break;
                case LogLevel.Debug:
                    Console.ForegroundColor = ConsoleColor.Green;
                    break;
                case LogLevel.Error:
                    Console.ForegroundColor = ConsoleColor.Red;
                    break;
                case LogLevel.Info:
                    Console.ForegroundColor = ConsoleColor.White;
                    break;
                case LogLevel.None:
                    Console.ForegroundColor = ConsoleColor.White;
                    break;
                case LogLevel.Warn:
                    Console.ForegroundColor = ConsoleColor.Yellow;
                    break;
            }

            Trc.Log(loglevel, message);
            Console.WriteLine(message);
            Console.ResetColor();
        }

        #region Enable/Disable Console methods
        /// <summary>
        /// Attaches console to current application.
        /// </summary>
        public static void ShowConsoleWindow()
        {
            var handle = GetConsoleWindow();

            if (handle == IntPtr.Zero)
            {
                AllocConsole();
            }
            else
            {
                ShowWindow(handle, ShowConsole);
            }
        }

        /// <summary>
        /// Remove console window from current application.
        /// </summary>
        public static void HideConsoleWindow()
        {
            var handle = GetConsoleWindow();

            ShowWindow(handle, HideConsole);
        }

        #region DLL Import Declarations
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool AllocConsole();

        [DllImport("kernel32.dll")]
        static extern IntPtr GetConsoleWindow();

        [DllImport("user32.dll")]
        static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
        #endregion
        #endregion
    }
}
