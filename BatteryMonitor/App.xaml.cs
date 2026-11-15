using System;
using System.IO;
using System.Threading;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.Win32;
using BatteryMonitor.Services;
using BatteryMonitor.Views;

namespace BatteryMonitor
{
    public partial class App : Application
    {
        private static Mutex? s_singleInstanceMutex;
        private static DispatcherQueue? s_dispatcherQueue;

        private static FlyoutWindow? s_flyoutWindow;
        private static SettingsWindow? s_settingsWindow;

        // GC roots for unmanaged callbacks
        private static NativeCore.CoreStateChangeCallback? s_onStateChange;
        private static NativeCore.CoreBarClickCallback? s_onBarClick;
        private static NativeCore.CoreTrayRightClickCallback? s_onTrayRightClick;

        public App()
        {
            this.InitializeComponent();
        }

        protected override void OnLaunched(LaunchActivatedEventArgs args)
        {
            // Ensure working directory is application base directory (prevents System32 on auto-start)
            Directory.SetCurrentDirectory(AppContext.BaseDirectory);

            const string mutexName = @"Local\BatteryMonitorSingleInstance";
            s_singleInstanceMutex = new Mutex(true, mutexName, out bool createdNew);
            if (!createdNew)
            {
                Environment.Exit(0);
                return;
            }

            // Clean legacy startup entries and synchronize to new executable
            MigrateAndCleanAutoStartRegistry();

            s_dispatcherQueue = DispatcherQueue.GetForCurrentThread();

            // Initialize Windows 11 Acrylic Flyout Window
            s_flyoutWindow = new FlyoutWindow();

            // Setup callbacks
            s_onStateChange = OnDeviceStateChanged;
            s_onBarClick = OnTaskbarBarClicked;
            s_onTrayRightClick = OnTrayRightClicked;

            NativeCore.RapooCore_SetCallbacks(s_onStateChange, s_onBarClick, s_onTrayRightClick);

            // Initialize native hardware telemetry and taskbar bar
            NativeCore.RapooCore_Init();
        }

        private static void OnDeviceStateChanged()
        {
            s_dispatcherQueue?.TryEnqueue(() =>
            {
                s_flyoutWindow?.RefreshState();
            });
        }

        private static void OnTaskbarBarClicked()
        {
            s_dispatcherQueue?.TryEnqueue(() =>
            {
                s_flyoutWindow?.Toggle();
            });
        }

        private static void OnTrayRightClicked(int x, int y)
        {
            s_dispatcherQueue?.TryEnqueue(() =>
            {
                ShowSettingsWindow();
            });
        }

        public static void ShowSettingsWindow()
        {
            s_dispatcherQueue?.TryEnqueue(() =>
            {
                if (s_settingsWindow == null)
                {
                    s_settingsWindow = new SettingsWindow();
                    s_settingsWindow.Closed += (s, e) => { s_settingsWindow = null; };
                }
                s_settingsWindow.Activate();
            });
        }

        private static void MigrateAndCleanAutoStartRegistry()
        {
            const string runRegKey = @"Software\Microsoft\Windows\CurrentVersion\Run";
            const string appRunName = "BatteryMonitor";
            string[] legacyNames = new[] { "RapooTray", "rapoo-tray" };

            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(runRegKey, true);
                if (key == null) return;

                bool hadLegacyAutoStart = false;
                foreach (var legacy in legacyNames)
                {
                    if (key.GetValue(legacy) != null)
                    {
                        hadLegacyAutoStart = true;
                        key.DeleteValue(legacy, false);
                    }
                }

                string currentExePath = Environment.ProcessPath ?? "";
                if (!string.IsNullOrEmpty(currentExePath))
                {
                    if (hadLegacyAutoStart || key.GetValue(appRunName) != null)
                    {
                        key.SetValue(appRunName, $"\"{currentExePath}\"");
                    }
                }
            }
            catch { }
        }

        public static void ExitApplication()
        {
            NativeCore.RapooCore_Cleanup();
            s_singleInstanceMutex?.ReleaseMutex();
            s_singleInstanceMutex?.Dispose();
            Environment.Exit(0);
        }
    }
}
