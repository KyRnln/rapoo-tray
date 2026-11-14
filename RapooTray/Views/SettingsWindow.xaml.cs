using System;
using System.Collections.Generic;
using Microsoft.UI.Composition.SystemBackdrops;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.Win32;
using RapooTray.Models;
using RapooTray.Services;
using Windows.Graphics;

namespace RapooTray.Views
{
    public sealed partial class SettingsWindow : Window
    {
        private AppWindow _appWindow;
        private const string RunRegKey = @"Software\Microsoft\Windows\CurrentVersion\Run";
        private const string AppRunName = "RapooTray";

        public SettingsWindow()
        {
            this.InitializeComponent();

            this.SystemBackdrop = new MicaBackdrop() { Kind = MicaKind.Base };

            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            var windowId = Microsoft.UI.Win32Interop.GetWindowIdFromWindow(hWnd);
            _appWindow = AppWindow.GetFromWindowId(windowId);

            _appWindow.Resize(new SizeInt32(960, 680));
            this.Title = "Rapoo Tray 设置";

            NavView.SelectedItem = NavView.MenuItems[0];
            LoadPersonalizationPage();
        }

        private void OnNavSelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
        {
            if (args.SelectedItem is NavigationViewItem item)
            {
                string tag = item.Tag?.ToString() ?? "";
                switch (tag)
                {
                    case "Personalization":
                        LoadPersonalizationPage();
                        break;
                    case "Devices":
                        LoadDevicesPage();
                        break;
                    case "General":
                        LoadGeneralPage();
                        break;
                    case "About":
                        LoadAboutPage();
                        break;
                }
            }
        }

        private void LoadPersonalizationPage()
        {
            ContentArea.Children.Clear();

            AddHeader("个性化设置", "自定义任务栏常驻进度条的外观、尺寸与显示模式");

            // 1. 常驻进度条开关
            var toggleBar = new ToggleSwitch
            {
                IsOn = NativeCore.RapooCore_IsBarEnabled(),
                OnContent = "开启",
                OffContent = "关闭"
            };
            toggleBar.Toggled += (s, e) =>
            {
                NativeCore.RapooCore_SetBarEnabled(toggleBar.IsOn);
                SaveRegDword("ShowTaskbarBar", toggleBar.IsOn ? 1 : 0);
            };
            ContentArea.Children.Add(CreateSettingCard("显示任务栏常驻进度条", "在 Windows 11 任务栏上显示当前置顶外设的电量进度条", toggleBar));

            // 2. 停靠位置
            var posCombo = new ComboBox
            {
                Width = 160,
                SelectedIndex = Math.Clamp(NativeCore.RapooCore_GetBarPosition(), 0, 2)
            };
            posCombo.Items.Add("靠左侧 (小组件)");
            posCombo.Items.Add("居中空白区域");
            posCombo.Items.Add("靠右侧 (系统托盘)");
            posCombo.SelectionChanged += (s, e) =>
            {
                NativeCore.RapooCore_SetBarPosition(posCombo.SelectedIndex);
                SaveRegDword("TaskbarBarPosition", posCombo.SelectedIndex);
            };
            ContentArea.Children.Add(CreateSettingCard("常驻进度条位置", "设置常驻进度条在 Windows 11 任务栏中的停靠位置", posCombo));

            // 3. 宽度调节
            int curWidth = NativeCore.RapooCore_GetBarWidth();
            var widthSlider = new Slider
            {
                Minimum = 100,
                Maximum = 280,
                StepFrequency = 5,
                Value = curWidth > 0 ? curWidth : 140,
                Width = 200
            };
            var widthValueText = new TextBlock { Text = $"{widthSlider.Value} px", VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(10, 0, 0, 0) };
            var widthPanel = new StackPanel { Orientation = Orientation.Horizontal };
            widthPanel.Children.Add(widthSlider);
            widthPanel.Children.Add(widthValueText);

            widthSlider.ValueChanged += (s, e) =>
            {
                int val = (int)e.NewValue;
                widthValueText.Text = $"{val} px";
                NativeCore.RapooCore_SetBarWidth(val);
                SaveRegDword("TaskbarBarWidth", val);
            };
            ContentArea.Children.Add(CreateSettingCard("进度条宽度调节", "调整常驻进度条的总宽度 (100 ~ 280 像素)", widthPanel));

            // 4. 设备图标大小
            int curIconSize = NativeCore.RapooCore_GetBarIconSize();
            var iconSizeSlider = new Slider
            {
                Minimum = 12,
                Maximum = 28,
                StepFrequency = 1,
                Value = curIconSize > 0 ? curIconSize : 18,
                Width = 200
            };
            var iconSizeText = new TextBlock { Text = $"{iconSizeSlider.Value} px", VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(10, 0, 0, 0) };
            var iconSizePanel = new StackPanel { Orientation = Orientation.Horizontal };
            iconSizePanel.Children.Add(iconSizeSlider);
            iconSizePanel.Children.Add(iconSizeText);

            iconSizeSlider.ValueChanged += (s, e) =>
            {
                int val = (int)e.NewValue;
                iconSizeText.Text = $"{val} px";
                NativeCore.RapooCore_SetBarIconSize(val);
                SaveRegDword("TaskbarBarIconSize", val);
            };
            ContentArea.Children.Add(CreateSettingCard("设备图标大小", "调整常驻进度条右侧/左侧设备图标的尺寸 (12 ~ 28 像素)", iconSizePanel));

            // 5. 设备图标上下偏移
            int curOffsetY = NativeCore.RapooCore_GetBarIconOffsetY();
            var offsetSlider = new Slider
            {
                Minimum = -8,
                Maximum = 8,
                StepFrequency = 1,
                Value = curOffsetY,
                Width = 200
            };
            var offsetText = new TextBlock { Text = $"{offsetSlider.Value} px", VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(10, 0, 0, 0) };
            var offsetPanel = new StackPanel { Orientation = Orientation.Horizontal };
            offsetPanel.Children.Add(offsetSlider);
            offsetPanel.Children.Add(offsetText);

            offsetSlider.ValueChanged += (s, e) =>
            {
                int val = (int)e.NewValue;
                offsetText.Text = $"{val} px";
                NativeCore.RapooCore_SetBarIconOffsetY(val);
                SaveRegDword("TaskbarBarIconOffsetY", val);
            };
            ContentArea.Children.Add(CreateSettingCard("设备图标上下偏移", "微调设备图标的垂直居中位置 (-8 ~ +8 像素)", offsetPanel));

            // 6. 字体文本大小
            int curFontSize = NativeCore.RapooCore_GetBarFontSize();
            var fontSizeSlider = new Slider
            {
                Minimum = 9,
                Maximum = 16,
                StepFrequency = 1,
                Value = curFontSize > 0 ? curFontSize : 11,
                Width = 200
            };
            var fontSizeText = new TextBlock { Text = $"{fontSizeSlider.Value} pt", VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(10, 0, 0, 0) };
            var fontSizePanel = new StackPanel { Orientation = Orientation.Horizontal };
            fontSizePanel.Children.Add(fontSizeSlider);
            fontSizePanel.Children.Add(fontSizeText);

            fontSizeSlider.ValueChanged += (s, e) =>
            {
                int val = (int)e.NewValue;
                fontSizeText.Text = $"{val} pt";
                NativeCore.RapooCore_SetBarFontSize(val);
                SaveRegDword("TaskbarBarFontSize", val);
            };
            ContentArea.Children.Add(CreateSettingCard("字体文本大小", "调整常驻进度条上方设备名称与电量文本的字号 (9 ~ 16 磅)", fontSizePanel));

            // 7. 文本与进度条上下间距
            int curSpacing = NativeCore.RapooCore_GetBarSpacing();
            var spacingSlider = new Slider
            {
                Minimum = 0,
                Maximum = 8,
                StepFrequency = 1,
                Value = curSpacing >= 0 ? curSpacing : 2,
                Width = 200
            };
            var spacingText = new TextBlock { Text = $"{spacingSlider.Value} px", VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(10, 0, 0, 0) };
            var spacingPanel = new StackPanel { Orientation = Orientation.Horizontal };
            spacingPanel.Children.Add(spacingSlider);
            spacingPanel.Children.Add(spacingText);

            spacingSlider.ValueChanged += (s, e) =>
            {
                int val = (int)e.NewValue;
                spacingText.Text = $"{val} px";
                NativeCore.RapooCore_SetBarSpacing(val);
                SaveRegDword("TaskbarBarSpacing", val);
            };
            ContentArea.Children.Add(CreateSettingCard("进度条上下间距", "调整文字行与下方电量进度条之间的垂直间距 (0 ~ 8 像素)", spacingPanel));

            // 8. 设备图标颜色
            var iconColorCombo = new ComboBox
            {
                Width = 160,
                SelectedIndex = Math.Clamp(NativeCore.RapooCore_GetBarIconColorMode(), 0, 2)
            };
            iconColorCombo.Items.Add("跟随系统主题 (默认)");
            iconColorCombo.Items.Add("纯黑色");
            iconColorCombo.Items.Add("系统强调色");
            iconColorCombo.SelectionChanged += (s, e) =>
            {
                NativeCore.RapooCore_SetBarIconColorMode(iconColorCombo.SelectedIndex);
                SaveRegDword("TaskbarBarIconColor", iconColorCombo.SelectedIndex);
            };
            ContentArea.Children.Add(CreateSettingCard("设备图标颜色", "设置常驻进度条右侧设备图标的色彩样式", iconColorCombo));

            // 9. 进度条电量颜色
            var barColorCombo = new ComboBox
            {
                Width = 160,
                SelectedIndex = Math.Clamp(NativeCore.RapooCore_GetBarColorMode(), 0, 2)
            };
            barColorCombo.Items.Add("跟随系统主题 (默认)");
            barColorCombo.Items.Add("纯黑色");
            barColorCombo.Items.Add("系统强调色");
            barColorCombo.SelectionChanged += (s, e) =>
            {
                NativeCore.RapooCore_SetBarColorMode(barColorCombo.SelectedIndex);
                SaveRegDword("TaskbarBarColor", barColorCombo.SelectedIndex);
            };
            ContentArea.Children.Add(CreateSettingCard("进度条电量颜色", "设置电量填充进度条的色彩模式", barColorCombo));
        }

        private void LoadDevicesPage()
        {
            ContentArea.Children.Clear();

            AddHeader("设备管理", "管理当前已连接的雷柏外设、自定义别名或置顶首选设备");

            var devices = NativeCore.GetAllDevices();
            if (devices.Count == 0)
            {
                var emptyCard = new Border
                {
                    Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
                    CornerRadius = new CornerRadius(6),
                    Padding = new Thickness(24),
                    Child = new TextBlock
                    {
                        Text = "当前未检测到任何雷柏外设。请确认无线接收器已插入或设备已处于开机连接状态。",
                        HorizontalAlignment = HorizontalAlignment.Center,
                        Foreground = (Brush)Application.Current.Resources["TextFillColorSecondaryBrush"]
                    }
                };
                ContentArea.Children.Add(emptyCard);
                return;
            }

            foreach (var dev in devices)
            {
                var card = new Border
                {
                    Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
                    BorderBrush = (Brush)Application.Current.Resources["CardStrokeColorDefaultBrush"],
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(6),
                    Padding = new Thickness(16, 12, 16, 12)
                };

                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                var icon = new FontIcon
                {
                    Glyph = dev.CategoryGlyph,
                    FontSize = 24,
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(0, 0, 16, 0),
                    Foreground = (Brush)Application.Current.Resources["AccentTextFillColorPrimaryBrush"]
                };
                Grid.SetColumn(icon, 0);
                grid.Children.Add(icon);

                var infoPanel = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Spacing = 4 };
                var nameBox = new TextBox
                {
                    Text = dev.ModelName,
                    Header = "设备显示名称 (可自定义)",
                    Width = 260,
                    HorizontalAlignment = HorizontalAlignment.Left
                };
                infoPanel.Children.Add(nameBox);

                var subInfo = new TextBlock
                {
                    Text = $"{dev.BrandName} · {dev.ConnType} · 电量: {dev.Battery}%",
                    FontSize = 11,
                    Foreground = (Brush)Application.Current.Resources["TextFillColorSecondaryBrush"]
                };
                infoPanel.Children.Add(subInfo);

                Grid.SetColumn(infoPanel, 1);
                grid.Children.Add(infoPanel);

                var actionPanel = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, VerticalAlignment = VerticalAlignment.Center };

                var saveBtn = new Button { Content = "保存名称" };
                saveBtn.Click += (s, e) =>
                {
                    NativeCore.RapooCore_SetCustomName(dev.Id, nameBox.Text);
                };
                actionPanel.Children.Add(saveBtn);

                var pinBtn = new Button
                {
                    Content = dev.IsPinned ? "已置顶" : "置顶",
                    IsEnabled = !dev.IsPinned
                };
                pinBtn.Click += (s, e) =>
                {
                    NativeCore.RapooCore_SetPinned(dev.Id);
                    LoadDevicesPage();
                };
                actionPanel.Children.Add(pinBtn);

                Grid.SetColumn(actionPanel, 2);
                grid.Children.Add(actionPanel);

                card.Child = grid;
                ContentArea.Children.Add(card);
            }
        }

        private void LoadGeneralPage()
        {
            ContentArea.Children.Clear();

            AddHeader("常规与自启", "管理应用程序的系统集成与运行偏好");

            // 开机自启动
            bool isAutoStart = CheckAutoStart();
            var autoStartToggle = new ToggleSwitch
            {
                IsOn = isAutoStart,
                OnContent = "开启",
                OffContent = "关闭"
            };
            autoStartToggle.Toggled += (s, e) =>
            {
                SetAutoStart(autoStartToggle.IsOn);
            };
            ContentArea.Children.Add(CreateSettingCard("开机自动启动", "登录 Windows 时自动在后台启动 Rapoo Tray 托盘监控", autoStartToggle));

            // 退出应用
            var exitBtn = new Button
            {
                Content = "退出程序",
                Style = (Style)Application.Current.Resources["DefaultButtonStyle"]
            };
            exitBtn.Click += (s, e) =>
            {
                App.ExitApplication();
            };
            ContentArea.Children.Add(CreateSettingCard("退出 Rapoo Tray", "完全退出外设电量监控托盘程序", exitBtn));
        }

        private void LoadAboutPage()
        {
            ContentArea.Children.Clear();

            AddHeader("关于 Rapoo Tray", "适用于 Windows 11 的现代化雷柏外设电量监控工具");

            var aboutCard = new Border
            {
                Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
                BorderBrush = (Brush)Application.Current.Resources["CardStrokeColorDefaultBrush"],
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(8),
                Padding = new Thickness(24)
            };

            var stack = new StackPanel { Spacing = 14 };

            stack.Children.Add(new TextBlock
            {
                Text = "Rapoo Tray 外设电量监控",
                FontSize = 18,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold
            });

            stack.Children.Add(new TextBlock
            {
                Text = "版本: 2.0.0 (WinUI 3 官方云母架构重构版)\n开发架构: Windows App SDK (WinUI 3) + .NET 8 + Native C++ HID Telemetry Core",
                Foreground = (Brush)Application.Current.Resources["TextFillColorSecondaryBrush"],
                FontSize = 13,
                LineHeight = 20
            });

            stack.Children.Add(new TextBlock
            {
                Text = "特性亮点:\n• 100% 官方 Windows 11 云母 (Mica) 与亚克力 (Acrylic) 材质，告别白底与闪烁\n• Windows 11 任务栏常驻电量指示，随心调节宽度、位置、字号与间距\n• 多设备智能分类显示，支持 DPI、回报率与充电状态实时展现",
                Foreground = (Brush)Application.Current.Resources["TextFillColorTertiaryBrush"],
                FontSize = 12,
                LineHeight = 18
            });

            aboutCard.Child = stack;
            ContentArea.Children.Add(aboutCard);
        }

        private void AddHeader(string title, string subtitle)
        {
            var headerStack = new StackPanel { Spacing = 4, Margin = new Thickness(0, 0, 0, 8) };
            headerStack.Children.Add(new TextBlock
            {
                Text = title,
                FontSize = 24,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold
            });
            headerStack.Children.Add(new TextBlock
            {
                Text = subtitle,
                FontSize = 13,
                Foreground = (Brush)Application.Current.Resources["TextFillColorSecondaryBrush"]
            });
            ContentArea.Children.Add(headerStack);
        }

        private Border CreateSettingCard(string title, string description, FrameworkElement actionControl)
        {
            var border = new Border
            {
                Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
                BorderBrush = (Brush)Application.Current.Resources["CardStrokeColorDefaultBrush"],
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(6),
                Padding = new Thickness(16, 12, 16, 12)
            };

            var grid = new Grid();
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

            var textPanel = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Spacing = 2 };
            textPanel.Children.Add(new TextBlock
            {
                Text = title,
                FontSize = 13,
                FontWeight = Microsoft.UI.Text.FontWeights.Medium
            });
            if (!string.IsNullOrEmpty(description))
            {
                textPanel.Children.Add(new TextBlock
                {
                    Text = description,
                    FontSize = 11,
                    Foreground = (Brush)Application.Current.Resources["TextFillColorSecondaryBrush"]
                });
            }

            Grid.SetColumn(textPanel, 0);
            grid.Children.Add(textPanel);

            Grid.SetColumn(actionControl, 1);
            grid.Children.Add(actionControl);

            border.Child = grid;
            return border;
        }

        private void SaveRegDword(string name, int value)
        {
            try
            {
                using var key = Registry.CurrentUser.CreateSubKey(@"Software\rapoo-tray");
                key?.SetValue(name, value, RegistryValueKind.DWord);
            }
            catch { }
        }

        private bool CheckAutoStart()
        {
            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(RunRegKey, false);
                return key?.GetValue(AppRunName) != null;
            }
            catch
            {
                return false;
            }
        }

        private void SetAutoStart(bool enable)
        {
            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(RunRegKey, true);
                if (key != null)
                {
                    if (enable)
                    {
                        string exePath = Environment.ProcessPath ?? "";
                        key.SetValue(AppRunName, $"\"{exePath}\"");
                    }
                    else
                    {
                        key.DeleteValue(AppRunName, false);
                    }
                }
            }
            catch { }
        }
    }
}
