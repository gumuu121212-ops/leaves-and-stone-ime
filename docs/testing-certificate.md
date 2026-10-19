# 测试签名证书说明

本仓库不保存任何私钥。测试包用一次性自签名证书签名，证书只用于让 Windows 在安装时提示
“已验证的发布者”，不代表任何商业信任。

## 生成

```powershell
.\scripts\new-test-certificate.ps1
```

- 私钥（`.pfx`）默认落在 `%USERPROFILE%\Documents\LeavesAndStone-test-signing\`，脚本会拒绝把 PFX 放进仓库任何位置。
- 公钥证书（`.cer`）导出到 `output\certs\`，可以跟着测试包一起发给测试者。
- 脚本最后会打印 thumbprint 与撤销命令，记下来。

## 信任（需要用户明确确认）

```powershell
Import-Certificate -FilePath output\certs\leaves-and-stone-test-signing.cer -CertStoreLocation Cert:\CurrentUser\Root
```

把证书装进“受信任的根证书颁发机构”等于让这张证书签名的任何程序被本机默认信任。
**必须先向使用者说明这一点并取得同意**，不要静默安装。

## 签名

```powershell
.\scripts\package.ps1 -Sign -Thumbprint <thumbprint>
# 或者用 PFX
.\scripts\package.ps1 -Sign -PfxPath <仓库外的 pfx> -PfxPassword (Read-Host -AsSecureString)
```

签名覆盖 `output\*.exe`、`output\*.dll`、`output\Win32\*` 与 `output\archives\*-installer.exe`，
逐个用 `signtool verify /pa` 复核；任一失败即整体失败。

## 撤销

```powershell
Get-ChildItem Cert:\CurrentUser\My, Cert:\CurrentUser\Root |
  Where-Object Thumbprint -eq <thumbprint> | Remove-Item
Remove-Item <pfx 路径>
```

卸载输入法不会移除这张证书，必须单独撤销。

## 交付时必须同时给出

1. 安装包与 `SHA256SUMS.txt`（`scripts\package.ps1` 生成）。
2. 公开证书 `.cer` 与它的 SHA256。
3. 上面的撤销步骤。
4. 明确说明：未签名或自签名的包只用于测试，不要当作正式发布。
