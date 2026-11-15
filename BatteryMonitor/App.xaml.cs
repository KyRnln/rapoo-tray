using System;
using System.Threading;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
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
            const string mutexName = @"Local\BatteryMonitorSingleInstance";
            s_singleInstanceMutex = new Mutex(true, mutexName, out bool createdNew);
            if (!createdNew)
            {
                Environment.Exit(0);
                return;
            }

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

        public static void ExitApplication()
        {
            NativeCore.RapooCore_Cleanup();
            s_singleInstanceMutex?.ReleaseMutex();
            s_singleInstanceMutex?.Dispose();
            Environment.Exit(0);
        }
    }
}
