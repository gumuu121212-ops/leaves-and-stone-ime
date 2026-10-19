#include "stdafx.h"

#include <WeaselIPCData.h>
#include <thread>
#include <shellapi.h>
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "LanguageBar.h"
#include "Compartment.h"
#include "ResponseParser.h"
#include "TsfDiagnostics.h"

static void error_message(const WCHAR* msg) {
  static DWORD next_tick = 0;
  DWORD now = GetTickCount();
  if (now > next_tick) {
    next_tick = now + 10000;  // (ms)
    MessageBox(NULL, msg, get_weasel_ime_name().c_str(), MB_ICONERROR | MB_OK);
  }
}

WeaselTSF::WeaselTSF() {
  _cRef = 1;

  _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
  _dwProfileNotifySinkCookie = TF_INVALID_COOKIE;

  _dwTextEditSinkCookie = TF_INVALID_COOKIE;
  _dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
  _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
  _fTestKeyDownPending = FALSE;
  _fTestKeyUpPending = FALSE;

  _fCUASWorkaroundTested = _fCUASWorkaroundEnabled = FALSE;

  _cand = new CCandidateList(this);

  DllAddRef();
}

WeaselTSF::~WeaselTSF() {
  _toolbar_activation.Stop();
  DllRelease();
}

STDAPI WeaselTSF::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL)
    return E_INVALIDARG;

  *ppvObject = NULL;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfTextInputProcessor))
    *ppvObject = (ITfTextInputProcessor*)this;
  else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    *ppvObject = (ITfTextInputProcessorEx*)this;
  else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    *ppvObject = (ITfThreadMgrEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextEditSink))
    *ppvObject = (ITfTextEditSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextLayoutSink))
    *ppvObject = (ITfTextLayoutSink*)this;
  else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    *ppvObject = (ITfKeyEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfCompositionSink))
    *ppvObject = (ITfCompositionSink*)this;
  else if (IsEqualIID(riid, IID_ITfEditSession))
    *ppvObject = (ITfEditSession*)this;
  else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    *ppvObject = (ITfThreadFocusSink*)this;
  else if (IsEqualIID(riid, IID_ITfActiveLanguageProfileNotifySink))
    *ppvObject = (ITfActiveLanguageProfileNotifySink*)this;
  else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
    *ppvObject = (ITfDisplayAttributeProvider*)this;

  if (*ppvObject) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDAPI_(ULONG) WeaselTSF::AddRef() {
  return ++_cRef;
}

STDAPI_(ULONG) WeaselTSF::Release() {
  LONG cr = --_cRef;

  assert(_cRef >= 0);

  if (_cRef == 0)
    delete this;

  return cr;
}

STDAPI WeaselTSF::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
  return ActivateEx(pThreadMgr, tfClientId, 0U);
}

STDAPI WeaselTSF::Deactivate() {
  _toolbar_profile_active = false;
  _toolbar_activation.Stop();
  m_client.FocusOut();
  m_client.EndSession();

  _InitTextEditSink(com_ptr<ITfDocumentMgr>());

  _UninitThreadMgrEventSink();

  _UninitKeyEventSink();
  _UninitPreservedKey();

  _UninitLanguageBar();

  _UninitCompartment();

  _UninitThreadFocusSink();

  _pThreadMgr = NULL;

  _tfClientId = TF_CLIENTID_NULL;

  _cand->DestroyAll();

  return S_OK;
}

STDAPI WeaselTSF::ActivateEx(ITfThreadMgr* pThreadMgr,
                             TfClientId tfClientId,
                             DWORD dwFlags) {
  com_ptr<ITfDocumentMgr> pDocMgrFocus;
  _activateFlags = dwFlags;
  _toolbar_profile_active = true;

  _pThreadMgr = pThreadMgr;
  _tfClientId = tfClientId;

  if (!_InitThreadMgrEventSink())
    goto ExitError;

  if ((_pThreadMgr->GetFocus(&pDocMgrFocus) == S_OK) &&
      (pDocMgrFocus != NULL)) {
    _InitTextEditSink(pDocMgrFocus);
  }

  if (!_InitKeyEventSink())
    goto ExitError;

  // if (!_InitDisplayAttributeGuidAtom())
  //	goto ExitError;
  //	some app might init failed because it not provide DisplayAttributeInfo,
  // like some opengl stuff
  _InitDisplayAttributeGuidAtom();

  if (!_InitPreservedKey())
    goto ExitError;

  if (!_InitLanguageBar())
    goto ExitError;

  if (!_IsKeyboardOpen())
    _SetKeyboardOpen(TRUE);

  if (!_InitCompartment())
    goto ExitError;
  if (!_InitThreadFocusSink())
    goto ExitError;

  if (_EnsureServerConnected()) {
    BOOL focused = FALSE;
    if (SUCCEEDED(_pThreadMgr->IsThreadFocus(&focused)) && focused)
      m_client.FocusIn();
  }
  // Initial profile/focus notifications may precede the IPC session or the
  // final thread-focus state. Reconcile after activation, without a key event.
  _ScheduleToolbarActivation();

  return S_OK;

ExitError:
  Deactivate();
  return E_FAIL;
}

STDMETHODIMP WeaselTSF::OnSetThreadFocus() {
  _ScheduleToolbarActivation();
  std::wstring _ToggleImeOnOpenClose{};
  RegGetStringValue(HKEY_CURRENT_USER, leaves_stone::product::kRegistryRoot,
                    L"ToggleImeOnOpenClose", _ToggleImeOnOpenClose);
  _isToOpenClose = (_ToggleImeOnOpenClose == L"yes");
  if (m_client.Echo()) {
    m_client.FocusIn();
    m_client.ProcessKeyEvent(0);
    weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
    bool ok = m_client.GetResponseData(std::ref(parser));
    if (ok)
      _UpdateLanguageBar(_status);
  }
  return S_OK;
}
STDMETHODIMP WeaselTSF::OnKillThreadFocus() {
  _toolbar_activation.Stop();
  m_client.FocusOut();
  _AbortComposition();
  return S_OK;
}
BOOL WeaselTSF::_InitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return FALSE;
  if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink,
                                 (ITfThreadFocusSink*)this,
                                 &_dwThreadFocusSinkCookie)))
    return FALSE;
  return TRUE;
}

void WeaselTSF::_UninitThreadFocusSink() {
  if (_dwThreadFocusSinkCookie == TF_INVALID_COOKIE || !_pThreadMgr)
    return;
  com_ptr<ITfSource> source;
  if (SUCCEEDED(_pThreadMgr->QueryInterface(&source)))
    source->UnadviseSink(_dwThreadFocusSinkCookie);
  _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
}
STDMETHODIMP WeaselTSF::OnActivated(REFCLSID clsid,
                                    REFGUID guidProfile,
                                    BOOL isActivated) {
  if (!IsEqualCLSID(clsid, c_clsidTextService)) {
    return S_OK;
  }

  if (isActivated) {
    _toolbar_profile_active = true;
    _ShowLanguageBar(TRUE);
    _UpdateLanguageBar(_status);
    BOOL focused = FALSE;
    if (_pThreadMgr && SUCCEEDED(_pThreadMgr->IsThreadFocus(&focused)) &&
        focused && m_client.Echo())
      m_client.FocusIn();
    _ScheduleToolbarActivation();
  } else {
    _toolbar_profile_active = false;
    _toolbar_activation.Stop();
    m_client.FocusOut();
    _DeleteCandidateList();
    _ShowLanguageBar(FALSE);
  }
  leaves_stone::WriteTsfDiagnostic("profile", isActivated ? "active=1" : "active=0");
  return S_OK;
}

void WeaselTSF::_ScheduleToolbarActivation() {
  if (!_toolbar_profile_active || !_pThreadMgr) return;
  const com_ptr<WeaselTSF> owner(this);
  _toolbar_activation.Start([this, owner]() {
    // Keep the service alive across a reentrant deactivation during IPC.
    if (!owner->_toolbar_profile_active || !owner->_pThreadMgr) return true;
    BOOL focused = FALSE;
    const bool thread_focused =
        SUCCEEDED(_pThreadMgr->IsThreadFocus(&focused)) && focused;
    const bool connected = thread_focused && _EnsureServerConnected();
    if (!_toolbar_profile_active || !_pThreadMgr) return true;
    BOOL still_focused = FALSE;
    const bool ready = connected &&
        SUCCEEDED(_pThreadMgr->IsThreadFocus(&still_focused)) && still_focused;
    leaves_stone::WriteTsfDiagnostic(
        "toolbar-focus", std::string("focused=") + (thread_focused ? "1" : "0") +
                             " ready=" + (ready ? "1" : "0"));
    if (ready) m_client.FocusIn();
    return ready;
  });
}

void WeaselTSF::_Reconnect() {
  m_client.Disconnect();
  m_client.Connect(NULL);
  m_client.StartSession();
  weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
  bool ok = m_client.GetResponseData(std::ref(parser));
  if (ok) {
    _UpdateLanguageBar(_status);
  }
}

bool WeaselTSF::_EnsureServerConnected() {
  if (!m_client.Echo()) {
    _Reconnect();
    ++_server_retry;
    if (_server_retry >= 6) {
      HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerExclusiveMutex");
      if (!m_client.Echo() && GetLastError() != ERROR_ALREADY_EXISTS) {
        std::wstring dir = _GetRootDir();
        std::thread th([dir]() {
          ShellExecuteW(NULL, L"open", (dir + L"\\start_service.bat").c_str(),
                        NULL, dir.c_str(), SW_HIDE);
          // Activation retries reconnect on the TSF thread. Do not retain the
          // text service or access its COM language bar from this launcher.
        });
        th.detach();
      }
      if (hMutex) {
        CloseHandle(hMutex);
      }
      _server_retry = 0;
    }
    return (m_client.Echo() != 0);
  } else {
    _server_retry = 0;
    return true;
  }
}
