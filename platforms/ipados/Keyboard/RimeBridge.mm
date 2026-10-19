#import "RimeBridge.h"
#include "PinyinSession.h"
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace {
NSString *String(const std::string& value) {
    return [[NSString alloc] initWithBytes:value.data() length:value.size()
                                  encoding:NSUTF8StringEncoding] ?: @"";
}
maodou::Action Action(MDInputAction action) {
    switch (action) {
        case MDInputActionKey: return maodou::Action::Key;
        case MDInputActionSpace: return maodou::Action::Space;
        case MDInputActionEnter: return maodou::Action::Return;
        case MDInputActionBackspace: return maodou::Action::Backspace;
        case MDInputActionToggleMode: return maodou::Action::ToggleMode;
        case MDInputActionLiteral: return maodou::Action::Literal;
        case MDInputActionSelect: return maodou::Action::Select;
        case MDInputActionNextPage: return maodou::Action::NextPage;
        case MDInputActionPreviousPage: return maodou::Action::PreviousPage;
        case MDInputActionClear: return maodou::Action::Clear;
    }
    return maodou::Action::Clear;
}
}

@implementation MDInputSnapshot
- (instancetype)initWithSnapshot:(const maodou::Snapshot&)snapshot {
    if ((self = [super init])) {
        _preedit = String(snapshot.preedit);
        NSMutableArray<NSString *> *words = [NSMutableArray array];
        for (const auto& word : snapshot.candidates) [words addObject:String(word)];
        _candidates = [words copy];
        _selectedIndex = snapshot.selected;
        _asciiMode = snapshot.ascii;
        _hasPreviousPage = snapshot.previous_page;
        _hasNextPage = snapshot.next_page;
    }
    return self;
}
@end

@implementation MDInputUpdate
- (instancetype)initWithUpdate:(const maodou::Update&)update {
    if ((self = [super init])) {
        _snapshot = [[MDInputSnapshot alloc] initWithSnapshot:update.snapshot];
        _committedText = String(update.text);
        _shouldDeleteBackward = update.delete_backward;
    }
    return self;
}
@end

@implementation MDRimeBridge {
    std::unique_ptr<maodou::Session> _session;
}
- (nullable instancetype)initWithDataPath:(NSString *)dataPath
                                userPath:(NSString *)userPath error:(NSError **)error {
    if (!(self = [super init])) return nil;
    try {
        static std::once_flag initialized;
        auto* api = rime_get_api();
        std::call_once(initialized, [&] {
            const std::string data = dataPath.UTF8String;
            const std::string user = userPath.UTF8String;
            const std::string prebuilt = data + "/build";
            if (!std::filesystem::exists(prebuilt + "/leaves_stone.table.bin"))
                throw std::runtime_error("The prebuilt dictionary is missing");
            std::filesystem::create_directories(user);
            RIME_STRUCT(RimeTraits, traits);
            traits.shared_data_dir = data.c_str();
            traits.user_data_dir = user.c_str();
            traits.prebuilt_data_dir = prebuilt.c_str();
            traits.app_name = "rime.xiaomaodou_keyboard";
            traits.log_dir = "";
            traits.min_log_level = 3;
            api->setup(&traits);
            api->initialize(nullptr);
            // All dictionaries are compiled at build time. Never start a
            // maintenance thread in the user's keyboard extension.
        });
        _session = std::make_unique<maodou::Session>(api);
    } catch (const std::exception& exception) {
        if (error) *error = [NSError errorWithDomain:@"XiaomaodouInput" code:1
            userInfo:@{NSLocalizedDescriptionKey:String(exception.what())}];
        return nil;
    }
    return self;
}
- (MDInputUpdate *)applyAction:(MDInputAction)action text:(NSString *)text index:(NSInteger)index {
    const size_t selected = index < 0 ? std::numeric_limits<size_t>::max() : static_cast<size_t>(index);
    return [[MDInputUpdate alloc] initWithUpdate:
        _session->Apply(Action(action), text.UTF8String ?: "", selected)];
}
@end
