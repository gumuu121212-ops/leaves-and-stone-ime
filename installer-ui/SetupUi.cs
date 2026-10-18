// Custom installer front end for Leaves & Stone.
//
// The wizard is rendered from the supplied layout.json: every page lists its
// elements with absolute coordinates, colours and per-state styling, so the
// window matches the design instead of a hand-tuned approximation. The actual
// installation still runs the proven NSIS package, embedded in this executable,
// in silent mode.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace LeavesStoneSetup
{
    internal sealed class SetupWindow : Form
    {
        private readonly Dictionary<string, object> _layout;
        private readonly Dictionary<string, Image> _images =
            new Dictionary<string, Image>();
        private readonly float _canvasWidth;
        private readonly float _canvasHeight;
        private readonly float _scale;
        private readonly string _installDir;
        private readonly TextBox _field;
        private readonly System.Windows.Forms.Timer _progressTimer =
            new System.Windows.Forms.Timer { Interval = 120 };

        private string _page = "directory";
        private string _hover = "";
        private bool _pressed;
        private bool _installing;
        private string _result;
        private double _progress = 0.37;

        // Optional keys must not throw: the indexer raises KeyNotFoundException,
        // which used to be swallowed together with the whole element.
        private static object Obj(Dictionary<string, object> d, string key)
        {
            object value;
            return d != null && d.TryGetValue(key, out value) ? value : null;
        }

        private static Dictionary<string, object> Dict(object value)
        {
            return value as Dictionary<string, object>;
        }

        // JavaScriptSerializer returns object[] for JSON arrays, not List<object>.
        private static List<object> Seq(object value)
        {
            List<object> direct = value as List<object>;
            if (direct != null) return direct;
            System.Collections.IEnumerable items = value as System.Collections.IEnumerable;
            if (items == null || value is string) return null;
            List<object> result = new List<object>();
            foreach (object item in items) result.Add(item);
            return result;
        }

        private static string Str(Dictionary<string, object> d, string key,
                                  string fallback = "")
        {
            object v;
            return d != null && d.TryGetValue(key, out v) && v != null
                       ? Convert.ToString(v) : fallback;
        }

        private static double Num(Dictionary<string, object> d, string key,
                                  double fallback = 0)
        {
            object v;
            if (d == null || !d.TryGetValue(key, out v) || v == null) return fallback;
            try { return Convert.ToDouble(v); }
            catch { return fallback; }
        }

        private static bool Flag(Dictionary<string, object> d, string key, bool fallback)
        {
            object v;
            if (d == null || !d.TryGetValue(key, out v) || v == null) return fallback;
            try { return Convert.ToBoolean(v); }
            catch { return fallback; }
        }

        private static RectangleF RectOf(Dictionary<string, object> element)
        {
            List<object> r = element != null ? Seq(Obj(element, "rect")) : null;
            if (r == null || r.Count < 4) return RectangleF.Empty;
            try
            {
                RectangleF rect = new RectangleF(Convert.ToSingle(r[0]),
                                                 Convert.ToSingle(r[1]),
                                                 Convert.ToSingle(r[2]),
                                                 Convert.ToSingle(r[3]));
                if (float.IsNaN(rect.X) || float.IsNaN(rect.Y) ||
                    float.IsNaN(rect.Width) || float.IsNaN(rect.Height) ||
                    rect.Width < 0 || rect.Height < 0)
                    return RectangleF.Empty;
                return rect;
            }
            catch
            {
                return RectangleF.Empty;
            }
        }

        private static Color Hex(string value, Color fallback)
        {
            if (string.IsNullOrEmpty(value) || value.Length < 7 || value[0] != '#')
                return fallback;
            try
            {
                int r = Convert.ToInt32(value.Substring(1, 2), 16);
                int g = Convert.ToInt32(value.Substring(3, 2), 16);
                int b = Convert.ToInt32(value.Substring(5, 2), 16);
                return Color.FromArgb(r, g, b);
            }
            catch
            {
                return fallback;
            }
        }

        public SetupWindow(Dictionary<string, object> layout, string installDir)
        {
            _layout = layout;
            _installDir = installDir;

            List<object> canvas = Seq(layout["canvas"]);
            _canvasWidth = Convert.ToSingle(canvas[0]);
            _canvasHeight = Convert.ToSingle(canvas[1]);

            Rectangle work = Screen.PrimaryScreen.WorkingArea;
            float dpi = 96f;
            using (Bitmap probe = new Bitmap(1, 1))
            using (Graphics surface = Graphics.FromImage(probe))
                dpi = surface.DpiX;
            float fit = Math.Min(work.Width / _canvasWidth, work.Height / _canvasHeight);
            // Never exceed the work area, and never grow past 1.35 of the
            // design size: a 200% desktop would otherwise ask for 2560 px.
            _scale = Math.Min(Math.Min(dpi / 96f, fit * 0.96f), 1.35f);
            if (_scale <= 0.1f || float.IsNaN(_scale)) _scale = 1f;

            Text = "Leaves & Stone 安装";
            FormBorderStyle = FormBorderStyle.None;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Hex(Str(Dict(layout["colors"]), "background"),
                            Color.FromArgb(251, 248, 241));
            ClientSize = new Size((int)(_canvasWidth * _scale),
                                  (int)(_canvasHeight * _scale));
            DoubleBuffered = true;
            ApplyRoundedRegion();

            LoadImages();
            _field = BuildField();
            if (_field != null) Controls.Add(_field);

            _progressTimer.Tick += delegate
            {
                if (_progress < 0.94) _progress += 0.01;
                Invalidate();
            };
        }

        // The design has a 30 px rounded frame. Clip the window to that shape so
        // the corners are real, then paint the border just inside it because a
        // stroke on the exact edge is half clipped.
        private void ApplyRoundedRegion()
        {
            int width = ClientSize.Width;
            int height = ClientSize.Height;
            if (width < 8 || height < 8) return;
            float radius = Math.Min(30f * _scale, Math.Min(width, height) / 2f);
            using (GraphicsPath path = new GraphicsPath())
            {
                path.AddArc(0, 0, radius * 2, radius * 2, 180, 90);
                path.AddArc(width - radius * 2, 0, radius * 2, radius * 2, 270, 90);
                path.AddArc(width - radius * 2, height - radius * 2,
                            radius * 2, radius * 2, 0, 90);
                path.AddArc(0, height - radius * 2, radius * 2, radius * 2, 90, 90);
                path.CloseFigure();
                Region = new Region(path);
            }
        }

        private void DrawFrame(Graphics g)
        {
            using (Pen pen = new Pen(Color.FromArgb(0xCD, 0xBE, 0xAA), 2f))
            using (GraphicsPath path = Rounded(
                       new RectangleF(1, 1, _canvasWidth - 2, _canvasHeight - 2), 29f))
                g.DrawPath(pen, path);
        }

        private void LoadImages()
        {
            foreach (string name in new[] { "mascot", "paw", "cushion", "titlebar-dog" })
            {
                using (Stream stream = Assembly.GetExecutingAssembly()
                           .GetManifestResourceStream("img-" + name))
                {
                    if (stream == null) continue;
                    // Image.FromStream keeps using the stream, so take an owned
                    // copy before it is disposed.
                    using (Image loaded = Image.FromStream(stream))
                        _images[name] = new Bitmap(loaded);
                }
            }
        }

        private TextBox BuildField()
        {
            foreach (Dictionary<string, object> element in Elements())
            {
                if (Str(element, "type") != "input") continue;
                RectangleF rect = RectOf(element);
                List<object> padding = Seq(Obj(element, "padding"));
                float left = padding != null && padding.Count >= 4
                                 ? Convert.ToSingle(padding[3]) : 20f;
                return new TextBox
                {
                    Text = _installDir,
                    BorderStyle = BorderStyle.None,
                    BackColor = Hex(Str(element, "background"), Color.White),
                    ForeColor = Hex(Str(element, "color"), Color.Black),
                    Font = new Font(Str(element, "font", "Microsoft YaHei UI"),
                                    (float)Num(element, "size", 24),
                                    FontStyle.Regular, GraphicsUnit.Pixel),
                    Bounds = new Rectangle(
                        (int)((rect.X + left) * _scale),
                        (int)((rect.Y + rect.Height / 2 - 18) * _scale),
                        Math.Max(40, (int)((rect.Width - left * 2) * _scale)),
                        (int)(36 * _scale))
                };
            }
            return null;
        }

        private List<Dictionary<string, object>> Elements()
        {
            List<Dictionary<string, object>> result =
                new List<Dictionary<string, object>>();
            Dictionary<string, object> pages = Dict(_layout["pages"]);
            Dictionary<string, object> page = pages != null ? Dict(pages[_page]) : null;
            List<object> elements = page != null ? Seq(page["elements"]) : null;
            if (elements == null) return result;
            foreach (object item in elements)
            {
                Dictionary<string, object> element = Dict(item);
                if (element != null) result.Add(element);
            }
            return result;
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            PointF p = new PointF(e.X / _scale, e.Y / _scale);
            string hover = "";
            foreach (Dictionary<string, object> element in Elements())
            {
                string type = Str(element, "type");
                if (type != "button" && type != "icon-button") continue;
                if (!Flag(element, "visible", true) || !Flag(element, "enabled", true))
                    continue;
                if (RectOf(element).Contains(p)) { hover = Str(element, "id"); break; }
            }
            if (hover != _hover)
            {
                _hover = hover;
                Cursor = hover.Length == 0 ? Cursors.Default : Cursors.Hand;
                Invalidate();
            }
            base.OnMouseMove(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            PointF p = new PointF(e.X / _scale, e.Y / _scale);
            foreach (Dictionary<string, object> element in Elements())
            {
                if (Str(element, "type") == "drag-region" && RectOf(element).Contains(p))
                {
                    Native.ReleaseCapture();
                    Native.SendMessage(Handle, 0xA1, (IntPtr)2, IntPtr.Zero);
                    return;
                }
            }
            _pressed = true;
            Invalidate();
            base.OnMouseDown(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            PointF p = new PointF(e.X / _scale, e.Y / _scale);
            _pressed = false;
            foreach (Dictionary<string, object> element in Elements())
            {
                string type = Str(element, "type");
                if (type != "button" && type != "icon-button") continue;
                if (!Flag(element, "visible", true) || !Flag(element, "enabled", true))
                    continue;
                if (!RectOf(element).Contains(p)) continue;
                Act(Str(element, "action"), Str(element, "id"));
                break;
            }
            Invalidate();
            base.OnMouseUp(e);
        }

        private void Act(string action, string id)
        {
            switch (action)
            {
                case "browse":
                    Browse();
                    break;
                case "minimize":
                    WindowState = FormWindowState.Minimized;
                    break;
                case "cancel":
                case "back":
                    if (_installing) return;
                    Close();
                    break;
                case "close":
                case "finish":
                    if (!_installing) Close();
                    break;
                case "install":
                case "start-install":
                    BeginInstall();
                    break;
                default:
                    if (id == "footer.primary" || id == "installButton") BeginInstall();
                    break;
            }
        }

        private void Browse()
        {
            using (FolderBrowserDialog dialog = new FolderBrowserDialog())
            {
                dialog.Description = "选择安装位置";
                dialog.SelectedPath = _field.Text;
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    _field.Text = dialog.SelectedPath;
            }
        }

        private void BeginInstall()
        {
            string target = _field.Text.Trim();
            if (target.Length == 0)
            {
                MessageBox.Show(this, "请先选择安装目录。", "Leaves & Stone 安装");
                return;
            }
            _installing = true;
            _progress = 0.05;
            _page = "installing";
            _hover = "";
            _field.Visible = false;
            _progressTimer.Start();
            Invalidate();

            Thread worker = new Thread(delegate ()
            {
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
                    _result = null;
                }
                catch (Exception error)
                {
                    _result = error.Message;
                }
                try
                {
                    BeginInvoke((Action)delegate
                    {
                        _progressTimer.Stop();
                        _installing = false;
                        _page = "finished";
                        Invalidate();
                    });
                }
                catch
                {
                }
            });
            worker.IsBackground = true;
            worker.Start();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;
            g.Clear(BackColor);
            g.ScaleTransform(_scale, _scale);

            foreach (Dictionary<string, object> element in Elements())
            {
                if (!Flag(element, "visible", true)) continue;
                try
                {
                    switch (Str(element, "type"))
                    {
                        case "box":
                            DrawBox(g, element);
                            break;
                        case "image":
                            DrawImage(g, element);
                            break;
                        case "text":
                            DrawText(g, element);
                            break;
                        case "input":
                            DrawInput(g, element);
                            break;
                        case "button":
                        case "icon-button":
                            DrawButton(g, element, Str(element, "type") == "icon-button");
                            break;
                        case "progress":
                            DrawProgress(g, element);
                            break;
                    }
                }
                catch
                {
                    // A malformed element must not take the whole window down.
                }
            }
            DrawFrame(g);
        }

        // GDI+ starts a new sub-path on every AddArc, so mixing arcs and lines
        // produced several disconnected figures and the outline came out as a
        // doubled, diagonally cut ring. Build one closed polygon instead.
        private static GraphicsPath Rounded(RectangleF rect, float radius,
                                            float[] corners = null)
        {
            GraphicsPath path = new GraphicsPath();
            if (rect.Width < 1 || rect.Height < 1) return path;
            float tl = corners != null ? corners[0] : radius;
            float tr = corners != null ? corners[1] : radius;
            float br = corners != null ? corners[2] : radius;
            float bl = corners != null ? corners[3] : radius;
            float limit = Math.Min(rect.Width, rect.Height) / 2f;
            tl = Math.Max(0f, Math.Min(tl, limit));
            tr = Math.Max(0f, Math.Min(tr, limit));
            br = Math.Max(0f, Math.Min(br, limit));
            bl = Math.Max(0f, Math.Min(bl, limit));
            if (tl + tr + br + bl < 0.5f)
            {
                path.AddRectangle(rect);
                return path;
            }

            float x = rect.X, y = rect.Y, w = rect.Width, h = rect.Height;
            List<PointF> points = new List<PointF>(40);
            AddCorner(points, x + tl, y + tl, tl, 180f, 270f);
            AddCorner(points, x + w - tr, y + tr, tr, 270f, 360f);
            AddCorner(points, x + w - br, y + h - br, br, 0f, 90f);
            AddCorner(points, x + bl, y + h - bl, bl, 90f, 180f);
            path.AddPolygon(points.ToArray());
            return path;
        }

        private static void AddCorner(List<PointF> points, float cx, float cy,
                                      float radius, float fromDeg, float toDeg)
        {
            if (radius < 0.5f)
            {
                points.Add(new PointF(cx, cy));
                return;
            }
            int steps = Math.Max(2, (int)Math.Ceiling(radius / 2f));
            for (int i = 0; i <= steps; ++i)
            {
                double angle = (fromDeg + (toDeg - fromDeg) * i / steps) *
                               Math.PI / 180.0;
                points.Add(new PointF(cx + radius * (float)Math.Cos(angle),
                                      cy + radius * (float)Math.Sin(angle)));
            }
        }
        private void DrawBox(Graphics g, Dictionary<string, object> element)
        {
            RectangleF rect = RectOf(element);
            if (rect.IsEmpty) return;
            float radius = (float)Num(element, "radius", 0);
            float[] corners = null;
            List<object> list = Seq(Obj(element, "cornerRadii"));
            if (list != null && list.Count == 4)
                corners = new[] { Convert.ToSingle(list[0]), Convert.ToSingle(list[1]),
                                  Convert.ToSingle(list[2]), Convert.ToSingle(list[3]) };
            using (GraphicsPath path = Rounded(rect, radius, corners))
            {
                Dictionary<string, object> gradient =
                    Obj(element, "background") as Dictionary<string, object>;
                List<object> stops = gradient != null ? Seq(Obj(gradient, "stops")) : null;
                if (stops != null && stops.Count >= 2)
                {
                    Color from = Hex(Str(Dict(stops[0]), "color"), Color.White);
                    Color to = Hex(Str(Dict(stops[stops.Count - 1]), "color"), from);
                    double angle = Num(gradient, "angle", 180);
                    bool vertical = Math.Abs(angle - 180) < 1 || Math.Abs(angle) < 1;
                    PointF start = vertical ? new PointF(rect.X, rect.Y)
                                            : new PointF(rect.X, rect.Y);
                    PointF end = vertical ? new PointF(rect.X, rect.Bottom)
                                          : new PointF(rect.Right, rect.Y);
                    using (LinearGradientBrush brush =
                               new LinearGradientBrush(start, end, from, to))
                        g.FillPath(brush, path);
                }
                else
                {
                    Color fill = Hex(Str(element, "background"), Color.Transparent);
                    if (fill.A > 0)
                    {
                        using (SolidBrush brush = new SolidBrush(fill))
                            g.FillPath(brush, path);
                    }
                }
                Dictionary<string, object> border = Dict(Obj(element, "border"));
                float width = (float)Num(border, "width", 0);
                if (width > 0)
                {
                    using (Pen pen = new Pen(Hex(Str(border, "color"), Color.Gray), width))
                        g.DrawPath(pen, path);
                }
            }
        }

        private void DrawImage(Graphics g, Dictionary<string, object> element)
        {
            string asset = Path.GetFileNameWithoutExtension(Str(element, "asset"));
            Image image;
            if (asset.Length == 0 || !_images.TryGetValue(asset, out image)) return;
            RectangleF rect = RectOf(element);
            if (rect.IsEmpty) return;
            g.DrawImage(image, rect);
        }

        private void DrawText(Graphics g, Dictionary<string, object> element)
        {
            RectangleF rect = RectOf(element);
            if (rect.IsEmpty) return;
            float size = (float)Num(element, "size", 16);
            if (size <= 0.5f) size = 16f;
            int weight = (int)Num(element, "weight", 400);
            string text = Str(element, "text");
            if (Str(element, "binding") == "directory" && _field != null)
                text = _field.Text;
            using (Font font = new Font(Str(element, "font", "Microsoft YaHei UI"), size,
                                        weight >= 600 ? FontStyle.Bold : FontStyle.Regular,
                                        GraphicsUnit.Pixel))
            using (SolidBrush brush = new SolidBrush(
                       Hex(Str(element, "color"), Color.Black)))
            using (StringFormat format = new StringFormat())
            {
                string align = Str(element, "align", "left");
                format.Alignment = align == "center" ? StringAlignment.Center
                                  : align == "right" ? StringAlignment.Far
                                                     : StringAlignment.Near;
                format.LineAlignment = Str(element, "verticalAlign", "middle") == "top"
                                           ? StringAlignment.Near
                                           : StringAlignment.Center;
                format.FormatFlags = StringFormatFlags.NoWrap;
                g.DrawString(text, font, brush, rect, format);
            }
        }

        private void DrawInput(Graphics g, Dictionary<string, object> element)
        {
            RectangleF rect = RectOf(element);
            if (rect.IsEmpty) return;
            using (GraphicsPath path = Rounded(rect, (float)Num(element, "radius", 12)))
            {
                using (SolidBrush brush = new SolidBrush(
                           Hex(Str(element, "background"), Color.White)))
                    g.FillPath(brush, path);
                Dictionary<string, object> border = Dict(Obj(element, "border"));
                float width = (float)Num(border, "width", 2);
                if (width > 0)
                {
                    using (Pen pen = new Pen(Hex(Str(border, "color"), Color.Gray), width))
                        g.DrawPath(pen, path);
                }
            }
        }

        private void DrawButton(Graphics g, Dictionary<string, object> element,
                                bool iconOnly)
        {
            RectangleF rect = RectOf(element);
            if (rect.IsEmpty) return;
            string id = Str(element, "id");
            bool enabled = Flag(element, "enabled", true);
            Dictionary<string, object> states = Dict(Obj(element, "states"));
            string state = !enabled ? "disabled"
                         : (_hover == id && _pressed) ? "pressed"
                         : _hover == id ? "hover" : "normal";
            Dictionary<string, object> look = states != null ? Dict(states[state]) : null;
            if (look == null && states != null) look = Dict(states["normal"]);

            float radius = (float)Num(element, "radius", rect.Height / 2);
            Color fill = Hex(look != null ? Str(look, "background") : "", Color.Transparent);
            string borderColor = look != null ? Str(look, "borderColor") : "";
            float borderWidth = (float)Num(Dict(Obj(element, "border")), "width", 0);
            if (borderColor.Length > 0 && borderWidth <= 0) borderWidth = 2f;

            using (GraphicsPath path = Rounded(rect, radius))
            {
                if (fill.A > 0)
                {
                    using (SolidBrush brush = new SolidBrush(fill))
                        g.FillPath(brush, path);
                }
                if (borderWidth > 0)
                {
                    using (Pen pen = new Pen(Hex(borderColor, Color.Gray), borderWidth))
                        g.DrawPath(pen, path);
                }
            }

            Color ink = Hex(look != null ? Str(look, "color") : "", Color.Black);
            if (iconOnly)
            {
                List<object> paths = Seq(Obj(element, "paths"));
                float stroke = (float)Num(element, "strokeWidth", 2);
                using (Pen pen = new Pen(ink, stroke))
                {
                    pen.StartCap = LineCap.Round;
                    pen.EndCap = LineCap.Round;
                    if (paths != null)
                    {
                        foreach (object value in paths)
                        {
                            List<object> points = Seq(value);
                            if (points == null || points.Count < 2) continue;
                            PointF[] line = new PointF[points.Count];
                            bool ok = true;
                            for (int i = 0; i < points.Count; ++i)
                            {
                                List<object> pair = Seq(points[i]);
                                if (pair == null || pair.Count < 2) { ok = false; break; }
                                line[i] = new PointF(Convert.ToSingle(pair[0]),
                                                     Convert.ToSingle(pair[1]));
                            }
                            if (ok) g.DrawLines(pen, line);
                        }
                    }
                }
                return;
            }

            using (Font font = new Font(Str(element, "font", "Microsoft YaHei UI"),
                                        (float)Num(element, "size", 20),
                                        (int)Num(element, "weight", 400) >= 600
                                            ? FontStyle.Bold : FontStyle.Regular,
                                        GraphicsUnit.Pixel))
            using (SolidBrush brush = new SolidBrush(ink))
            using (StringFormat format = new StringFormat())
            {
                format.Alignment = StringAlignment.Center;
                format.LineAlignment = StringAlignment.Center;
                g.DrawString(Str(element, "text"), font, brush, rect, format);
            }
        }

        private void DrawProgress(Graphics g, Dictionary<string, object> element)
        {
            RectangleF rect = RectOf(element);
            if (rect.IsEmpty) return;
            float radius = (float)Num(element, "radius", rect.Height / 2);
            using (GraphicsPath path = Rounded(rect, radius))
            using (SolidBrush brush = new SolidBrush(
                       Hex(Str(element, "background"), Color.LightGray)))
                g.FillPath(brush, path);
            double value = _page == "installing" ? _progress : Num(element, "value", 0);
            if (value <= 0) return;
            RectangleF bar = new RectangleF(rect.X, rect.Y,
                                            rect.Width * (float)value, rect.Height);
            using (GraphicsPath path = Rounded(bar, radius))
            using (SolidBrush brush = new SolidBrush(Hex(Str(element, "fill"), Color.Tan)))
                g.FillPath(brush, path);
        }
    }

    internal static class Native
    {
        [System.Runtime.InteropServices.DllImport("user32.dll")]
        public static extern bool ReleaseCapture();

        [System.Runtime.InteropServices.DllImport("user32.dll")]
        public static extern IntPtr SendMessage(IntPtr hWnd, int msg,
                                                IntPtr wParam, IntPtr lParam);
    }

    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            Dictionary<string, object> layout;
            using (Stream stream = Assembly.GetExecutingAssembly()
                       .GetManifestResourceStream("LayoutJson"))
            using (StreamReader reader = new StreamReader(stream))
            {
                layout = new JavaScriptSerializer { MaxJsonLength = int.MaxValue }
                    .Deserialize<Dictionary<string, object>>(reader.ReadToEnd());
            }

            if (args != null && args.Length >= 2 && args[0] == "--trace")
            {
                string log = args[1];
                Action<string> mark = delegate (string what)
                {
                    File.AppendAllText(log, what + Environment.NewLine);
                };
                try
                {
                    mark("json-parsed pages=" + (layout.ContainsKey("pages")));
                    Dictionary<string, object> pagesProbe = layout["pages"] as Dictionary<string, object>;
                    Dictionary<string, object> dirProbe = pagesProbe["directory"] as Dictionary<string, object>;
                    System.Collections.IEnumerable rawProbe = dirProbe["elements"] as System.Collections.IEnumerable;
                    int countProbe = 0;
                    foreach (object probe in rawProbe)
                    {
                        Dictionary<string, object> el = probe as Dictionary<string, object>;
                        mark("  " + countProbe + " " + el["id"] + " type=" + el["type"] +
                             " bgtype=" + (el.ContainsKey("background") ? el["background"].GetType().Name : "none"));
                        ++countProbe;
                    }
                    using (SetupWindow window = new SetupWindow(layout, "C:\\tmp"))
                    {
                        mark("window-created");
                        window.CreateControl();
                        mark("control-created");
                        using (Bitmap bitmap = new Bitmap(200, 200))
                        {
                            window.DrawToBitmap(bitmap, new Rectangle(0, 0, 200, 200));
                            mark("painted");
                        }
                    }
                    mark("done");
                }
                catch (Exception error)
                {
                    mark("exception: " + error);
                }
                return;
            }

            if (args != null && args.Length >= 3 && args[0] == "--render")
            {
                using (SetupWindow window = new SetupWindow(layout, ReadInstallDir()))
                {
                    typeof(SetupWindow)
                        .GetField("_page", BindingFlags.NonPublic | BindingFlags.Instance)
                        .SetValue(window, args[1]);
                    window.CreateControl();
                    using (Bitmap bitmap = new Bitmap(window.ClientSize.Width,
                                                      window.ClientSize.Height))
                    {
                        window.DrawToBitmap(bitmap, new Rectangle(0, 0,
                            bitmap.Width, bitmap.Height));
                        bitmap.Save(args[2], System.Drawing.Imaging.ImageFormat.Png);
                    }
                }
                return;
            }

            Application.Run(new SetupWindow(layout, ReadInstallDir()));
        }

        private static string ReadInstallDir()
        {
            try
            {
                using (Microsoft.Win32.RegistryKey key = Microsoft.Win32.Registry
                           .LocalMachine.OpenSubKey(@"Software\WOW6432Node\LeavesAndStone\IME"))
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
