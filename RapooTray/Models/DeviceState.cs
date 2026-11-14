using System;
using System.Runtime.InteropServices;

namespace RapooTray.Models
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 1)]
    public struct DeviceStateExport
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string Id;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string ModelName;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string DefaultModelName;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string BrandName;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)]
        public string Category;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)]
        public string ConnType;

        [MarshalAs(UnmanagedType.I1)]
        public bool IsConnected;

        [MarshalAs(UnmanagedType.I1)]
        public bool IsWired;

        [MarshalAs(UnmanagedType.I1)]
        public bool IsCharging;

        [MarshalAs(UnmanagedType.I1)]
        public bool HasBattery;

        public int Battery;

        [MarshalAs(UnmanagedType.I1)]
        public bool HasDpi;

        public int DpiLevel;
        public int DpiX;
        public int DpiY;

        [MarshalAs(UnmanagedType.I1)]
        public bool HasPolling;

        public int PollingHz;

        [MarshalAs(UnmanagedType.I1)]
        public bool IsPinned;
    }

    public class DeviceModel
    {
        public string Id { get; set; } = string.Empty;
        public string ModelName { get; set; } = "雷柏无线外设";
        public string DefaultModelName { get; set; } = string.Empty;
        public string BrandName { get; set; } = "Rapoo";
        public string Category { get; set; } = "mouse";
        public string ConnType { get; set; } = "2.4G 无线";
        public bool IsConnected { get; set; } = true;
        public bool IsWired { get; set; } = false;
        public bool IsCharging { get; set; } = false;
        public bool HasBattery { get; set; } = true;
        public int Battery { get; set; } = 100;
        public bool HasDpi { get; set; } = false;
        public int DpiLevel { get; set; } = 1;
        public int DpiX { get; set; } = 800;
        public int DpiY { get; set; } = 800;
        public bool HasPolling { get; set; } = false;
        public int PollingHz { get; set; } = 1000;
        public bool IsPinned { get; set; } = false;

        public string BatteryPercentText => $"{Battery}%";
        
        public string CategoryGlyph => Category?.ToLowerInvariant() switch
        {
            "keyboard" => "\uE765", // Keyboard
            "headset" => "\uE7F6",  // Headset
            "gamepad" => "\uE7FC",  // Gamepad
            _ => "\uE962"           // Mouse
        };

        public string ConnGlyph => IsWired ? "\uE8A7" : "\uE701";

        public string DpiText => HasDpi ? $"DPI: {DpiX}" : string.Empty;
        public string PollingText => HasPolling ? $"{PollingHz} Hz" : string.Empty;

        public static DeviceModel FromExport(DeviceStateExport exp)
        {
            return new DeviceModel
            {
                Id = exp.Id ?? string.Empty,
                ModelName = !string.IsNullOrWhiteSpace(exp.ModelName) ? exp.ModelName : "雷柏外设",
                DefaultModelName = exp.DefaultModelName ?? string.Empty,
                BrandName = exp.BrandName ?? "Rapoo",
                Category = exp.Category ?? "mouse",
                ConnType = exp.ConnType ?? "2.4G 无线",
                IsConnected = exp.IsConnected,
                IsWired = exp.IsWired,
                IsCharging = exp.IsCharging,
                HasBattery = exp.HasBattery,
                Battery = Math.Clamp(exp.Battery, 0, 100),
                HasDpi = exp.HasDpi,
                DpiLevel = exp.DpiLevel,
                DpiX = exp.DpiX,
                DpiY = exp.DpiY,
                HasPolling = exp.HasPolling,
                PollingHz = exp.PollingHz,
                IsPinned = exp.IsPinned
            };
        }
    }
}
