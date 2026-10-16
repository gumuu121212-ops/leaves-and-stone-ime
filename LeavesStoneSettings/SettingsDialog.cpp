#include "SettingsDialog.h"

#include <commctrl.h>

#include <array>
#include <algorithm>
#include <string>

namespace leaves_stone::settings {
namespace {

constexpr wchar_t kWindowClass[] = L"LeavesStoneSettingsWindow";
constexpr UINT kSettingsChangedMessage = WM_APP + 73;
enum ControlId {
  kCandidateCount = 101,
  kTextSize = 102,
  kOrientation = 103,
  kAnimation = 104,
  kDecoration = 105,
  kSave = 106,
  kReset = 107,
  kCancel = 108,
};

struct DialogState {
  SettingsStore* store = nullptr;
  UserSettings settings;
  HFONT font = nullptr;
};

HWND AddControl(HWND parent,
                const wchar_t* class_name,
                const wchar_t* text,
                DWORD style,
                int x,
                int y,
                int width,
                int height,
                int id) {
  HWND control = CreateWindowExW(0, class_name, text, WS_CHILD | WS_VISIBLE | style,
                                 x, y, width, height, parent,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                 GetModuleHandleW(nullptr), nullptr);
  SendMessageW(control, WM_SETFONT,
               reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
  return control;
}

void SelectCombo(HWND window, int id, int index) {
  SendDlgItemMessageW(window, id, CB_SETCURSEL, index, 0);
}

int ComboSelection(HWND window, int id) {
  return static_cast<int>(SendDlgItemMessageW(window, id, CB_GETCURSEL, 0, 0));
}

void PushToControls(HWND window, const UserSettings& settings) {
  SelectCombo(window, kCandidateCount,
              settings.candidate_count == 9 ? 2
              : settings.candidate_count == 7 ? 1
                                                : 0);
  SelectCombo(window, kTextSize,
              settings.text_size == TextSize::Large ? 2
              : settings.text_size == TextSize::Small ? 0
                                                       : 1);
  SelectCombo(window, kOrientation,
              settings.orientation == Orientation::Vertical ? 1 : 0);
  SendDlgItemMessageW(window, kAnimation, BM_SETCHECK,
                      settings.animation_enabled ? BST_CHECKED : BST_UNCHECKED,
                      0);
  SendDlgItemMessageW(window, kDecoration, BM_SETCHECK,
                      settings.decoration_enabled ? BST_CHECKED : BST_UNCHECKED,
                      0);
}

UserSettings PullFromControls(HWND window) {
  UserSettings settings;
  const int count = ComboSelection(window, kCandidateCount);
  settings.candidate_count = count == 2 ? 9 : count == 1 ? 7 : 5;
  const int text = ComboSelection(window, kTextSize);
  settings.text_size =
      text == 2 ? TextSize::Large : text == 0 ? TextSize::Small : TextSize::Medium;
  settings.orientation = ComboSelection(window, kOrientation) == 1
                             ? Orientation::Vertical
                             : Orientation::Horizontal;
  settings.animation_enabled =
      SendDlgItemMessageW(window, kAnimation, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.decoration_enabled =
      SendDlgItemMessageW(window, kDecoration, BM_GETCHECK, 0, 0) == BST_CHECKED;
  return settings;
}

void DrawPreview(HWND window, HDC dc, const UserSettings& settings) {
  RECT preview = {24, 248, 496, 388};
  HBRUSH backdrop = CreateSolidBrush(RGB(238, 235, 226));
  FillRect(dc, &preview, backdrop);
  DeleteObject(backdrop);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(52, 63, 53));
  RECT title = {40, 258, 480, 280};
  DrawTextW(dc, L"实时预览", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  const int count = settings.orientation == Orientation::Vertical
                        ? 3
                        : std::min(settings.candidate_count, 5);
  const int card_height = settings.text_size == TextSize::Large
                              ? 38
                              : settings.text_size == TextSize::Small ? 28 : 32;
  const std::array<COLORREF, 3> depth = {
      RGB(190, 105, 76), RGB(87, 120, 86), RGB(112, 159, 176)};
  for (int index = 0; index < count; ++index) {
    const int left = settings.orientation == Orientation::Vertical
                         ? 165
                         : 42 + index * 84;
    const int top = settings.orientation == Orientation::Vertical
                        ? 282 + index * 31
                        : 307;
    RECT thickness = {left + 3, top + 5, left + 76, top + card_height + 5};
    HBRUSH depth_brush = CreateSolidBrush(depth[index % depth.size()]);
    FillRect(dc, &thickness, depth_brush);
    DeleteObject(depth_brush);
    RECT face = {left, top, left + 73, top + card_height};
    HBRUSH paper = CreateSolidBrush(index == 1 ? RGB(224, 239, 235)
                                               : RGB(249, 244, 230));
    FillRect(dc, &face, paper);
    DeleteObject(paper);
    DrawTextW(dc, index == 0 ? L"1 今天" : index == 1 ? L"2 天气" : L"3 很好",
              -1, &face, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }
  if (settings.decoration_enabled) {
    HPEN leaf = CreatePen(PS_SOLID, 3, RGB(83, 126, 82));
    HGDIOBJ old_pen = SelectObject(dc, leaf);
    MoveToEx(dc, 38, 292, nullptr);
    LineTo(dc, 78, 270);
    MoveToEx(dc, 442, 286, nullptr);
    LineTo(dc, 478, 268);
    SelectObject(dc, old_pen);
    DeleteObject(leaf);
  }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param,
                            LPARAM l_param) {
  DialogState* state = reinterpret_cast<DialogState*>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
    state = reinterpret_cast<DialogState*>(create->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(state));
  }
  switch (message) {
    case WM_CREATE: {
      AddControl(window, L"STATIC", L"候选词数量", 0, 24, 24, 110, 24, 0);
      HWND count = AddControl(window, WC_COMBOBOXW, L"",
                              CBS_DROPDOWNLIST | WS_TABSTOP, 150, 20, 150, 120,
                              kCandidateCount);
      for (const wchar_t* value : {L"5 个", L"7 个", L"9 个"})
        SendMessageW(count, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      AddControl(window, L"STATIC", L"文字大小", 0, 24, 66, 110, 24, 0);
      HWND text = AddControl(window, WC_COMBOBOXW, L"",
                             CBS_DROPDOWNLIST | WS_TABSTOP, 150, 62, 150, 120,
                             kTextSize);
      for (const wchar_t* value : {L"小", L"中", L"大"})
        SendMessageW(text, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      AddControl(window, L"STATIC", L"排列方向", 0, 24, 108, 110, 24, 0);
      HWND orientation = AddControl(window, WC_COMBOBOXW, L"",
                                    CBS_DROPDOWNLIST | WS_TABSTOP, 150, 104,
                                    150, 100, kOrientation);
      for (const wchar_t* value : {L"横向", L"纵向"})
        SendMessageW(orientation, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(value));

      AddControl(window, L"BUTTON", L"启用轻微淡入动画",
                 BS_AUTOCHECKBOX | WS_TABSTOP, 24, 150, 210, 26, kAnimation);
      AddControl(window, L"BUTTON", L"显示叶片与建筑装饰",
                 BS_AUTOCHECKBOX | WS_TABSTOP, 250, 150, 220, 26,
                 kDecoration);
      AddControl(window, L"BUTTON", L"保存", BS_DEFPUSHBUTTON | WS_TABSTOP,
                 248, 414, 80, 30, kSave);
      AddControl(window, L"BUTTON", L"恢复默认", WS_TABSTOP, 336, 414, 80,
                 30, kReset);
      AddControl(window, L"BUTTON", L"取消", WS_TABSTOP, 424, 414, 72, 30,
                 kCancel);
      PushToControls(window, state->settings);
      return 0;
    }
    case WM_COMMAND: {
      const int id = LOWORD(w_param);
      const int notification = HIWORD(w_param);
      if (id == kSave) {
        state->settings = PullFromControls(window);
        if (!state->store->Save(state->settings)) {
          MessageBoxW(window, L"设置保存失败。", L"Leaves & Stone",
                      MB_OK | MB_ICONERROR);
          return 0;
        }
        PostMessageW(HWND_BROADCAST, kSettingsChangedMessage, 0, 0);
        DestroyWindow(window);
        return 0;
      }
      if (id == kReset) {
        state->settings = UserSettings{};
        PushToControls(window, state->settings);
        InvalidateRect(window, nullptr, TRUE);
        return 0;
      }
      if (id == kCancel) {
        DestroyWindow(window);
        return 0;
      }
      if (notification == CBN_SELCHANGE || notification == BN_CLICKED) {
        state->settings = PullFromControls(window);
        InvalidateRect(window, nullptr, TRUE);
      }
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT paint = {};
      HDC dc = BeginPaint(window, &paint);
      DrawPreview(window, dc, state->settings);
      EndPaint(window, &paint);
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(window, message, w_param, l_param);
}

}  // namespace

int ShowSettingsDialog(HINSTANCE instance, SettingsStore* store) {
  if (!store)
    return 1;
  WNDCLASSEXW window_class = {sizeof(WNDCLASSEXW)};
  window_class.hInstance = instance;
  window_class.lpfnWndProc = WindowProc;
  window_class.lpszClassName = kWindowClass;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&window_class);

  DialogState state;
  state.store = store;
  state.settings = store->Load();
  HWND window = CreateWindowExW(
      WS_EX_DLGMODALFRAME, kWindowClass, L"Leaves & Stone 输入法设置",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
      536, 500, nullptr, nullptr, instance, &state);
  if (!window)
    return 1;
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);

  MSG message = {};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  return static_cast<int>(message.wParam);
}

}  // namespace leaves_stone::settings
