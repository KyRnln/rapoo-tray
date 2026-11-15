using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Microsoft.UI;
using Microsoft.UI.Composition;
using Microsoft.UI.Composition.SystemBackdrops;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using BatteryMonitor.Models;
using BatteryMonitor.Services;
using Windows.Graphics;
using WinRT;

namespace BatteryMonitor.Views
{
    internal class WindowsSystemDispatcherQueueHelper
    {
        private object? _dispatcherQueueController = null;

        public void EnsureWindowsSystemDispatcherQueueController()
        {
            if (Windows.System.DispatcherQueue.GetForCurrentThread() != null)
                return;

            if (_dispatcherQueueController == null)
            {
                DispatcherQueueOptions options;
                options.dwSize = Marshal.SizeOf(typeof(DispatcherQueueOptions));
                options.threadType = 2; // DQTYPE_THREAD_CURRENT
                options.apartmentType = 2; // DQTAT_COM_STA

                CreateDispatcherQueueController(options, ref _dispatcherQueueController);
            }
        }

        [DllImport("CoreMessaging.dll")]
        private static extern int CreateDispatcherQueueController(
            [In] DispatcherQueueOptions options,
            [In, Out, MarshalAs(UnmanagedType.IUnknown)] ref object? dispatcherQueueController);

        [StructLayout(LayoutKind.Sequential)]
        struct DispatcherQueueOptions
        {
            internal int dwSize;
            internal int threadType;
            internal int apartmentType;
        }
    }

    public sealed partial class FlyoutWindow : Window
    {
        private AppWindow _appWindow;
        private IntPtr _hWnd;
        private bool _isShowing = false;

        private WindowsSystemDispatcherQueueHelper? _wsdqHelper;
        private DesktopAcrylicController? _acrylicController;
        private SystemBackdropConfiguration? _configurationSource;

        [DllImport("dwmapi.dll")]
        private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int attrValue, int attrSize);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

        [DllImport("user32.dll")]
        private static extern IntPtr MonitorFromPoint(POINT pt, uint dwFlags);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GetCursorPos(out POINT lpPoint);

        [DllImport("shcore.dll")]
        private static extern int GetDpiForMonitor(IntPtr hmonitor, int dpiType, out uint dpiX, out uint dpiY);

        [DllImport("user32.dll")]
        private static extern uint GetDpiForWindow(IntPtr hWnd);

        [StructLayout(LayoutKind.Sequential)]
        private struct POINT
        {
            public int X;
            public int Y;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
        private struct MONITORINFO
        {
            public int cbSize;
            public RECT rcMonitor;
            public RECT rcWork;
            public uint dwFlags;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        private static extern bool GetMonitorInfo(IntPtr hMonitor, ref MONITORINFO lpmi);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

        private const int GWL_STYLE = -16;
        private const int WS_POPUP = unchecked((int)0x80000000);
        private const int WS_BORDER = 0x00800000;
        private const int WS_DLGFRAME = 0x00400000;
        private const int WS_CAPTION = 0x00C00000;
        private const int WS_THICKFRAME = 0x00040000;
        private const int WS_MINIMIZEBOX = 0x00020000;
        private const int WS_MAXIMIZEBOX = 0x00010000;
        private const int WS_SYSMENU = 0x00080000;

        private const uint SWP_NOMOVE = 0x0002;
        private const uint SWP_NOSIZE = 0x0001;
        private const uint SWP_NOZORDER = 0x0004;
        private const uint SWP_FRAMECHANGED = 0x0020;

        private const int GWL_EXSTYLE = -20;
        private const int WS_EX_TOOLWINDOW = 0x00000080;
        private const int WS_EX_APPWINDOW = 0x00040000;
        private const int DWMWA_WINDOW_CORNER_PREFERENCE = 33;
        private const int DWMWCP_ROUND = 2;
        private const int DWMWA_BORDER_COLOR = 34;
        private const int DWMWA_SYSTEMBACKDROP_TYPE = 38;
        private const int DWMSBT_TRANSIENTWINDOW = 3; // Desktop Acrylic

        private static Brush GetBrush(string key, Windows.UI.Color fallback)
        {
            try
            {
                if (Application.Current.Resources.TryGetValue(key, out var val) && val is Brush b)
                    return b;
                string theme = Application.Current.RequestedTheme == ApplicationTheme.Dark ? "Dark" : "Light";
                if (Application.Current.Resources.ThemeDictionaries.TryGetValue(theme, out var dictObj) && dictObj is ResourceDictionary rd)
                {
                    if (rd.TryGetValue(key, out var val2) && val2 is Brush b2)
                        return b2;
                }
            }
            catch { }
            return new SolidColorBrush(fallback);
        }

        public FlyoutWindow()
        {
            this.InitializeComponent();

            _hWnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            var windowId = Win32Interop.GetWindowIdFromWindow(_hWnd);
            _appWindow = AppWindow.GetFromWindowId(windowId);

            if (_appWindow.Presenter is OverlappedPresenter presenter)
            {
                presenter.IsResizable = false;
                presenter.IsMaximizable = false;
                presenter.IsMinimizable = false;
                presenter.SetBorderAndTitleBar(false, false);
                presenter.IsAlwaysOnTop = true;
            }

            // Direct DWM Transient Window Acrylic attribute
            int backdropType = DWMSBT_TRANSIENTWINDOW;
            DwmSetWindowAttribute(_hWnd, DWMWA_SYSTEMBACKDROP_TYPE, ref backdropType, sizeof(int));

            // Strip caption, apply rounded corners and set dark/gray accent line
            ApplyBorderlessStyles();

            TrySetCustomAcrylic();

            this.Activated += OnWindowActivated;
        }

        private void ApplyBorderlessStyles()
        {
            // Strip caption, thick resizing frame and non-client border styles
            int style = GetWindowLong(_hWnd, GWL_STYLE);
            style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER | WS_DLGFRAME);
            style |= WS_POPUP;
            SetWindowLong(_hWnd, GWL_STYLE, style);

            // Tool window style (exclude from Alt+Tab)
            int exStyle = GetWindowLong(_hWnd, GWL_EXSTYLE);
            exStyle |= WS_EX_TOOLWINDOW;
            exStyle &= ~WS_EX_APPWINDOW;
            SetWindowLong(_hWnd, GWL_EXSTYLE, exStyle);

            SetWindowPos(_hWnd, IntPtr.Zero, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

            // Windows 11 Fluent rounded corners
            int cornerPref = DWMWCP_ROUND;
            DwmSetWindowAttribute(_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, ref cornerPref, sizeof(int));

            // Suppress harsh opaque DWM border so XAML translucent FlyoutBorderThemeBrush (reduced 50% opacity) renders smoothly
            int borderColor = unchecked((int)0xFFFFFFFE);
            DwmSetWindowAttribute(_hWnd, DWMWA_BORDER_COLOR, ref borderColor, sizeof(int));
        }

        private bool TrySetCustomAcrylic()
        {
            if (!DesktopAcrylicController.IsSupported())
                return false;

            _wsdqHelper = new WindowsSystemDispatcherQueueHelper();
            _wsdqHelper.EnsureWindowsSystemDispatcherQueueController();

            _configurationSource = new SystemBackdropConfiguration
            {
                IsInputActive = true
            };

            bool isDark = (Application.Current.RequestedTheme == ApplicationTheme.Dark);
            _configurationSource.Theme = isDark ? SystemBackdropTheme.Dark : SystemBackdropTheme.Light;

            _acrylicController = new DesktopAcrylicController
            {
                // Custom tuned acrylic opacities to eliminate the milky white wall
                TintColor = isDark ? Windows.UI.Color.FromArgb(255, 24, 24, 24) : Windows.UI.Color.FromArgb(255, 245, 245, 250),
                TintOpacity = isDark ? 0.45f : 0.30f, // 30% in light mode allows high frosted-glass transparency!
                LuminosityOpacity = 0.60f,
                FallbackColor = isDark ? Windows.UI.Color.FromArgb(255, 30, 30, 30) : Windows.UI.Color.FromArgb(255, 242, 242, 242)
            };

            var target = this.As<Microsoft.UI.Composition.ICompositionSupportsSystemBackdrop>();
            _acrylicController.AddSystemBackdropTarget(target);
            _acrylicController.SetSystemBackdropConfiguration(_configurationSource);
            return true;
        }

        private void OnWindowActivated(object sender, WindowActivatedEventArgs args)
        {
            if (args.WindowActivationState == WindowActivationState.Deactivated && _isShowing)
            {
                HideFlyout();
            }
            if (_configurationSource != null)
            {
                // Always keep IsInputActive = true to avoid falling back to solid opaque color
                _configurationSource.IsInputActive = true;
            }
        }

        public void Toggle()
        {
            if (_isShowing)
            {
                HideFlyout();
            }
            else
            {
                ShowAtTaskbar();
            }
        }

        public void ShowAtTaskbar()
        {
            UpdateDeviceList();

            // Determine target monitor and DPI scale factor
            GetCursorPos(out var mousePt);
            IntPtr hMonitor = MonitorFromPoint(mousePt, 2 /* MONITOR_DEFAULTTONEAREST */);
            MONITORINFO mi = new MONITORINFO();
            mi.cbSize = Marshal.SizeOf<MONITORINFO>();
            GetMonitorInfo(hMonitor, ref mi);

            double scale = 1.0;
            if (GetDpiForMonitor(hMonitor, 0 /* MDT_EFFECTIVE_DPI */, out uint dpiX, out _) == 0 && dpiX > 0)
            {
                scale = dpiX / 96.0;
            }
            else
            {
                uint winDpi = GetDpiForWindow(_hWnd);
                scale = (winDpi > 0 ? winDpi : 96) / 96.0;
            }

            int count = DeviceListPanel.Children.Count;
            // DIP dimensions
            int logicalWidth = 340;
            // Header (44) + cards (count * 88) + footer (44) + vertical padding
            int logicalHeight = Math.Clamp(48 + (Math.Max(1, count) * 88) + 44, 140, 560);

            // Convert DIP to physical pixels for AppWindow.MoveAndResize
            int physicalWidth = (int)Math.Round(logicalWidth * scale);
            int physicalHeight = (int)Math.Round(logicalHeight * scale);

            int targetX;
            if (NativeCore.RapooCore_GetBarRect(out var barRect) && barRect.Bottom > 0)
            {
                targetX = (barRect.Left + barRect.Right) / 2 - physicalWidth / 2;
            }
            else
            {
                targetX = mi.rcWork.Right - physicalWidth - (int)Math.Round(12 * scale);
            }

            // 8 DIP spacing from taskbar
            int targetY = mi.rcWork.Bottom - physicalHeight - (int)Math.Round(8 * scale);

            // Safe boundary checks
            int margin = (int)Math.Round(8 * scale);
            if (targetX + physicalWidth > mi.rcWork.Right - margin) targetX = mi.rcWork.Right - physicalWidth - margin;
            if (targetX < mi.rcWork.Left + margin) targetX = mi.rcWork.Left + margin;
            if (targetY < mi.rcWork.Top + margin) targetY = mi.rcWork.Top + margin;

            _appWindow.MoveAndResize(new RectInt32(targetX, targetY, physicalWidth, physicalHeight));

            if (_configurationSource != null)
            {
                _configurationSource.IsInputActive = true;
            }

            // Reset animation state
            WindowTranslateTransform.Y = 20;
            RootBorder.Opacity = 0.0;

            ApplyBorderlessStyles();
            this.Activate();
            ApplyBorderlessStyles();
            _isShowing = true;

            SlideUpStoryboard?.Begin();
        }

        public void HideFlyout()
        {
            if (!_isShowing) return;
            _isShowing = false;
            SlideUpStoryboard?.Stop();
            RootBorder.Opacity = 0.0;
            _appWindow.Hide();
        }

        public void RefreshState()
        {
            if (_isShowing)
            {
                UpdateDeviceList();
            }
        }

        private void UpdateDeviceList()
        {
            DeviceListPanel.Children.Clear();
            var devices = NativeCore.GetAllDevices();

            if (devices.Count == 0)
            {
                var emptyCard = new Border
                {
                    Background = GetBrush("CardBackgroundFillColorDefaultBrush", Windows.UI.Color.FromArgb(200, 255, 255, 255)),
                    BorderThickness = new Thickness(0),
                    CornerRadius = new CornerRadius(8),
                    Padding = new Thickness(18),
                    Child = new TextBlock
                    {
                        Text = "未检测到雷柏外设\n请确认 2.4G 接收器已插入或设备已开机",
                        TextAlignment = TextAlignment.Center,
                        Foreground = GetBrush("TextFillColorSecondaryBrush", Windows.UI.Color.FromArgb(180, 128, 128, 128)),
                        FontSize = 12
                    }
                };
                DeviceListPanel.Children.Add(emptyCard);
                return;
            }

            foreach (var dev in devices)
            {
                DeviceListPanel.Children.Add(CreateDeviceCard(dev));
            }
        }

        private UIElement CreateDeviceCard(DeviceModel dev)
        {
            var cardBorder = new Border
            {
                Background = GetBrush("CardBackgroundFillColorDefaultBrush", Windows.UI.Color.FromArgb(220, 255, 255, 255)),
                BorderBrush = GetBrush("CardStrokeColorDefaultBrush", Windows.UI.Color.FromArgb(30, 128, 128, 128)),
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(6),
                Padding = new Thickness(14, 11, 14, 11)
            };

            var rootGrid = new Grid();
            rootGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            rootGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

            // Row 0: Icon + Name + Battery Text
            var headerGrid = new Grid();
            headerGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            headerGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            headerGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

            var icon = new FontIcon
            {
                Glyph = dev.CategoryGlyph,
                FontSize = 18,
                VerticalAlignment = VerticalAlignment.Center,
                Margin = new Thickness(0, 0, 10, 0),
                Foreground = GetBrush("AccentTextFillColorPrimaryBrush", Windows.UI.Color.FromArgb(255, 0, 120, 215))
            };
            Grid.SetColumn(icon, 0);
            headerGrid.Children.Add(icon);

            var namePanel = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
            var nameText = new TextBlock
            {
                Text = dev.ModelName,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                FontSize = 13,
                TextTrimming = TextTrimming.CharacterEllipsis
            };
            namePanel.Children.Add(nameText);

            var subText = new TextBlock
            {
                Text = dev.IsConnected ? dev.ConnType : "未连接",
                FontSize = 11,
                Foreground = GetBrush("TextFillColorTertiaryBrush", Windows.UI.Color.FromArgb(120, 128, 128, 128))
            };
            namePanel.Children.Add(subText);

            Grid.SetColumn(namePanel, 1);
            headerGrid.Children.Add(namePanel);

            var batPanel = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Spacing = 4,
                VerticalAlignment = VerticalAlignment.Center
            };

            if (dev.IsCharging)
            {
                batPanel.Children.Add(new FontIcon
                {
                    Glyph = "\uEA80", // Lightning bolt
                    FontSize = 13,
                    Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 255, 185, 0))
                });
            }

            batPanel.Children.Add(new TextBlock
            {
                Text = dev.HasBattery ? $"{dev.Battery}%" : "--",
                FontSize = 13,
                FontWeight = Microsoft.UI.Text.FontWeights.Medium,
                VerticalAlignment = VerticalAlignment.Center
            });

            Grid.SetColumn(batPanel, 2);
            headerGrid.Children.Add(batPanel);

            Grid.SetRow(headerGrid, 0);
            rootGrid.Children.Add(headerGrid);

            // Row 1: Progress Bar + Badges (DPI, Polling)
            if (dev.HasBattery || dev.HasDpi || dev.HasPolling)
            {
                var detailsPanel = new StackPanel { Spacing = 6, Margin = new Thickness(0, 8, 0, 0) };

                if (dev.HasBattery)
                {
                    var pbar = new ProgressBar
                    {
                        Value = dev.Battery,
                        Maximum = 100,
                        Height = 4,
                        HorizontalAlignment = HorizontalAlignment.Stretch
                    };

                    if (dev.Battery <= 15)
                    {
                        pbar.Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 232, 17, 35)); // Red
                    }
                    else if (dev.Battery <= 30)
                    {
                        pbar.Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 255, 185, 0)); // Amber
                    }

                    detailsPanel.Children.Add(pbar);
                }

                if (dev.HasDpi || dev.HasPolling)
                {
                    var badgesPanel = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Margin = new Thickness(0, 2, 0, 0) };

                    if (dev.HasDpi)
                    {
                        badgesPanel.Children.Add(CreateBadge($"DPI {dev.DpiX}"));
                    }

                    if (dev.HasPolling)
                    {
                        badgesPanel.Children.Add(CreateBadge($"{dev.PollingHz} Hz"));
                    }

                    detailsPanel.Children.Add(badgesPanel);
                }

                Grid.SetRow(detailsPanel, 1);
                rootGrid.Children.Add(detailsPanel);
            }

            cardBorder.Child = rootGrid;
            return cardBorder;
        }

        private Border CreateBadge(string text)
        {
            return new Border
            {
                Background = GetBrush("SubtleFillColorSecondaryBrush", Windows.UI.Color.FromArgb(20, 128, 128, 128)),
                CornerRadius = new CornerRadius(4),
                Padding = new Thickness(6, 2, 6, 2),
                Child = new TextBlock
                {
                    Text = text,
                    FontSize = 10,
                    Foreground = GetBrush("TextFillColorSecondaryBrush", Windows.UI.Color.FromArgb(180, 128, 128, 128))
                }
            };
        }

        private void OnRefreshClicked(object sender, RoutedEventArgs e)
        {
            NativeCore.RapooCore_Refresh();
            UpdateDeviceList();
        }

        private void OnOpenSettingsClicked(object sender, RoutedEventArgs e)
        {
            HideFlyout();
            App.ShowSettingsWindow();
        }
    }
}
