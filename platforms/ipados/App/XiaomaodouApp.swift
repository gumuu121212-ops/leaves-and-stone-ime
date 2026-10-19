import SwiftUI

@main
struct XiaomaodouApp: App {
    var body: some Scene { WindowGroup { SetupView() } }
}

struct DogPortrait: UIViewRepresentable {
    func makeUIView(context: Context) -> MascotView { MascotView() }
    func updateUIView(_ view: MascotView, context: Context) {}
}

struct SetupView: View {
    @State private var sample = ""
    @State private var showLicenses = false
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                VStack(spacing: 10) {
                    DogPortrait().frame(width: 220, height: 122)
                    Text("小毛豆输入法").font(.largeTitle.bold())
                    Text("陪你慢慢打字").foregroundColor(Color(MaodouTheme.muted))
                }.frame(maxWidth: .infinity).padding(.top, 24)
                VStack(alignment: .leading, spacing: 16) {
                    Text("把小毛豆加到键盘").font(.title2.bold())
                    Text("1. 打开「设置」→「通用」→「键盘」→「键盘」。")
                    Text("2. 点「添加新键盘」，选择「小毛豆」。")
                    Text("3. 回到输入框，长按地球图标，切换到「小毛豆」。")
                    Button("打开系统设置") {
                        guard let url = URL(string: UIApplication.openSettingsURLString) else { return }
                        UIApplication.shared.open(url)
                    }.buttonStyle(.borderedProminent).tint(Color(MaodouTheme.muted))
                    Text("离线拼音，无需开启「允许完全访问」。密码框或部分 App 会使用系统键盘。")
                        .font(.callout).foregroundColor(Color(MaodouTheme.muted))
                }.padding(24).background(Color.white.opacity(0.65)).cornerRadius(24)
                VStack(alignment: .leading, spacing: 12) {
                    Text("试着和小毛豆打个招呼").font(.title3.bold())
                    TextEditor(text: $sample).frame(minHeight: 150).padding(12)
                        .background(Color.white).cornerRadius(16)
                    Text("输入 nihao 后点「你好」，或按空格选词。中 / A 切换中英文；候选词可以左右滑动。")
                        .font(.callout).foregroundColor(Color(MaodouTheme.muted))
                }
                Button("开源许可与素材说明") { showLicenses = true }
                    .foregroundColor(Color(MaodouTheme.muted))
            }.padding(32).frame(maxWidth: 760).frame(maxWidth: .infinity)
        }.background(Color(MaodouTheme.background).ignoresSafeArea())
            .foregroundColor(Color(MaodouTheme.ink))
            .sheet(isPresented: $showLicenses) { LicenseView() }
    }
}

struct LicenseView: View {
    @Environment(\.dismiss) private var dismiss
    private var notices: String {
        let manager = FileManager.default
        guard let root = Bundle.main.url(forResource: "Notices", withExtension: nil),
              let files = manager.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey]) else { return "许可文件未找到" }
        return files.compactMap { $0 as? URL }.sorted { $0.path < $1.path }.compactMap { file in
            guard ["txt", "md", "", "license"].contains(file.pathExtension.lowercased()),
                  (try? file.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true,
                  let text = try? String(contentsOf: file, encoding: .utf8) else { return nil }
            return file.lastPathComponent + "\n\n" + text
        }.joined(separator: "\n\n")
    }
    var body: some View {
        NavigationView {
            ScrollView { Text(notices).font(.system(size: 13, design: .monospaced)).padding() }
                .navigationTitle("开源许可")
                .toolbar { Button("完成") { dismiss() } }
        }.navigationViewStyle(.stack)
    }
}
