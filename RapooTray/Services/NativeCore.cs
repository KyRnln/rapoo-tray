using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using RapooTray.Models;

namespace RapooTray.Services
{
    public static class NativeCore
    {
        private const string DllName = "rapoo_core.dll";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void CoreStateChangeCallback();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void CoreBarClickCallback();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void CoreTrayRightClickCallback(int x, int y);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool RapooCore_Init();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_Cleanup();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetCallbacks(
            CoreStateChangeCallback onStateChange,
            CoreBarClickCallback onBarClick,
            CoreTrayRightClickCallback onTrayRightClick
        );

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RapooCore_GetDeviceCount();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool RapooCore_GetDevice(int index, out DeviceStateExport outState);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool RapooCore_GetPrimaryDevice(out DeviceStateExport outState);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_Refresh();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void RapooCore_SetPinned(string deviceId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void RapooCore_SetCustomName(string deviceId, string customName);

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool RapooCore_GetBarRect(out RECT outRect);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_UpdateBar();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetBarEnabled([MarshalAs(UnmanagedType.I1)] bool enabled);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool RapooCore_IsBarEnabled();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetBarPosition(int pos);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RapooCore_GetBarPosition();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetBarWidth(int width);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RapooCore_GetBarWidth();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetBarSpacing(int spacing);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RapooCore_GetBarSpacing();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetBarIconSize(int size);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RapooCore_GetBarIconSize();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetBarIconOffsetY(int offset);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RapooCore_GetBarIconOffsetY();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetBarFontSize(int size);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RapooCore_GetBarFontSize();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetBarIconColorMode(int mode);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RapooCore_GetBarIconColorMode();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RapooCore_SetBarColorMode(int mode);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RapooCore_GetBarColorMode();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void RapooCore_UpdateTrayTooltip(string tip);

        public static List<DeviceModel> GetAllDevices()
        {
            var list = new List<DeviceModel>();
            int count = RapooCore_GetDeviceCount();
            for (int i = 0; i < count; i++)
            {
                if (RapooCore_GetDevice(i, out var exp))
                {
                    list.Add(DeviceModel.FromExport(exp));
                }
            }
            return list;
        }
    }
}
