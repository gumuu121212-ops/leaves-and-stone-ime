#include "ThemeAssets.h"

#include <objidl.h>
#include <wincodec.h>

#include <cstring>
#include <fstream>
#include <string>

#include "resource.h"

namespace leaves_stone {
namespace {
constexpr std::array<int, static_cast<size_t>(ThemeAssetId::Count)>
    kResourceIds = {
        IDR_DOG_MASCOT_DEFAULT, IDR_DOG_MASCOT_HOVER, IDR_DOG_MASCOT_PRESSED,
        IDR_DOG_MASCOT_IDLE,    IDR_DOG_MASCOT_AVATAR, IDR_DOG_PANEL,
        IDR_DOG_MENU,           IDR_DOG_PAW,           IDR_DOG_ICON_ZH,
        IDR_DOG_ICON_EN,        IDR_DOG_ICON_JP,       IDR_DOG_ICON_KR,
        IDR_DOG_ICON_MOON,      IDR_DOG_ICON_SHIRT,    IDR_DOG_ICON_GEAR,
        IDR_DOG_TOOLBAR_DEFAULT, IDR_DOG_TOOLBAR_HOVER, IDR_DOG_TOOLBAR_PRESSED,
        IDR_DOG_TOOLBAR_IDLE,   IDR_DOG_TB_MODE_BG,    IDR_DOG_TB_MODE_ZH,
        IDR_DOG_TB_MODE_EN,     IDR_DOG_TB_ICON_MOON,  IDR_DOG_TB_ICON_SHIRT,
        IDR_DOG_TB_ICON_GEAR,   IDR_DOG_TB_BTN_HOVER,  IDR_DOG_TB_BTN_PRESSED,
        IDR_DOG_TB_DECOR_HOVER, IDR_DOG_TB_DECOR_PRESSED, IDR_DOG_TB_DECOR_IDLE,
        IDR_DOG_REST_SLEEP,     IDR_DOG_REST_AWAKE,
        IDR_DOG_REST_DECOR_SLEEP, IDR_DOG_REST_DECOR_AWAKE,
        IDR_DOG_REST_HEAD_SLEEP, IDR_DOG_REST_HEAD_AWAKE};

// The candidate window is drawn both by WeaselServer.exe and, in this build, by
// the text service inside the client process. GetModuleHandleW(nullptr) returns
// the host executable, which holds no artwork, so resolve the module that
// actually contains this code instead.
HMODULE ResolveAssetModule() {
  HMODULE module = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&ResolveAssetModule),
                         &module)) {
    return module;
  }
  return GetModuleHandleW(nullptr);
}
}  // namespace

ThemeAssets::~ThemeAssets() {
  Discard();
}

bool ThemeAssets::Load(HMODULE module) {
  Discard();
  if (!module)
    module = ResolveAssetModule();

  HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&factory_));
  if (result == CO_E_NOTINITIALIZED) {
    const HRESULT initialized =
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    owns_com_initialization_ = SUCCEEDED(initialized);
    result = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                              CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&factory_));
  }
  if (FAILED(result) || !factory_) {
    Discard();
    return false;
  }

  for (size_t index = 0; index < kResourceIds.size(); ++index) {
    if (!DecodeResource(module, kResourceIds[index], &bitmaps_[index])) {
      // Record which layer failed so the runtime log explains a classic-layout
      // fallback instead of leaving it a mystery.
      wchar_t temp_path[MAX_PATH] = {0};
      if (GetTempPathW(MAX_PATH, temp_path) != 0) {
        std::wstring path = std::wstring(temp_path) + L"leaves-stone-ui.log";
        std::ofstream out(path.c_str(), std::ios::app | std::ios::binary);
        if (out) {
          out << "asset_load_failed index=" << index
              << " resource_id=" << kResourceIds[index] << "\n";
        }
      }
      Discard();
      return false;
    }
  }
  return true;
}

void ThemeAssets::Discard() {
  for (auto& bitmap : bitmaps_) {
    if (bitmap.handle)
      DeleteObject(bitmap.handle);
    bitmap = {};
  }
  if (factory_) {
    factory_->Release();
    factory_ = nullptr;
  }
  if (owns_com_initialization_) {
    CoUninitialize();
    owns_com_initialization_ = false;
  }
}

const ThemeBitmap* ThemeAssets::Get(ThemeAssetId id) const {
  const size_t index = static_cast<size_t>(id);
  if (index >= bitmaps_.size() || !bitmaps_[index].handle)
    return nullptr;
  return &bitmaps_[index];
}

size_t ThemeAssets::LoadedBitmapCount() const {
  size_t count = 0;
  for (const auto& bitmap : bitmaps_) {
    if (bitmap.handle)
      ++count;
  }
  return count;
}

bool ThemeAssets::DecodeResource(HMODULE module,
                                 int resource_id,
                                 ThemeBitmap* destination) {
  if (!module || !destination || !factory_)
    return false;

  const HRSRC resource =
      FindResourceW(module, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
  if (!resource)
    return false;
  const DWORD resource_size = SizeofResource(module, resource);
  const HGLOBAL loaded_resource = LoadResource(module, resource);
  const void* resource_bytes = LockResource(loaded_resource);
  if (!resource_bytes || resource_size == 0)
    return false;

  HGLOBAL stream_memory = GlobalAlloc(GMEM_MOVEABLE, resource_size);
  if (!stream_memory)
    return false;
  void* stream_bytes = GlobalLock(stream_memory);
  if (!stream_bytes) {
    GlobalFree(stream_memory);
    return false;
  }
  std::memcpy(stream_bytes, resource_bytes, resource_size);
  GlobalUnlock(stream_memory);

  IStream* stream = nullptr;
  HRESULT result = CreateStreamOnHGlobal(stream_memory, TRUE, &stream);
  if (FAILED(result)) {
    GlobalFree(stream_memory);
    return false;
  }

  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICFormatConverter* converter = nullptr;
  result = factory_->CreateDecoderFromStream(
      stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
  if (SUCCEEDED(result))
    result = decoder->GetFrame(0, &frame);
  if (SUCCEEDED(result))
    result = factory_->CreateFormatConverter(&converter);
  if (SUCCEEDED(result)) {
    result = converter->Initialize(
        frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr,
        0.0, WICBitmapPaletteTypeCustom);
  }

  UINT width = 0;
  UINT height = 0;
  if (SUCCEEDED(result))
    result = converter->GetSize(&width, &height);

  void* pixels = nullptr;
  HBITMAP bitmap = nullptr;
  if (SUCCEEDED(result) && width > 0 && height > 0) {
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG>(width);
    info.bmiHeader.biHeight = -static_cast<LONG>(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr,
                              0);
    if (!bitmap || !pixels)
      result = E_OUTOFMEMORY;
  }
  if (SUCCEEDED(result)) {
    const UINT stride = width * 4;
    result = converter->CopyPixels(nullptr, stride, stride * height,
                                   static_cast<BYTE*>(pixels));
  }

  if (converter)
    converter->Release();
  if (frame)
    frame->Release();
  if (decoder)
    decoder->Release();
  stream->Release();

  if (FAILED(result)) {
    if (bitmap)
      DeleteObject(bitmap);
    return false;
  }
  destination->handle = bitmap;
  destination->width = width;
  destination->height = height;
  return true;
}

}  // namespace leaves_stone
