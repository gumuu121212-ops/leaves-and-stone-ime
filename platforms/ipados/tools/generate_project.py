#!/usr/bin/env python3
"""Generate the native app/extension/test project without a package manager."""
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
objects = {}


def uid(name):
    return hashlib.sha1(name.encode()).hexdigest()[:24].upper()


def obj(identity, isa, **values):
    key = uid(identity)
    objects[key] = dict(isa=isa, **values)
    return key


def encode(value, level=0):
    if isinstance(value, dict):
        return "{\n" + "".join("\t" * (level + 1) + json.dumps(str(k)) + " = " +
            encode(v, level + 1) + ";\n" for k, v in value.items()) + "\t" * level + "}"
    if isinstance(value, list):
        return "(" + ", ".join(encode(v, level) for v in value) + ")"
    return json.dumps(str(value))


def main():
    project_dir = ROOT / "Xiaomaodou.xcodeproj"
    project_dir.mkdir(exist_ok=True)
    shared = ["Shared/MascotView.swift", "Shared/Theme.swift"]
    keyboard = ["Keyboard/InputService.swift", "Keyboard/KeyboardSurface.swift",
                "Keyboard/KeyboardViewController.swift", "Keyboard/RimeBridge.mm",
                "Native/PinyinSession.cpp"]
    image_resources = ["Resources/mascot-awake.png", "Resources/mascot-sleep.png"]
    resources = image_resources + ["Resources/Rime", "Resources/Notices", "Resources/Assets.xcassets"]
    all_files = shared + keyboard + resources + ["App/XiaomaodouApp.swift", "Tests/KeyboardTests.swift",
        "Keyboard/RimeBridge.h", "Keyboard/Keyboard-Bridging-Header.h", "Native/PinyinSession.h",
        "Vendor/librime-static.xcframework", "Configuration/App.plist", "Configuration/Keyboard.plist"]
    refs = {}
    types = {".swift": "sourcecode.swift", ".mm": "sourcecode.cpp.objcpp",
             ".cpp": "sourcecode.cpp.cpp", ".h": "sourcecode.c.h", ".png": "image.png",
             ".plist": "text.plist.xml", ".xcframework": "wrapper.xcframework",
             ".xcassets": "folder.assetcatalog"}
    for file in all_files:
        refs[file] = obj(file, "PBXFileReference", path=file, sourceTree="SOURCE_ROOT",
                         lastKnownFileType=types.get(Path(file).suffix, "folder"))
    products = {}
    for name, ext, kind in [("Xiaomaodou", ".app", "wrapper.application"),
                             ("XiaomaodouKeyboard", ".appex", "wrapper.app-extension"),
                             ("XiaomaodouTests", ".xctest", "wrapper.cfbundle")]:
        products[name] = obj(name + ":product", "PBXFileReference", path=name + ext,
                             explicitFileType=kind, sourceTree="BUILT_PRODUCTS_DIR")
    product_group = obj("products", "PBXGroup", name="Products", sourceTree="<group>",
                        children=list(products.values()))
    main_group = obj("main", "PBXGroup", sourceTree="<group>", children=list(refs.values()) + [product_group])
    common = dict(IPHONEOS_DEPLOYMENT_TARGET="15.0", TARGETED_DEVICE_FAMILY="2", SDKROOT="iphoneos",
        SWIFT_VERSION="5.0", CLANG_CXX_LANGUAGE_STANDARD="c++17", CLANG_ENABLE_OBJC_ARC="YES",
        CLANG_ENABLE_MODULES="YES", GCC_C_LANGUAGE_STANDARD="gnu11", ENABLE_BITCODE="NO",
        CODE_SIGN_STYLE="Automatic", CURRENT_PROJECT_VERSION="1", MARKETING_VERSION="0.1.0",
        GENERATE_INFOPLIST_FILE="NO", DEBUG_INFORMATION_FORMAT="dwarf-with-dsym",
        GCC_WARN_64_TO_32_BIT_CONVERSION="YES", CLANG_WARN_DOCUMENTATION_COMMENTS="YES")

    def configs(name, extra):
        configs = []
        for mode in ["Debug", "Release"]:
            settings = dict(common, **extra)
            settings.update(SWIFT_OPTIMIZATION_LEVEL="-Onone" if mode == "Debug" else "-O",
                            GCC_OPTIMIZATION_LEVEL="0" if mode == "Debug" else "s")
            if mode == "Debug":
                settings.update(SWIFT_ACTIVE_COMPILATION_CONDITIONS="DEBUG", ENABLE_TESTABILITY="YES")
            configs.append(obj(name + mode, "XCBuildConfiguration", name=mode, buildSettings=settings))
        return obj(name + ":configs", "XCConfigurationList", buildConfigurations=configs,
                   defaultConfigurationIsVisible="0", defaultConfigurationName="Release")

    def phase(name, kind, paths):
        return obj(name, kind, buildActionMask="2147483647", runOnlyForDeploymentPostprocessing="0",
                   files=[obj(name + path, "PBXBuildFile", fileRef=refs[path]) for path in paths])

    native_settings = dict(HEADER_SEARCH_PATHS=["$(inherited)", "$(SRCROOT)/Vendor/RimeHeaders",
            "$(SRCROOT)/Native", "$(SRCROOT)/Keyboard"], OTHER_LDFLAGS=["$(inherited)", "-all_load", "-lc++", "-liconv", "-lz"],
            SWIFT_OBJC_BRIDGING_HEADER="Keyboard/Keyboard-Bridging-Header.h")
    targets = []
    for name, sources, target_resources, product_type in [
        ("XiaomaodouKeyboard", shared + keyboard, resources[:-1], "com.apple.product-type.app-extension"),
        ("Xiaomaodou", shared + ["App/XiaomaodouApp.swift"], image_resources + ["Resources/Notices", "Resources/Assets.xcassets"], "com.apple.product-type.application"),
        ("XiaomaodouTests", shared + keyboard + ["Tests/KeyboardTests.swift"], resources[:-1], "com.apple.product-type.bundle.unit-test")]:
        settings = dict(PRODUCT_NAME="$(TARGET_NAME)", PRODUCT_BUNDLE_IDENTIFIER="org.leavesandstone.xiaomaodou" +
                        (".keyboard" if name.endswith("Keyboard") else ".tests" if name.endswith("Tests") else ""),
                        LD_RUNPATH_SEARCH_PATHS=["$(inherited)", "@executable_path/Frameworks", "@executable_path/../../Frameworks"])
        if name != "Xiaomaodou":
            settings.update(native_settings)
        if name == "XiaomaodouKeyboard":
            settings.update(APPLICATION_EXTENSION_API_ONLY="YES", SKIP_INSTALL="YES", INFOPLIST_FILE="Configuration/Keyboard.plist")
        elif name == "XiaomaodouTests":
            settings.update(GENERATE_INFOPLIST_FILE="YES", TEST_HOST="$(BUILT_PRODUCTS_DIR)/Xiaomaodou.app/Xiaomaodou",
                            BUNDLE_LOADER="$(TEST_HOST)", SKIP_INSTALL="YES")
        else:
            settings.update(INFOPLIST_FILE="Configuration/App.plist", ASSETCATALOG_COMPILER_APPICON_NAME="AppIcon")
        phases = [phase(name + ":sources", "PBXSourcesBuildPhase", sources),
                  phase(name + ":resources", "PBXResourcesBuildPhase", target_resources),
                  phase(name + ":frameworks", "PBXFrameworksBuildPhase", [] if name == "Xiaomaodou" else ["Vendor/librime-static.xcframework"])]
        dependencies = []
        if name != "XiaomaodouKeyboard":
            dependency = "XiaomaodouKeyboard" if name == "Xiaomaodou" else "Xiaomaodou"
            proxy = obj(name + ":proxy", "PBXContainerItemProxy", containerPortal=uid("project"), proxyType="1",
                        remoteGlobalIDString=uid(dependency + ":target"), remoteInfo=dependency)
            dependencies.append(obj(name + ":dependency", "PBXTargetDependency", target=uid(dependency + ":target"), targetProxy=proxy))
        if name == "Xiaomaodou":
            build_file = obj("embedKeyboard", "PBXBuildFile", fileRef=products["XiaomaodouKeyboard"],
                             settings=dict(ATTRIBUTES=["RemoveHeadersOnCopy"]))
            phases.append(obj("embedExtensions", "PBXCopyFilesBuildPhase", buildActionMask="2147483647",
                dstPath="", dstSubfolderSpec="13", name="Embed App Extensions", files=[build_file], runOnlyForDeploymentPostprocessing="0"))
        targets.append(obj(name + ":target", "PBXNativeTarget", name=name, productName=name,
            productReference=products[name], productType=product_type, buildConfigurationList=configs(name, settings),
            buildPhases=phases, buildRules=[], dependencies=dependencies))
    project = obj("project", "PBXProject", buildConfigurationList=configs("Project", {}), compatibilityVersion="Xcode 14.0",
        developmentRegion="zh-Hans", knownRegions=["zh-Hans", "en", "Base"], mainGroup=main_group,
        productRefGroup=product_group, projectDirPath="", projectRoot="", targets=targets,
        attributes=dict(LastUpgradeCheck="1600", BuildIndependentTargetsInParallel="YES"))
    text = "// !$*UTF8*$!\n" + encode(dict(archiveVersion="1", classes={}, objectVersion="56", objects=objects, rootObject=project)) + "\n"
    (project_dir / "project.pbxproj").write_text(text, encoding="utf-8")
    scheme = ET.Element("Scheme", LastUpgradeVersion="1600", version="1.3")
    build = ET.SubElement(scheme, "BuildAction", parallelizeBuildables="YES", buildImplicitDependencies="YES")
    entries = ET.SubElement(build, "BuildActionEntries")
    def reference(parent, name, ext):
        ET.SubElement(parent, "BuildableReference", BuildableIdentifier="primary", BlueprintIdentifier=uid(name + ":target"),
            BuildableName=name + ext, BlueprintName=name, ReferencedContainer="container:Xiaomaodou.xcodeproj")
    entry = ET.SubElement(entries, "BuildActionEntry", buildForTesting="YES", buildForRunning="YES",
        buildForProfiling="YES", buildForArchiving="YES", buildForAnalyzing="YES")
    reference(entry, "Xiaomaodou", ".app")
    test = ET.SubElement(scheme, "TestAction", buildConfiguration="Debug", selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB",
                         selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB", shouldUseLaunchSchemeArgsEnv="YES")
    testable = ET.SubElement(ET.SubElement(test, "Testables"), "TestableReference", skipped="NO", parallelizable="NO")
    reference(testable, "XiaomaodouTests", ".xctest")
    launch = ET.SubElement(scheme, "LaunchAction", buildConfiguration="Debug", selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB",
        selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB", launchStyle="0", useCustomWorkingDirectory="NO", ignoresPersistentStateOnLaunch="NO",
        debugDocumentVersioning="YES", debugServiceExtension="internal", allowLocationSimulation="YES")
    reference(ET.SubElement(launch, "BuildableProductRunnable", runnableDebuggingMode="0"), "Xiaomaodou", ".app")
    ET.SubElement(scheme, "AnalyzeAction", buildConfiguration="Debug")
    ET.SubElement(scheme, "ArchiveAction", buildConfiguration="Release", revealArchiveInOrganizer="YES")
    scheme_dir = project_dir / "xcshareddata" / "xcschemes"
    scheme_dir.mkdir(parents=True, exist_ok=True)
    ET.indent(scheme)
    ET.ElementTree(scheme).write(scheme_dir / "Xiaomaodou.xcscheme", encoding="utf-8", xml_declaration=True)
    print("Native app, keyboard extension and hosted test project generated")


if __name__ == "__main__":
    main()
