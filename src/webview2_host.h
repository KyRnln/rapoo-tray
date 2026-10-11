#pragma once
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <strsafe.h>
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace WebView2 {

struct EventRegistrationToken {
    int64_t value;
};

struct COREWEBVIEW2_COLOR {
    BYTE A;
    BYTE R;
    BYTE G;
    BYTE B;
};

static const IID IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler =
    { 0x4E8A3389, 0xC9D8, 0x4BD2, { 0xB6, 0xB5, 0x12, 0x4F, 0xEE, 0x6C, 0xC1, 0x4D } };

static const IID IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler =
    { 0x6C4819F3, 0xC9B7, 0x4260, { 0x81, 0x27, 0xC9, 0xF5, 0xBD, 0xE7, 0xF6, 0x8C } };

static const IID IID_ICoreWebView2WebMessageReceivedEventHandler =
    { 0x57213F19, 0x00E6, 0x49FA, { 0x8E, 0x07, 0x89, 0x8E, 0xA0, 0x1E, 0xCB, 0xD2 } };

static const IID IID_ICoreWebView2NavigationCompletedEventHandler =
    { 0xD33A35BF, 0x1C49, 0x4F98, { 0x93, 0xAB, 0x00, 0x6E, 0x05, 0x33, 0xFE, 0x1C } };

static const IID IID_ICoreWebView2Controller2 =
    { 0xC979903E, 0xD4CA, 0x4228, { 0x92, 0xEB, 0x47, 0xEE, 0x3F, 0xA9, 0x6E, 0xAB } };

static const IID IID_ICoreWebView2Settings3 =
    { 0xFDB5AB74, 0xAF33, 0x4854, { 0x84, 0xF0, 0x0A, 0x63, 0x1D, 0xEB, 0x5E, 0xBA } };

struct ICoreWebView2Environment;
struct ICoreWebView2Controller;
struct ICoreWebView2;
struct ICoreWebView2Settings;
struct ICoreWebView2WebMessageReceivedEventArgs;
struct ICoreWebView2NavigationCompletedEventArgs;

MIDL_INTERFACE("4E8A3389-C9D8-4BD2-B6B5-124FEE6CC14D")
ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* createdEnvironment) = 0;
};

MIDL_INTERFACE("6C4819F3-C9B7-4260-8127-C9F5BDE7F68C")
ICoreWebView2CreateCoreWebView2ControllerCompletedHandler : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* createdController) = 0;
};

MIDL_INTERFACE("0F99A40C-E962-4207-9E92-E3D542EFF849")
ICoreWebView2WebMessageReceivedEventArgs : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE get_Source(LPWSTR* source) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_WebMessageAsJson(LPWSTR* webMessageAsJson) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryGetWebMessageAsString(LPWSTR* webMessageAsString) = 0;
};

MIDL_INTERFACE("57213F19-00E6-49FA-8E07-898EA01ECBD2")
ICoreWebView2WebMessageReceivedEventHandler : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) = 0;
};

MIDL_INTERFACE("D33A35BF-1C49-4F98-93AB-006E0533FE1C")
ICoreWebView2NavigationCompletedEventHandler : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) = 0;
};

MIDL_INTERFACE("E562E4F0-D7FA-43AC-8D71-C05150499F00")
ICoreWebView2Settings : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE get_IsScriptEnabled(BOOL* isScriptEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsScriptEnabled(BOOL isScriptEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsWebMessageEnabled(BOOL* isWebMessageEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsWebMessageEnabled(BOOL isWebMessageEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AreDefaultScriptDialogsEnabled(BOOL* areDefaultScriptDialogsEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AreDefaultScriptDialogsEnabled(BOOL areDefaultScriptDialogsEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsStatusBarEnabled(BOOL* isStatusBarEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsStatusBarEnabled(BOOL isStatusBarEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AreDevToolsEnabled(BOOL* areDevToolsEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AreDevToolsEnabled(BOOL areDevToolsEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AreDefaultContextMenusEnabled(BOOL* enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AreDefaultContextMenusEnabled(BOOL enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AreHostObjectsAllowed(BOOL* allowed) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AreHostObjectsAllowed(BOOL allowed) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsZoomControlEnabled(BOOL* enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsZoomControlEnabled(BOOL enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsBuiltInErrorPageEnabled(BOOL* enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsBuiltInErrorPageEnabled(BOOL enabled) = 0;
};

MIDL_INTERFACE("EE9A0F68-F46C-4E32-AC23-EF8CAC224D2A")
ICoreWebView2Settings2 : public ICoreWebView2Settings {
public:
    virtual HRESULT STDMETHODCALLTYPE get_UserAgent(LPWSTR* userAgent) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_UserAgent(LPCWSTR userAgent) = 0;
};

MIDL_INTERFACE("FDB5AB74-AF33-4854-84F0-0A631DEB5EBA")
ICoreWebView2Settings3 : public ICoreWebView2Settings2 {
public:
    virtual HRESULT STDMETHODCALLTYPE get_AreBrowserAcceleratorKeysEnabled(BOOL* areBrowserAcceleratorKeysEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AreBrowserAcceleratorKeysEnabled(BOOL areBrowserAcceleratorKeysEnabled) = 0;
};

MIDL_INTERFACE("76ECEACB-0462-4D94-AC83-423A6793775E")
ICoreWebView2 : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE get_Settings(ICoreWebView2Settings** settings) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Source(LPWSTR* uri) = 0;
    virtual HRESULT STDMETHODCALLTYPE Navigate(LPCWSTR uri) = 0;
    virtual HRESULT STDMETHODCALLTYPE NavigateToString(LPCWSTR htmlContent) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NavigationStarting(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NavigationStarting(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ContentLoading(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ContentLoading(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_SourceChanged(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_SourceChanged(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_HistoryChanged(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_HistoryChanged(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NavigationCompleted(ICoreWebView2NavigationCompletedEventHandler* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NavigationCompleted(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationStarting(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationStarting(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationCompleted(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationCompleted(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ScriptDialogOpening(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ScriptDialogOpening(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_PermissionRequested(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_PermissionRequested(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ProcessFailed(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ProcessFailed(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddScriptToExecuteOnDocumentCreated(LPCWSTR javaScript, void* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveScriptToExecuteOnDocumentCreated(LPCWSTR id) = 0;
    virtual HRESULT STDMETHODCALLTYPE ExecuteScript(LPCWSTR javaScript, void* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE CapturePreview(int imageFormat, void* imageStream, void* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE Reload() = 0;
    virtual HRESULT STDMETHODCALLTYPE PostWebMessageAsJson(LPCWSTR webMessageAsJson) = 0;
    virtual HRESULT STDMETHODCALLTYPE PostWebMessageAsString(LPCWSTR webMessageAsString) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_WebMessageReceived(ICoreWebView2WebMessageReceivedEventHandler* handler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_WebMessageReceived(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE CallDevToolsProtocolMethod(LPCWSTR methodName, LPCWSTR parametersAsJson, void* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_BrowserProcessId(UINT32* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanGoBack(BOOL* canGoBack) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanGoForward(BOOL* canGoForward) = 0;
    virtual HRESULT STDMETHODCALLTYPE GoBack() = 0;
    virtual HRESULT STDMETHODCALLTYPE GoForward() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDevToolsProtocolEventReceiver(LPCWSTR eventName, void** receiver) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NewWindowRequested(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NewWindowRequested(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_DocumentTitleChanged(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_DocumentTitleChanged(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_DocumentTitle(LPWSTR* title) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddHostObjectToScript(LPCWSTR name, VARIANT* object) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveHostObjectFromScript(LPCWSTR name) = 0;
    virtual HRESULT STDMETHODCALLTYPE OpenDevToolsWindow() = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ContainsFullScreenElementChanged(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ContainsFullScreenElementChanged(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ContainsFullScreenElement(BOOL* containsFullScreenElement) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_WebResourceRequested(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_WebResourceRequested(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddWebResourceRequestedFilter(LPCWSTR uri, int resourceContext) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveWebResourceRequestedFilter(LPCWSTR uri, int resourceContext) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_WindowCloseRequested(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_WindowCloseRequested(EventRegistrationToken token) = 0;
};

MIDL_INTERFACE("4D00C0D1-9434-4EB6-8078-8697A560334F")
ICoreWebView2Controller : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE get_IsVisible(BOOL* isVisible) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsVisible(BOOL isVisible) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Bounds(RECT* bounds) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Bounds(RECT bounds) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ZoomFactor(double* zoomFactor) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ZoomFactor(double zoomFactor) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ZoomFactorChanged(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ZoomFactorChanged(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBoundsAndZoomFactor(RECT bounds, double zoomFactor) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveFocus(int reason) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_MoveFocusRequested(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_MoveFocusRequested(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_GotFocus(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_GotFocus(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_LostFocus(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_LostFocus(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_AcceleratorKeyPressed(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_AcceleratorKeyPressed(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ParentWindow(HWND* parentWindow) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ParentWindow(HWND parentWindow) = 0;
    virtual HRESULT STDMETHODCALLTYPE NotifyParentWindowPositionChanged() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CoreWebView2(ICoreWebView2** coreWebView2) = 0;
};

MIDL_INTERFACE("C979903E-D4CA-4228-92EB-47EE3FA96EAB")
ICoreWebView2Controller2 : public ICoreWebView2Controller {
public:
    virtual HRESULT STDMETHODCALLTYPE get_DefaultBackgroundColor(COREWEBVIEW2_COLOR* backgroundColor) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_DefaultBackgroundColor(COREWEBVIEW2_COLOR backgroundColor) = 0;
};

MIDL_INTERFACE("B96D755E-0319-4E92-A296-23436F46A1FC")
ICoreWebView2Environment : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE CreateCoreWebView2Controller(HWND parentWindow, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateWebResourceResponse(void* content, int statusCode, LPCWSTR reasonPhrase, LPCWSTR headers, void** response) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_BrowserVersionString(LPWSTR* versionInfo) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NewBrowserVersionAvailable(void* eventHandler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NewBrowserVersionAvailable(EventRegistrationToken token) = 0;
};

typedef HRESULT(STDMETHODCALLTYPE* pfnCreateWebViewEnvironmentWithOptionsInternal)(
    bool checkRunningInstance,
    int runtimeType,
    PCWSTR userDataFolder,
    IUnknown* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* createdHandler
);

inline std::wstring FindEmbeddedBrowserDllPath() {
    const WCHAR* subKey = L"SOFTWARE\\Microsoft\\EdgeUpdate\\ClientState\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";
    HKEY roots[2] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    for (int i = 0; i < 2; ++i) {
        HKEY hKey = NULL;
        if (RegOpenKeyExW(roots[i], subKey, 0, KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS) {
            WCHAR ebPath[MAX_PATH] = {0};
            DWORD cbData = sizeof(ebPath), type = 0;
            if (RegQueryValueExW(hKey, L"EBWebView", NULL, &type, (LPBYTE)ebPath, &cbData) == ERROR_SUCCESS && ebPath[0]) {
                RegCloseKey(hKey);
                std::wstring dllPath = ebPath;
                if (dllPath.back() != L'\\' && dllPath.back() != L'/') dllPath += L'\\';
                dllPath += L"EBWebView\\x64\\EmbeddedBrowserWebView.dll";
                if (GetFileAttributesW(dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    return dllPath;
                }
            } else {
                RegCloseKey(hKey);
            }
        }
    }

    // Fallback: scan C:\Program Files (x86)\Microsoft\EdgeWebView\Application\*\EBWebView\x64\EmbeddedBrowserWebView.dll
    WCHAR progFiles[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"ProgramFiles(x86)", progFiles, MAX_PATH) > 0) {
        std::wstring baseDir = std::wstring(progFiles) + L"\\Microsoft\\EdgeWebView\\Application\\";
        WIN32_FIND_DATAW fd = {0};
        HANDLE hFind = FindFirstFileW((baseDir + L"*").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') {
                    std::wstring candidate = baseDir + fd.cFileName + L"\\EBWebView\\x64\\EmbeddedBrowserWebView.dll";
                    if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        FindClose(hFind);
                        return candidate;
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }
    return L"";
}

inline std::wstring GetUserDataFolderPath() {
    WCHAR appData[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData))) {
        std::wstring dir = std::wstring(appData) + L"\\rapoo-tray";
        CreateDirectoryW(dir.c_str(), NULL);
        dir += L"\\WebView2";
        CreateDirectoryW(dir.c_str(), NULL);
        return dir;
    }
    return L"";
}

// Shared Environment Singleton so Flyout and Settings share one WebView2 browser process
class EnvironmentManager : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    using EnvReadyCallback = std::function<void(ICoreWebView2Environment*)>;

    static void RequestEnvironment(EnvReadyCallback cb) {
        static EnvironmentManager* s_mgr = new EnvironmentManager();
        s_mgr->Request(std::move(cb));
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() override { return --m_refCount; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* createdEnvironment) override {
        m_creating = false;
        if (SUCCEEDED(errorCode) && createdEnvironment) {
            m_env = createdEnvironment;
            m_env->AddRef();
        }
        auto callbacks = std::move(m_pending);
        m_pending.clear();
        for (auto& cb : callbacks) {
            if (cb) cb(m_env);
        }
        return S_OK;
    }

private:
    void Request(EnvReadyCallback cb) {
        if (m_env) {
            cb(m_env);
            return;
        }
        m_pending.push_back(std::move(cb));
        if (m_creating) return;

        std::wstring dllPath = FindEmbeddedBrowserDllPath();
        if (dllPath.empty()) {
            auto callbacks = std::move(m_pending);
            m_pending.clear();
            for (auto& c : callbacks) if (c) c(nullptr);
            return;
        }

        if (!m_hDll) {
            m_hDll = LoadLibraryExW(dllPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
        if (!m_hDll) {
            auto callbacks = std::move(m_pending);
            m_pending.clear();
            for (auto& c : callbacks) if (c) c(nullptr);
            return;
        }

        auto fnCreate = (pfnCreateWebViewEnvironmentWithOptionsInternal)GetProcAddress(
            m_hDll, "CreateWebViewEnvironmentWithOptionsInternal");
        if (!fnCreate) {
            auto callbacks = std::move(m_pending);
            m_pending.clear();
            for (auto& c : callbacks) if (c) c(nullptr);
            return;
        }

        m_creating = true;
        std::wstring userData = GetUserDataFolderPath();
        HRESULT hr = fnCreate(true, 0 /* installed */, userData.c_str(), nullptr, this);
        if (FAILED(hr)) {
            m_creating = false;
            auto callbacks = std::move(m_pending);
            m_pending.clear();
            for (auto& c : callbacks) if (c) c(nullptr);
        }
    }

    std::atomic<ULONG> m_refCount{1};
    HMODULE m_hDll = NULL;
    ICoreWebView2Environment* m_env = nullptr;
    bool m_creating = false;
    std::vector<EnvReadyCallback> m_pending;
};

class Host : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
             public ICoreWebView2WebMessageReceivedEventHandler,
             public ICoreWebView2NavigationCompletedEventHandler {
public:
    using MessageCallback = std::function<void(const std::wstring&)>;
    using ReadyCallback   = std::function<void()>;

    Host() = default;
    virtual ~Host() {
        Shutdown();
    }

    void Initialize(HWND hWnd, bool transparentBg, const std::wstring& htmlContent,
                    MessageCallback onMessage, ReadyCallback onReady = nullptr) {
        m_hWnd = hWnd;
        m_transparentBg = transparentBg;
        m_initialHtml = htmlContent;
        m_onMessage = std::move(onMessage);
        m_onReady = std::move(onReady);

        AddRef(); // Hold reference while async controller creation is in flight
        EnvironmentManager::RequestEnvironment([this](ICoreWebView2Environment* env) {
            if (!env || !IsWindow(m_hWnd)) {
                Release();
                return;
            }
            HRESULT hr = env->CreateCoreWebView2Controller(m_hWnd, this);
            if (FAILED(hr)) {
                Release();
            }
        });
    }

    void Shutdown() {
        if (m_webview) {
            m_webview->Release();
            m_webview = nullptr;
        }
        if (m_controller) {
            m_controller->Close();
            m_controller->Release();
            m_controller = nullptr;
        }
        m_isReady = false;
    }

    bool IsReady() const { return m_isReady && m_webview != nullptr; }

    void Resize() {
        if (!m_controller || !IsWindow(m_hWnd)) return;
        RECT rc = {0};
        GetClientRect(m_hWnd, &rc);
        m_controller->put_Bounds(rc);
    }

    void SetVisible(bool visible) {
        if (m_controller) {
            m_controller->put_IsVisible(visible ? TRUE : FALSE);
        }
    }

    void NotifyPositionChanged() {
        if (m_controller) {
            m_controller->NotifyParentWindowPositionChanged();
        }
    }

    void ExecuteScript(const std::wstring& script) {
        if (m_isReady && m_webview) {
            m_webview->ExecuteScript(script.c_str(), nullptr);
        }
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = --m_refCount;
        return c;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        if (IsEqualIID(riid, IID_ICoreWebView2WebMessageReceivedEventHandler)) {
            *ppv = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        if (IsEqualIID(riid, IID_ICoreWebView2NavigationCompletedEventHandler)) {
            *ppv = static_cast<ICoreWebView2NavigationCompletedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* createdController) override {
        if (FAILED(errorCode) || !createdController || !IsWindow(m_hWnd)) {
            Release();
            return S_OK;
        }

        m_controller = createdController;
        m_controller->AddRef();

        // Set transparent background if requested (via ICoreWebView2Controller2)
        if (m_transparentBg) {
            ICoreWebView2Controller2* ctrl2 = nullptr;
            if (SUCCEEDED(m_controller->QueryInterface(IID_ICoreWebView2Controller2, (void**)&ctrl2)) && ctrl2) {
                COREWEBVIEW2_COLOR trans = { 0, 0, 0, 0 };
                ctrl2->put_DefaultBackgroundColor(trans);
                ctrl2->Release();
            }
        }

        m_controller->get_CoreWebView2(&m_webview);
        if (m_webview) {
            ICoreWebView2Settings* settings = nullptr;
            if (SUCCEEDED(m_webview->get_Settings(&settings)) && settings) {
                settings->put_AreDefaultContextMenusEnabled(FALSE);
                settings->put_IsStatusBarEnabled(FALSE);
                settings->put_IsZoomControlEnabled(FALSE);
                settings->put_AreDevToolsEnabled(FALSE);

                ICoreWebView2Settings3* settings3 = nullptr;
                if (SUCCEEDED(settings->QueryInterface(IID_ICoreWebView2Settings3, (void**)&settings3)) && settings3) {
                    settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
                    settings3->Release();
                }
                settings->Release();
            }

            EventRegistrationToken msgToken = {0};
            m_webview->add_WebMessageReceived(this, &msgToken);

            EventRegistrationToken navToken = {0};
            m_webview->add_NavigationCompleted(this, &navToken);

            Resize();
            m_controller->put_IsVisible(TRUE);

            if (!m_initialHtml.empty()) {
                m_webview->NavigateToString(m_initialHtml.c_str());
            }
        }

        Release();
        return S_OK;
    }

    // ICoreWebView2WebMessageReceivedEventHandler
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        if (!args) return S_OK;
        LPWSTR msg = nullptr;
        if (SUCCEEDED(args->TryGetWebMessageAsString(&msg)) && msg) {
            if (m_onMessage) {
                m_onMessage(std::wstring(msg));
            }
            CoTaskMemFree(msg);
        }
        return S_OK;
    }

    // ICoreWebView2NavigationCompletedEventHandler
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2NavigationCompletedEventArgs* /*args*/) override {
        m_isReady = true;
        if (m_onReady) {
            m_onReady();
        }
        return S_OK;
    }

private:
    std::atomic<ULONG> m_refCount{1};
    HWND m_hWnd = NULL;
    bool m_transparentBg = true;
    bool m_isReady = false;
    std::wstring m_initialHtml;
    MessageCallback m_onMessage;
    ReadyCallback m_onReady;
    ICoreWebView2Controller* m_controller = nullptr;
    ICoreWebView2* m_webview = nullptr;
};

} // namespace WebView2
