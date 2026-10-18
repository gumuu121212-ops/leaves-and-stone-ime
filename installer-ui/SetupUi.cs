// Custom installer front end for Leaves & Stone.
//
// Everything is painted by this file so the wizard can match the supplied
// artwork exactly: warm paper background, a rounded card holding the mascot,
// pill buttons and a three step header. The actual installation still runs the
// proven NSIS package, embedded in this executable, in silent mode.
using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

namespace LeavesStoneSetup
{
    internal static class Palette
    {
        public static readonly Color Background = Color.FromArgb(250, 246, 240);
        public static readonly Color Card = Color.FromArgb(243, 233, 221);
        public static readonly Color Ink = Color.FromArgb(62, 47, 38);
        public static readonly Color Muted = Color.FromArgb(138, 118, 102);
        public static readonly Color Accent = Color.FromArgb(213, 189, 167);
        public static readonly Color AccentDark = Color.FromArgb(196, 168, 142);
        public static readonly Color Field = Color.FromArgb(255, 253, 250);
        public static readonly Color Outline = Color.FromArgb(223, 208, 191);
        public static readonly Color StepActive = Color.FromArgb(232, 217, 201);
    }

    internal sealed class SetupWindow : Form
    {
        private const int W = 980;
        private const int H = 660;
        private const int CardX = 18;
        private const int CardY = 18;
        private const int CardW = 250;
        private const int ContentX = 300;
        // The wizard is drawn on a 980x660 canvas and scaled up so the window
        // matches the size of the reference design.
        private const float K = 1.38f;

        private readonly Image _dogCrop;

        private readonly Rectangle _browseRect = new Rectangle(880, 232, 84, 42);
        private readonly Rectangle _fieldRect = new Rectangle(300, 232, 566, 42);
        private readonly Rectangle _backRect = new Rectangle(560, 594, 112, 38);
        private readonly Rectangle _installRect = new Rectangle(684, 594, 112, 38);
        private readonly Rectangle _cancelRect = new Rectangle(808, 594, 112, 38);

        private readonly TextBox _dirBox;
        private string _hover = "";
        private int _page;            // 0 location, 1 progress, 2 finished
        private int _marquee;
        private readonly Timer _timer;
        private string _status = "";

        public SetupWindow(string installDir, Image dog)
        {
            _dogCrop = CropMascot(dog);
            Text = "Leaves & Stone 安装";
            ClientSize = new Size((int)(W * K), (int)(H * K));
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Palette.Background;
            DoubleBuffered = true;
            Font = new Font("Microsoft YaHei UI", 14f, FontStyle.Regular, GraphicsUnit.Pixel);

            _dirBox = new TextBox
            {
                Text = installDir,
                BorderStyle = BorderStyle.None,
                BackColor = Palette.Field,
                ForeColor = Palette.Ink,
                Font = new Font("Microsoft YaHei UI", 15f, FontStyle.Regular, GraphicsUnit.Pixel),
                Bounds = new Rectangle((int)((_fieldRect.X + 14) * K),
                                       (int)((_fieldRect.Y + 12) * K),
                                       (int)((_fieldRect.Width - 28) * K),
                                       (int)(20 * K))
            };
            _dirBox.TextChanged += delegate { Invalidate(); };
            Controls.Add(_dirBox);

            _timer = new Timer { Interval = 30 };
            _timer.Tick += delegate
            {
                _marquee = (_marquee + 14) % (W + 200);
                Invalidate();
            };
        }

        private static Image CropMascot(Image source)
        {
            if (source == null) return null;
            // The avatar carries a lot of empty space; keep the content box.
            Rectangle box = new Rectangle(342, 450, 340, 188);
            if (box.Right > source.Width || box.Bottom > source.Height)
                box = new Rectangle(0, 0, source.Width, source.Height);
            Bitmap cropped = new Bitmap(box.Width, box.Height);
            using (Graphics g = Graphics.FromImage(cropped))
                g.DrawImage(source, new Rectangle(0, 0, box.Width, box.Height),
                            box, GraphicsUnit.Pixel);
            return cropped;
        }

        // ---------------------------------------------------------------- input

        protected override void OnMouseMove(MouseEventArgs e)
        {
            string hover = "";
            if (_page == 0 && _browseRect.Contains(e.Location)) hover = "browse";
            if (_page != 1 && _backRect.Contains(e.Location)) hover = "back";
            if (_page == 0 && _installRect.Contains(e.Location)) hover = "install";
            if (_page != 1 && _cancelRect.Contains(e.Location)) hover = "cancel";
            if (hover != _hover)
            {
                _hover = hover;
                Cursor = hover.Length == 0 ? Cursors.Default : Cursors.Hand;
                Invalidate();
            }
            base.OnMouseMove(e);
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            _hover = "";
            Cursor = Cursors.Default;
            Invalidate();
            base.OnMouseLeave(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            if (_page == 0 && _browseRect.Contains(e.Location)) Browse();
            else if (_page != 1 && _backRect.Contains(e.Location)) SetPage(0);
            else if (_page == 0 && _installRect.Contains(e.Location)) Install();
            else if (_page != 1 && _cancelRect.Contains(e.Location)) Close();
            base.OnMouseUp(e);
        }

        private void Browse()
        {
            using (FolderBrowserDialog dialog = new FolderBrowserDialog())
            {
                dialog.Description = "选择安装位置";
                dialog.SelectedPath = _dirBox.Text;
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    _dirBox.Text = dialog.SelectedPath;
            }
        }

        // ----------------------------------------------------------------- flow

        private void SetPage(int page)
        {
            _page = page;
            _dirBox.Visible = page == 0;
            if (page == 1) _timer.Start(); else _timer.Stop();
            Invalidate();
        }

        private void Install()
        {
            string target = _dirBox.Text.Trim();
            if (target.Length == 0)
            {
                MessageBox.Show(this, "请先选择安装目录。", "Leaves & Stone 安装");
                return;
            }
            _status = "正在复制文件并注册输入法…";
            SetPage(1);
            try
            {
                string core = Path.Combine(Path.GetTempPath(),
                                           "leaves-stone-setup-core.exe");
                using (Stream stream = Assembly.GetExecutingAssembly()
                           .GetManifestResourceStream("CoreInstaller"))
                using (FileStream file = File.Create(core))
                    stream.CopyTo(file);
                ProcessStartInfo info = new ProcessStartInfo(core)
                {
                    Arguments = "/S /D=" + target,
                    UseShellExecute = false
                };
                using (Process process = Process.Start(info))
                    process.WaitForExit();
                SetPage(2);
            }
            catch (Exception error)
            {
                MessageBox.Show(this, "安装失败：" + error.Message, "Leaves & Stone 安装");
                SetPage(0);
            }
        }

        // ---------------------------------------------------------------- paint

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;
            g.Clear(Palette.Background);
            g.ScaleTransform(K, K);

            DrawCard(g);
            DrawHeader(g);
            if (_page == 0) DrawLocation(g);
            else if (_page == 1) DrawProgress(g);
            else DrawFinished(g);
            DrawFooter(g);
        }

        private void DrawCard(Graphics g)
        {
            Rectangle card = new Rectangle(CardX, CardY, CardW, H - CardY * 2);
            using (GraphicsPath path = Rounded(card, 22))
            using (SolidBrush brush = new SolidBrush(Palette.Card))
                g.FillPath(brush, path);

            using (Font brand = new Font("Microsoft YaHei UI", 26f, FontStyle.Bold, GraphicsUnit.Pixel))
            using (Font sub = new Font("Microsoft YaHei UI", 12f, FontStyle.Regular, GraphicsUnit.Pixel))
            using (SolidBrush ink = new SolidBrush(Palette.Ink))
            using (SolidBrush muted = new SolidBrush(Palette.Muted))
            {
                g.DrawString("Leaves & Stone", brand, ink, 38, 54);
                g.DrawString("输入法安装向导", sub, muted, 39, 88);
            }

            Rectangle pill = new Rectangle(38, 116, 82, 26);
            using (GraphicsPath path = Rounded(pill, 13))
            using (SolidBrush brush = new SolidBrush(Palette.Background))
                g.FillPath(brush, path);
            using (Font f = new Font("Microsoft YaHei UI", 11f, FontStyle.Regular, GraphicsUnit.Pixel))
            using (SolidBrush muted = new SolidBrush(Palette.Muted))
                g.DrawString("0.17.4", f, muted, 52, 121);

            if (_dogCrop != null)
            {
                int width = CardW - 30;
                int height = width * _dogCrop.Height / _dogCrop.Width;
                g.DrawImage(_dogCrop,
                            new Rectangle(CardX + 15, card.Bottom - height - 72,
                                          width, height));
            }

            // Paw mark under the mascot.
            using (SolidBrush paw = new SolidBrush(Color.FromArgb(214, 196, 176)))
            {
                g.FillEllipse(paw, CardX + 118, card.Bottom - 44, 15, 13);
                g.FillEllipse(paw, CardX + 106, card.Bottom - 58, 7, 8);
                g.FillEllipse(paw, CardX + 116, card.Bottom - 62, 7, 8);
                g.FillEllipse(paw, CardX + 126, card.Bottom - 58, 7, 8);
            }
        }

        private void DrawHeader(Graphics g)
        {
            string title = _page == 0 ? "选择安装位置"
                         : _page == 1 ? "正在安装" : "安装完成";
            string subtitle = _page == 0
                ? "选择 Leaves & Stone 的安装文件夹。"
                : _page == 1 ? "请稍候，正在复制文件并注册输入法。"
                             : "Leaves & Stone 已安装完成，可以开始使用了。";

            using (Font f = new Font("Microsoft YaHei UI", 30f, FontStyle.Bold, GraphicsUnit.Pixel))
            using (SolidBrush ink = new SolidBrush(Palette.Ink))
                g.DrawString(title, f, ink, ContentX, 44);
            using (Font f = new Font("Microsoft YaHei UI", 13f, FontStyle.Regular, GraphicsUnit.Pixel))
            using (SolidBrush muted = new SolidBrush(Palette.Muted))
                g.DrawString(subtitle, f, muted, ContentX + 2, 92);

            string[] labels = { "安装位置", "正在安装", "完成" };
            int x = ContentX;
            for (int index = 0; index < labels.Length; ++index)
            {
                using (Font f = new Font("Microsoft YaHei UI", 13f, FontStyle.Regular, GraphicsUnit.Pixel))
                {
                    string text = (index < _page ? "✓ " : (index + 1) + "  ") + labels[index];
                    Size size = TextRenderer.MeasureText(text, f);
                    Rectangle box = new Rectangle(x, 140, size.Width + 26, 34);
                    if (index == _page)
                    {
                        using (GraphicsPath path = Rounded(box, 17))
                        using (SolidBrush brush = new SolidBrush(Palette.StepActive))
                            g.FillPath(brush, path);
                    }
                    using (SolidBrush brush = new SolidBrush(
                               index == _page ? Palette.Ink : Palette.Muted))
                        g.DrawString(text, f, brush, box.X + 14, box.Y + 8);
                    x = box.Right + 12;
                }
            }
        }

        private void DrawLocation(Graphics g)
        {
            using (Font f = new Font("Microsoft YaHei UI", 14f, FontStyle.Regular, GraphicsUnit.Pixel))
            using (SolidBrush ink = new SolidBrush(Palette.Ink))
            using (SolidBrush muted = new SolidBrush(Palette.Muted))
            {
                g.DrawString("安装目录", f, ink, ContentX, 206);

                using (GraphicsPath path = Rounded(_fieldRect, 12))
                {
                    using (SolidBrush brush = new SolidBrush(Palette.Field))
                        g.FillPath(brush, path);
                    using (Pen pen = new Pen(Palette.Outline))
                        g.DrawPath(pen, path);
                }
                DrawButton(g, _browseRect, "浏览…", false, _hover == "browse");

                using (Font small = new Font("Microsoft YaHei UI", 11f, FontStyle.Regular, GraphicsUnit.Pixel))
                    g.DrawString("点击「浏览」可选择其他文件夹。", small, muted,
                                 ContentX + 2, 288);

                Rectangle info = new Rectangle(ContentX, 336, 676, 96);
                using (GraphicsPath path = Rounded(info, 16))
                using (SolidBrush brush = new SolidBrush(Palette.Card))
                    g.FillPath(brush, path);

                using (Font small = new Font("Microsoft YaHei UI", 11f, FontStyle.Regular, GraphicsUnit.Pixel))
                using (Font value = new Font("Microsoft YaHei UI", 19f, FontStyle.Regular, GraphicsUnit.Pixel))
                {
                    g.DrawString("所需空间", small, muted, info.X + 30, info.Y + 20);
                    g.DrawString("约 91 MB", value, ink, info.X + 30, info.Y + 42);
                    g.DrawString("可用空间", small, muted, info.X + 330, info.Y + 20);
                    g.DrawString(FreeSpace(), value, ink, info.X + 330, info.Y + 42);
                }
            }
        }

        private static string FreeSpace()
        {
            try
            {
                DriveInfo drive = new DriveInfo(Path.GetPathRoot(
                    Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles)));
                return (drive.AvailableFreeSpace / (1024L * 1024L * 1024L)) + " GB";
            }
            catch
            {
                return "-";
            }
        }

        private void DrawProgress(Graphics g)
        {
            Rectangle track = new Rectangle(ContentX, 300, 676, 14);
            using (GraphicsPath path = Rounded(track, 7))
            using (SolidBrush brush = new SolidBrush(Palette.Card))
                g.FillPath(brush, path);
            Rectangle bar = new Rectangle(ContentX + _marquee - 180, 300, 180, 14);
            bar.Intersect(track);
            if (bar.Width > 4)
            {
                using (GraphicsPath path = Rounded(bar, 7))
                using (SolidBrush brush = new SolidBrush(Palette.Accent))
                    g.FillPath(brush, path);
            }
            using (Font f = new Font("Microsoft YaHei UI", 13f, FontStyle.Regular, GraphicsUnit.Pixel))
            using (SolidBrush muted = new SolidBrush(Palette.Muted))
                g.DrawString(_status, f, muted, ContentX, 332);
        }

        private void DrawFinished(Graphics g)
        {
            using (Font f = new Font("Microsoft YaHei UI", 15f, FontStyle.Regular, GraphicsUnit.Pixel))
            using (SolidBrush ink = new SolidBrush(Palette.Ink))
            using (SolidBrush muted = new SolidBrush(Palette.Muted))
            {
                g.DrawString("输入法已经注册到系统。", f, ink, ContentX, 210);
                g.DrawString("按下 Win + 空格 可以在「Leaves & Stone」和微软拼音之间切换。",
                             f, muted, ContentX, 244);
                g.DrawString("右键托盘里的图标可以打开设置、重新部署或卸载。",
                             f, muted, ContentX, 278);
            }
        }

        private void DrawFooter(Graphics g)
        {
            using (Pen pen = new Pen(Palette.Outline))
                g.DrawLine(pen, ContentX, 566, W - 28, 566);

            using (Font f = new Font("Microsoft YaHei UI", 12f, FontStyle.Regular, GraphicsUnit.Pixel))
            using (SolidBrush muted = new SolidBrush(Palette.Muted))
            {
                if (_page == 2)
                    g.DrawString("Leaves & Stone 0.17.4", f, muted, 28, 592);
            }

            if (_page != 1)
                DrawButton(g, _backRect, "上一步", false, _hover == "back");
            if (_page == 0)
                DrawButton(g, _installRect, "安装", true, _hover == "install");
            if (_page != 1)
                DrawButton(g, _cancelRect, _page == 2 ? "完成" : "取消", false,
                           _hover == "cancel");
        }

        private void DrawButton(Graphics g, Rectangle rect, string text,
                                bool primary, bool hover)
        {
            using (GraphicsPath path = Rounded(rect, rect.Height / 2))
            {
                Color fill = primary
                    ? (hover ? Palette.AccentDark : Palette.Accent)
                    : (hover ? Color.FromArgb(252, 248, 242) : Color.White);
                using (SolidBrush brush = new SolidBrush(fill))
                    g.FillPath(brush, path);
                if (!primary)
                {
                    using (Pen pen = new Pen(Palette.Outline))
                        g.DrawPath(pen, path);
                }
            }
            using (Font f = new Font("Microsoft YaHei UI", 14f, FontStyle.Regular, GraphicsUnit.Pixel))
            using (SolidBrush brush = new SolidBrush(
                       primary ? Color.FromArgb(58, 44, 34) : Palette.Ink))
            using (StringFormat format = new StringFormat())
            {
                format.Alignment = StringAlignment.Center;
                format.LineAlignment = StringAlignment.Center;
                g.DrawString(text, f, brush, rect, format);
            }
        }

        private static GraphicsPath Rounded(Rectangle rect, int radius)
        {
            GraphicsPath path = new GraphicsPath();
            int d = Math.Max(2, radius * 2);
            if (d > rect.Width) d = rect.Width;
            if (d > rect.Height) d = rect.Height;
            path.AddArc(rect.X, rect.Y, d, d, 180, 90);
            path.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
            path.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
            path.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
            path.CloseFigure();
            return path;
        }
    }

    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            Image dog = LoadImage("DogImage");
            string installDir = ReadInstallDir();

            if (args != null && args.Length == 2 && args[0] == "--render")
            {
                using (SetupWindow window = new SetupWindow(installDir, dog))
                {
                    window.CreateControl();
                    using (Bitmap bitmap = new Bitmap(window.ClientSize.Width,
                                                      window.ClientSize.Height))
                    {
                        window.DrawToBitmap(bitmap, new Rectangle(0, 0,
                            bitmap.Width, bitmap.Height));
                        bitmap.Save(args[1], System.Drawing.Imaging.ImageFormat.Png);
                    }
                }
                return;
            }

            Application.Run(new SetupWindow(installDir, dog));
        }

        private static Image LoadImage(string name)
        {
            using (Stream stream = Assembly.GetExecutingAssembly()
                       .GetManifestResourceStream(name))
            {
                if (stream == null) return null;
                return Image.FromStream(stream);
            }
        }

        private static string ReadInstallDir()
        {
            try
            {
                using (Microsoft.Win32.RegistryKey key = Microsoft.Win32.Registry.LocalMachine
                           .OpenSubKey(@"Software\WOW6432Node\LeavesAndStone\IME"))
                {
                    object value = key == null ? null : key.GetValue("InstallDir");
                    if (value is string && ((string)value).Length > 0)
                        return (string)value;
                }
            }
            catch
            {
            }
            return Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
                "Leaves & Stone");
        }
    }
}
