using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Microsoft.UI;
using Microsoft.UI.Composition.SystemBackdrops;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using RapooTray.Models;
using RapooTray.Services;
using Windows.Graphics;

namespace RapooTray.Views
{
    public sealed partial class FlyoutWindow : Window
    {
        private AppWindow _appWindow;
        private IntPtr _hWnd;
        private bool _isShowing = false;

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

        private const int GWL_EXSTYLE = -20;
        private const int WS_EX_TOOLWINDOW = 0x00000080;
        private const int WS_EX_APPWINDOW = 0x00040000;
        private const int DWMWA_WINDOW_CORNER_PREFERENCE = 33;
        private const int DWMWCP_ROUND = 2;

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

            this.SystemBackdrop = new MicaBackdrop() { Kind = MicaKind.Base };
            this.ExtendsContentIntoTitleBar = true;

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

            // Windows 11 Rounded corners
            int cornerPref = DWMWCP_ROUND;
            DwmSetWindowAttribute(_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, ref cornerPref, sizeof(int));

            // Tool window style (exclude from Alt+Tab)
            int exStyle = GetWindowLong(_hWnd, GWL_EXSTYLE);
            exStyle |= WS_EX_TOOLWINDOW;
            exStyle &= ~WS_EX_APPWINDOW;
            SetWindowLong(_hWnd, GWL_EXSTYLE, exStyle);

            this.Activated += OnWindowActivated;
        }

        private void OnWindowActivated(object sender, WindowActivatedEventArgs args)
        {
            if (args.WindowActivationState == WindowActivationState.Deactivated && _isShowing)
            {
                HideFlyout();
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

            int cardWidth = 320;
            int cardHeight = Math.Clamp(60 + (DeviceListPanel.Children.Count * 90) + 48, 120, 520);

            // Compute Coordinates right above the taskbar bar
            GetCursorPos(out var mousePt);
            IntPtr hMonitor = MonitorFromPoint(mousePt, 2 /* MONITOR_DEFAULTTONEAREST */);
            MONITORINFO mi = new MONITORINFO();
            mi.cbSize = Marshal.SizeOf<MONITORINFO>();
            GetMonitorInfo(hMonitor, ref mi);

            int targetX;
            if (NativeCore.RapooCore_GetBarRect(out var barRect) && barRect.Bottom > 0)
            {
                targetX = (barRect.Left + barRect.Right) / 2 - cardWidth / 2;
            }
            else
            {
                targetX = mi.rcWork.Right - cardWidth - 12;
            }

            // Spacing from taskbar matching right click menu (8px above workArea.Bottom)
            int targetY = mi.rcWork.Bottom - cardHeight - 8;

            if (targetX + cardWidth > mi.rcWork.Right - 8) targetX = mi.rcWork.Right - cardWidth - 8;
            if (targetX < mi.rcWork.Left + 8) targetX = mi.rcWork.Left + 8;
            if (targetY < mi.rcWork.Top + 8) targetY = mi.rcWork.Top + 8;

            _appWindow.MoveAndResize(new RectInt32(targetX, targetY, cardWidth, cardHeight));
            this.Activate();
            _isShowing = true;
            SlideUpStoryboard?.Begin();
        }

        public void HideFlyout()
        {
            if (!_isShowing) return;
            _isShowing = false;
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
                    Background = GetBrush("CardBackgroundFillColorDefaultBrush", Windows.UI.Color.FromArgb(20, 128, 128, 128)),
                    CornerRadius = new CornerRadius(6),
                    Padding = new Thickness(16),
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
                Background = GetBrush("CardBackgroundFillColorDefaultBrush", Windows.UI.Color.FromArgb(20, 128, 128, 128)),
                BorderBrush = GetBrush("CardStrokeColorDefaultBrush", Windows.UI.Color.FromArgb(30, 128, 128, 128)),
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(6),
                Padding = new Thickness(12, 10, 12, 10)
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
