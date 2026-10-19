#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN
typedef NS_ENUM(NSInteger, MDInputAction) {
    MDInputActionKey, MDInputActionSpace, MDInputActionEnter,
    MDInputActionBackspace, MDInputActionToggleMode, MDInputActionLiteral,
    MDInputActionSelect, MDInputActionNextPage, MDInputActionPreviousPage,
    MDInputActionClear
};

@interface MDInputSnapshot : NSObject
@property(nonatomic, readonly) NSString *preedit;
@property(nonatomic, readonly) NSArray<NSString *> *candidates;
@property(nonatomic, readonly) NSInteger selectedIndex;
@property(nonatomic, readonly) BOOL asciiMode;
@property(nonatomic, readonly) BOOL hasPreviousPage;
@property(nonatomic, readonly) BOOL hasNextPage;
@end

@interface MDInputUpdate : NSObject
@property(nonatomic, readonly) MDInputSnapshot *snapshot;
@property(nonatomic, readonly) NSString *committedText;
@property(nonatomic, readonly) BOOL shouldDeleteBackward;
@end

@interface MDRimeBridge : NSObject
- (nullable instancetype)initWithDataPath:(NSString *)dataPath
                                userPath:(NSString *)userPath
                                   error:(NSError **)error;
- (MDInputUpdate *)applyAction:(MDInputAction)action text:(NSString *)text index:(NSInteger)index;
@end
NS_ASSUME_NONNULL_END
